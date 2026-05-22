# Large File Strategy

> Planning document for handling very large audio analysis files.
> Current implementation loads the full audio buffer and all partials into RAM.
> This document outlines the constraints and a phased plan for segmented/streaming operation.

---

## Background: How Loris Works Internally

`Loris::Analyzer::analyze()` (`third_party/loris/src/Analyzer.C`) is a **frame-by-frame STFT loop** over the complete audio buffer. At each hop it:

1. Computes the reassigned spectrum over a Kaiser-windowed region of samples
2. Selects spectral peaks
3. Feeds peaks into `PartialBuilder` — a stateful tracker that connects peaks across frames into continuous partial tracks

`PartialBuilder` is the critical constraint: it is created and destroyed inside `analyze()`, its internal state is not exposed through any API, and it requires seeing every frame in order. **Loris has no streaming or segmented analysis API.** The complete audio buffer must be present before analysis can begin.

---

## Where Memory Goes

Representative numbers for a 10-minute mono file at 48 kHz:

| Item | Approx size |
|---|---|
| Audio samples (`std::vector<double>` copy for Loris) | ~230 MB |
| Loris working buffers (spectrum, window, peaks) | ~5 MB |
| Loris `PartialList` result (5,000 partials × 3,600 bps × 40 bytes) | ~720 MB |
| Our `Project` Partial objects (same data, converted) | ~720 MB |
| **Peak RAM during analysis** | **~1.7 GB** |

A 30-minute file could exceed 5 GB total peak RAM. The partial data is the dominant cost; the audio buffer is the secondary cost.

---

## Constraints

| Goal | Possible without modifying Loris? |
|---|---|
| Reduce audio buffer memory during analysis | Yes — Strategy A |
| Reduce runtime editing memory | Yes — Strategy C |
| Reduce peak partial RAM during analysis | Yes — Strategy B (multiple Loris instances per segment) |
| True streaming analysis (feed Loris in chunks) | **No** — requires modifying `PartialBuilder` internals |

---

## Strategy A — Memory-Mapped Audio Input

**Effort**: Low  
**Memory relief**: Eliminates the ~230 MB audio buffer during analysis  
**Loris changes**: None — Loris already accepts `const double*` via `analyze(bufBegin, bufEnd, srate)`

Instead of copying the WAV into `juce::AudioBuffer<float>` and then into `std::vector<double>`, memory-map the audio file and feed Loris a pointer directly. The OS pages in only the windowed regions Loris touches — the hop-by-hop access pattern is highly cache-friendly so working set stays small.

**Implementation notes:**
- Replace `project.loadSourceAudio()` copy path with `mmap` (POSIX) / `CreateFileMapping` (Windows)
- Need a float→double conversion shim since WAV is typically 32-bit float and Loris needs `double`; can do this lazily in a thin adapter rather than materialising a full `double` buffer
- `juce::MemoryMappedAudioFormatReader` provides a cross-platform wrapper that may serve as the basis

**Files to modify:** `Source/Model/Project.cpp`, `Source/Analysis/Analyzer.cpp`

---

## Strategy B — Overlapping Segment Analysis

**Effort**: High  
**Memory relief**: Only one segment's audio and Loris state in RAM at a time; reduces audio peak from ~230 MB to ~23 MB for a 60 s segment  
**Loris changes**: None — each segment is a complete, independent Loris run

### Approach

1. Divide audio into segments (e.g., 60 s) with overlapping regions (e.g., 5 s at each boundary)
2. Run a fresh `Loris::Analyzer` on each segment sequentially; free the segment's audio buffer when done
3. After all segments complete, stitch the resulting partial lists together at each boundary

### Boundary stitching algorithm

In the overlap zone (e.g., t ∈ [55 s, 65 s] for a boundary at 60 s):

1. Collect all partials from segment N that have breakpoints after `boundary − overlap/2`
2. Collect all partials from segment N+1 that have breakpoints before `boundary + overlap/2`
3. For each "open" partial from segment N, find the closest-frequency partial in segment N+1 within a frequency tolerance (e.g., ±`freqResolutionHz`)
4. For matched pairs: concatenate breakpoints, removing duplicates from the overlap region (keep segment N's breakpoints up to the boundary, segment N+1's breakpoints from the boundary onward)
5. Unmatched partials from either side terminate naturally at the boundary

### Risk factors

- **Dense spectra**: many partials at similar frequencies in the overlap zone make matching ambiguous; a greedy frequency-proximity match can produce incorrect connections
- **Frequency drift across boundary**: if a partial changes frequency rapidly near a boundary, the match may fail even with a generous tolerance
- **Segment boundary artefacts**: partials that begin or end at the segment edge (due to the analysis window edge effects) may be spurious

### Mitigation

- Use a generous overlap (5–10 s) to give partial tracks time to establish stable frequency trajectories before the boundary
- Apply a quality heuristic: only stitch if the frequency delta is < half the hop's expected drift AND both partials have at least 3 breakpoints on each side of the boundary

**New files:** `Source/Analysis/SegmentedAnalyzer.{h,cpp}`, `Source/Analysis/PartialStitcher.{h,cpp}`  
**Modified files:** `Source/Analysis/Analyzer.h`, `Source/UI/MainComponent.cpp`

---

## Strategy C — Disk-Backed Partial Store

**Effort**: Medium  
**Memory relief**: Solves the runtime editing memory problem regardless of file length; analysis-phase RAM is unchanged  
**Loris changes**: None

### Approach

After analysis completes, serialize all partials to a compact binary temp file with a time-range index. `Project` holds only the index directory in RAM. `PartialView` requests partials by time range; the store loads matching partials from disk and evicts others as the viewport changes.

### Binary store format

```
Header:   magic (4 bytes) | version (2) | partial_count (4) | sample_rate (8)
Index:    partial_count × { partial_id (4) | time_start (8) | time_end (8) | file_offset (8) | breakpoint_count (4) }
Data:     per partial — colour (4) | breakpoints (count × { time(8) freq(4) amp(4) phase(4) bw(4) })
```

Index is sorted by `time_start` to support efficient range queries (binary search for viewport start, sequential scan for end).

### Viewport-aware loading

- `PartialView` exposes `setVisibleTimeRange()` (already exists for zoom/pan)
- On range change, `Project::getPartialsInRange(t0, t1)` queries the index, loads missing partials from disk, and evicts partials fully outside the range
- Eviction uses a simple LRU or "outside viewport × 2" policy
- Audio thread constraint: eviction must happen between audio blocks (brief `CriticalSection`), same as existing delete operations

### Load latency

At 48 kHz with 5 ms hop, a 60 s window contains at most ~12,000 frames. With 5,000 partials and average 3,600 breakpoints, loading the visible window (partials active in 60 s ÷ total file duration) is typically < 50 MB — fast enough for smooth pan/zoom with a 100–200 ms prefetch window.

**New files:** `Source/Model/PartialStore.{h,cpp}`  
**Modified files:** `Source/Model/Project.{h,cpp}`, `Source/UI/PartialView.h`

---

## Recommended Phased Implementation

| Phase | Strategy | Prerequisite | Expected effort |
|---|---|---|---|
| 1 | A — Memory-mapped audio | None | 1–2 days |
| 2 | C — Disk-backed partials | None (independent of A) | 3–5 days |
| 3 | B — Segmented analysis | Phase 1 (audio streaming) | 1–2 weeks |

Phases 1 and 2 are independent and can be done in either order. Phase 3 builds on Phase 1's audio-reading infrastructure. Implementing Phases 1 and 2 together would handle the practical case of files up to ~15 minutes on a 8 GB machine. Phase 3 is required only for files where even a single Loris pass exceeds available RAM.

---

## Decision Criteria for "When Is This Needed?"

A rough threshold for when the current implementation becomes problematic:

- **File length > 5 min at 48 kHz**: audio buffer + partial data likely exceeds 1 GB
- **Partial count > 10,000**: working set during editing exceeds comfortable runtime RAM
- **Target machine RAM < 8 GB**: any file over ~2 min may strain available memory

At v0.1, the primary use case is short analysis targets (30 s – 3 min percussion/voice). These fit comfortably within current architecture. Revisit when users report OOM conditions or analysis stalls on longer material.