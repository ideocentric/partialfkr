# PartialEditor — Design Document

> Detailed design rationale and reference. Read `CLAUDE.md` first for the at-a-glance summary; this document expands on the *why* behind each decision and provides the math, algorithms, and rejected alternatives.

## 1. Background and Motivation

This project is a sinusoidal modeling and resynthesis tool inspired by Michael Klingbeil's SPEAR (Sinusoidal Partial Editing Analysis and Resynthesis). SPEAR was the canonical macOS application for analyzing audio into a cloud of frequency/amplitude tracks, editing them interactively, and resynthesizing or exporting. It is no longer maintained, was never open-sourced, and has stopped running reliably on current macOS.

Predecessor approaches the project draws on:

- **Csound's `hetro` analysis / `adsyn` opcode** — heterodyne filtering against a known fundamental. Produces extremely compact harmonic-amplitude data; ~20 partials can carry recognizable speech. Limited to monophonic harmonic sources.
- **McAulay-Quatieri sinusoidal model (1986)** — peak-pick the STFT magnitude in each frame, link peaks across frames into partial tracks with birth/death/continuation logic, interpolate amplitude linearly and phase cubically. Handles polyphony and inharmonicity. The basis of SPEAR's analyzer.
- **Loris's reassigned bandwidth-enhanced additive model (Fitz & Haken, late 1990s)** — extends MQ with time-frequency reassignment (sharper localization) and a per-partial bandwidth envelope (carries noise energy nearby). Single component type for both sine and noise.

### The Artistic Workflow

PartialEditor exists to support this loop:

1. Analyze a source audio recording (often speech, solo voice, or a single instrumental tone) into sinusoidal partials.
2. **Qualitative reduction**: interactively remove or mute partials, listening continuously, to find the minimum set that still conveys recognizable source identity. The goal is not fidelity — it is characteristic preservation under aggressive data reduction.
3. Use the reduced partial set as a control signal for re-orchestration: drive sample-based string instruments via MIDI, generate csound scores that replace sines with instrumental samples, etc.
4. The reduced signal acts as a "crude manual convolution" of the source spectrum through whatever target timbre receives the partial data.

The end aesthetic: hearing the implied original signal — its onsets, its phonetic gestures, its melodic shape — refracted through the timbre of an unrelated instrumental ensemble. Speech transformed into bowed strings without losing its speech-ness.

This is why **onsets are sacred** in the export specification. Recognizability survives reduction in proportion to onset preservation.

## 2. Why Sinusoidal Modeling

The alternatives considered and rejected for the analysis core:

| Method | Strengths | Weaknesses |
|---|---|---|
| Heterodyne (csound `hetro`) | Compact harmonic data, intelligible with <30 partials, low CPU | Requires known f₀, fails on polyphony, fails on noisy material |
| McAulay-Quatieri (MQ) | Handles polyphony and inharmonicity, well-documented | Jittery partials in noisy regions, onset smearing under long windows |
| Reassigned bandwidth-enhanced (Loris) | Sharper time-frequency localization, single component for sine+noise, better onset preservation than vanilla MQ | Bandwidth field doesn't map naturally to "pure sine" export targets (we ignore it for now) |
| SMS — sinusoids + stochastic residual (Serra) | Clean residual model, preserves "air" | Two-stream architecture complicates the editing UI for reduction |
| High-resolution methods (ESPRIT, MUSIC, matching pursuit) | Sub-bin frequency resolution | Computationally heavy, marginal benefit for our use case |

**We use Loris's reassigned bandwidth-enhanced analyzer** because:

1. It is the most mature open-source C++ implementation handling a wide range of source material without per-file parameter tuning.
2. Reassignment improves onset localization, which is critical for our workflow.
3. The bandwidth field can be safely ignored when resynthesizing as pure sines — the partial frequencies and amplitudes remain correct.
4. The data model (list of `Partial` containing `Breakpoint` records) is identical to what an MQ analyzer would produce, so a future swap to a custom MQ implementation is a localized change in `Analyzer.cpp`.

**Caveat:** SPEAR uses an MQ variant. Loris produces somewhat different partial layouts on noisy material — generally fewer partials with more noise energy folded into bandwidth envelopes (which we ignore). Users coming from SPEAR will need to adjust expectations for the analyzer's defaults. An empirical comparison against known SPEAR analyses is part of the first-build milestone.

## 3. Data Model

```cpp
struct Breakpoint {
    double time;        // seconds, in source timebase
    float frequency;    // Hz
    float amplitude;    // linear, 0.0 to 1.0+ (no enforced ceiling)
    float phase;        // radians, may be 0 if phase tracking off
    float bandwidth;    // Loris bandwidth, 0.0 = pure sine
                        // currently ignored at synthesis
};

struct Partial {
    uint32_t id;                                // stable identity across edits
    std::vector<Breakpoint> breakpoints;        // sorted by time, >= 2 entries
    std::atomic<bool> muted{false};             // audio thread reads
    std::atomic<bool> soloed{false};
    juce::Colour displayColour;                 // assigned at creation, stable

    double startTime() const { return breakpoints.front().time; }
    double endTime() const { return breakpoints.back().time; }
    bool containsTime(double t) const {
        return t >= startTime() && t <= endTime();
    }
};

class Project {
    std::vector<std::unique_ptr<Partial>> partials;
    juce::AudioBuffer<float> sourceAudio;
    double sampleRate = 0;
    juce::File sourceFile;
    juce::String projectName;

    // edit history, selection state, etc. (see Source/Model/)
};
```

Design notes:

- `id` is assigned monotonically at creation and never reused. It is the only safe long-lived reference to a partial across edits.
- Breakpoints stored explicitly rather than interpolated on demand. With Loris's typical 5 ms hop, this is the right tradeoff between memory and computation.
- `muted` and `soloed` are `std::atomic<bool>` so the audio thread observes them without locks. `std::memory_order_relaxed` is sufficient — we don't need synchronization with other memory operations.
- `Partial` does not own display state beyond colour. Selection lives in a separate `Selection` object keyed by partial ID.
- The `bandwidth` field is stored for round-trip fidelity (SDIF export should preserve it) but not consumed by `PartialSynth`.

## 4. The Reduction Workflow

This is the application's defining UX. It must always feel instant — the user is making aesthetic decisions in real time.

```cpp
class ReductionController {
public:
    // Keep only the top N loudest partials at each moment in time.
    // "Loudest" computed from peak amplitude across breakpoints overlapping
    // that time.
    void applyAmplitudeRank(int keepCount);

    // Mute partials shorter than the given duration in seconds.
    void applyDurationThreshold(double minSeconds);

    // Mute partials whose peak amplitude is below threshold.
    void applyAmplitudeThreshold(float minLinearAmplitude);

    // Mute partials whose median frequency falls outside [lowHz, highHz].
    void applyFrequencyBand(float lowHz, float highHz);

    // Mute partials whose total energy (sum of amplitude * duration) is
    // below threshold. Catches very long but very quiet partials that
    // amplitude-peak alone misses.
    void applyEnergyThreshold(float minEnergy);

    // Clear all reduction filters. Selection-based mutes are preserved.
    void clearReductions();

private:
    void recomputeMuteMask();
};
```

Each control connects to a JUCE `Slider`. As the slider moves, `recomputeMuteMask` runs on the message thread, computes the new mute states, and writes them via the atomic flags. The audio thread observes the change at the next block boundary (~5-10 ms latency). With playback looping a representative phrase, the user hears the result of each parameter change in real time.

### Two Mute Categories

**Reduction-mutes** (set by `ReductionController`) and **selection-mutes** (set by clicking partials in the UI) are conceptually separate but stored in the same `std::atomic<bool> muted` field. Internally, the controller maintains a separate "reduction reason" mask; selection-mutes are merged in at flag-write time. Toggling one type doesn't override the other — clearing reductions doesn't un-mute manually-muted partials, and vice versa.

The synthesizer treats any mute reason identically: do not synthesize.

### Solo Behavior

If any partial has `soloed == true`, `PartialSynth::hasSolo` is set, and only soloed partials are synthesized. This is the standard "if anything's soloed, listen to only those" convention.

## 5. Real-Time Synthesizer

`PartialSynth` is an oscillator bank running on the audio thread.

```cpp
class PartialSynth {
public:
    void prepareToPlay(double sampleRate, int blockSize);
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample, int numSamples);
    void setPlayheadTime(double seconds);
    void setProject(Project* project);
    void setSolo(bool anySoloed) { hasSolo.store(anySoloed); }

private:
    Project* project = nullptr;
    double sampleRate = 0;
    double playheadSeconds = 0;
    std::atomic<bool> hasSolo{false};

    struct PartialState {
        size_t bpIndex;     // index of current breakpoint pair
        double phase;       // running phase accumulator
    };
    std::vector<PartialState> states;

    void synthesizePartial(const Partial& p, PartialState& state,
                          float* out, int numSamples);
};
```

Per audio block:

1. Determine time range `[t_start, t_end]` from `playheadSeconds` and `numSamples / sampleRate`.
2. For each partial in `project->partials`:
   - Skip if `muted.load(std::memory_order_relaxed)`.
   - If `hasSolo.load()`, skip unless `soloed.load()`.
   - Skip if `[partial.startTime(), partial.endTime()]` doesn't overlap `[t_start, t_end]`.
3. For each sample of each active partial:
   - Find the bracketing breakpoint pair (cached in `PartialState::bpIndex`, advance as needed).
   - Linearly interpolate amplitude in linear space.
   - Linearly interpolate frequency in *cents* (equivalent to exponential in Hz; more perceptually natural).
   - Advance phase: `phase += 2π * freq / sampleRate`, wrap to `[-π, π]`.
   - Compute `out[i] += amp * std::sin(phase)`.

Phase continuity matters across blocks. `PartialState::phase` persists between `renderNextBlock` calls. When the playhead jumps (user clicks elsewhere on the timeline), `setPlayheadTime` resets all states; phase re-initializes from `breakpoint.phase` if available, otherwise zero.

### Performance Budget

256-sample block at 48 kHz = 5.33 ms. Synthesizing 500 partials with the above per-sample loop is comfortably in budget on Apple Silicon. At ~5000 partials, expect to need:

- Vectorization (process 4-8 samples at a time using `juce::dsp::SIMDRegister` or platform intrinsics).
- A sine lookup table with linear interpolation (faster than `std::sin` though `std::sin` is surprisingly competitive on modern hardware).
- Filtering out inaudibly-quiet partials before the per-sample loop.

Optimization is *not* a first-build concern. Get correctness first, profile, then optimize.

## 6. MIDI Export Specification (Detailed)

### Pre-Pass

Before generating MIDI, scan all partials to gather statistics:

```
For each partial P with breakpoints B[0..n-1]:
    nominal_freq = median(B[i].frequency for i in 0..n-1)
    nominal_midi = 69 + 12 * log2(nominal_freq / 440)
    nominal_pitch = round(nominal_midi)        // integer MIDI note
    nominal_anchored_freq = 440 * 2^((nominal_pitch - 69) / 12)

    For each breakpoint B[i]:
        deviation_cents = 1200 * log2(B[i].frequency / nominal_anchored_freq)
        track max(abs(deviation_cents)) for this partial
        track max(abs(deviation_cents)) globally

Present histogram of per-partial max deviations to user.
User picks bend range R from {2, 12, 24, 48} semitones.
Compute count of partials exceeding R*100 cents.
If any exceed and splitOnSaturation == false: warn user, offer to raise R.
```

The histogram is displayed in the export dialog as a horizontal bar chart with bins at semitone boundaries. The four R options are shown as vertical lines; the user can see exactly how many partials would saturate at each choice.

### MIDI Generation

- **PPQ**: 960 ticks per quarter note (high resolution for accurate breakpoint timing).
- **Tempo**: 120 BPM default (1 second = 2 quarter notes = 1920 ticks). User-overridable in export dialog.
- **Channel allocation**: MPE mode uses channels 2-16 in rotation. Channel 1 reserved as MPE manager channel (configuration messages only). Non-MPE mode puts everything on channel 1.

For each partial in order of start time:

1. Assign next MPE channel (round-robin, with hand-off on note-off).
2. At first breakpoint time:
   - If channel is fresh: emit RPN setup → bend range, then RPN reset (RPN 127,127).
   - Emit Note On: pitch = `nominal_pitch`, velocity = `amp_to_velocity(B[0].amplitude)`.
3. For each subsequent breakpoint:
   - Emit pitch bend: 14-bit value = `clamp((deviation_cents / (R * 100)) * 8192 + 8192, 0, 16383)`.
   - Emit CC11: `amp_to_velocity(B[i].amplitude)` (same mapping, reused for expression).
4. At last breakpoint time: emit Note Off.

### Amplitude Mapping

```
def amp_to_velocity(linear_amp, amp_floor=1e-4, amp_ceil=1.0):
    if linear_amp <= amp_floor:
        return 1
    db = 20 * log10(linear_amp)
    db_floor = -80     # 20 * log10(1e-4)
    db_ceil = 0
    normalized = (db - db_floor) / (db_ceil - db_floor)
    return clamp(round(normalized * 126) + 1, 1, 127)
```

The `amp_floor` and `amp_ceil` parameters are exposed in the export dialog for fine-tuning per source material. Default −80 dB floor and 0 dB ceiling work for most material.

### Non-MPE Fallback

If user disables MPE: all partials on channel 1, single bend range applied globally. Only suitable for monophonic resynthesis or temporally non-overlapping partials. Useful for synth targets that don't speak MPE (some sampler libraries, some hardware synths).

When this mode is selected, the pre-pass also flags partial *overlaps* — any two partials active at the same time will collide on channel 1's single bend stream. The dialog shows an overlap count; user can decide whether to proceed.

### Why No Re-Articulation

The `splitOnSaturation` flag is `false` by default and the workflow strongly discourages turning it on. Re-articulation (re-triggering a partial when its bend would saturate) creates new onset events that are not present in the source. For source material where onsets carry recognition information (speech, percussive instruments), introducing fake onsets corrupts the data more than the alternative (a saturated bend on rare extreme glides).

If the user wants the re-articulation aesthetic deliberately, they enable it. The default behavior preserves source onset structure.

## 7. Csound Export Specification

Two output modes:

### Mode A: Score with GEN02 Tables

Emit one f-statement per partial, defining frequency and amplitude tables, then one i-statement per partial referencing those tables. Suitable for files with many breakpoints per partial.

```csound
; auto-generated by PartialEditor
f1001 0 1024 -2  <time0> <freq0> <time1> <freq1> ...   ; partial 0 freq
f1002 0 1024 -2  <time0> <amp0>  <time1> <amp1>  ...   ; partial 0 amp
f1003 0 1024 -2  ...                                   ; partial 1 freq
; ...

i1 <start_0> <duration_0> 1001 1002    ; partial 0: read tables 1001, 1002
i1 <start_1> <duration_1> 1003 1004    ; partial 1
; ...
```

A template orchestra is provided that defines `instr 1` to read amplitude and frequency from the named tables and oscillate.

### Mode B: Score with Inline linseg

Each partial gets one i-statement with breakpoint p-fields:

```csound
i1 <start_time> <duration> <num_segments> <amp0> <freq0> <dur0> <amp1> <freq1> <dur1> ...
```

The orchestra reconstructs `linseg` envelopes from p-fields. Limited by csound's practical p-field count (~30) but readable for files with sparse breakpoints.

User selects mode in export dialog. Default: Mode A if average breakpoints/partial > 15, otherwise Mode B.

### Instrument Replacement

The csound orchestra is a separately-editable template (saved in `templates/csound/`). The default template uses `oscili` with a sine table, producing pure sinusoidal resynthesis. The user is expected to edit the template to replace the sine generator with their instrument of choice (a sampler reading instrument samples, a physical model, etc.). This is the core re-orchestration mechanism for csound output.

## 8. SDIF Export

Use Loris's built-in SDIF export. Frame type `1TRC` (sinusoidal track) is the standard. Frame type `RBEP` (Reassigned Bandwidth-Enhanced Partial, Loris's own) preserves the bandwidth field for round-trip with other Loris-aware tools.

User selects frame type in export dialog. Default: `1TRC` (broader compatibility).

This format interchanges with Max/MSP (via CNMAT's `sinusoids~`), OpenMusic, Audiosculpt, and any IRCAM-derived tool.

## 9. JSON Export

For ease of consumption by any external scripting target.

```json
{
  "schema_version": "1.0",
  "source_file": "voice_sample.wav",
  "sample_rate": 48000,
  "duration_seconds": 3.42,
  "partial_count": 47,
  "partials": [
    {
      "id": 0,
      "breakpoints": [
        {"t": 0.000, "f": 247.3, "a": 0.012, "phase": 0.0, "bw": 0.0},
        {"t": 0.005, "f": 248.1, "a": 0.018, "phase": 0.0, "bw": 0.0}
      ]
    }
  ]
}
```

Schema versioned at the top level. The export dialog has a checkbox to include phase and bandwidth (off by default — most consumers don't need them).

## 10. Future Considerations (Deliberately Out of Scope)

Listed so we don't accidentally drift toward them:

- **Sound morphing** between two analyses. Loris supports it natively; the UI for picking partial correspondences is non-trivial.
- **Bandwidth-enhanced resynthesis** (re-introducing noise via Loris's `Synthesizer`). The data is preserved; the synth doesn't consume it yet.
- **Real-time analysis** (streaming input). Currently offline only.
- **VST/AU plugin wrapping.** The application architecture is plugin-amenable but the editing workflow doesn't fit a plugin host's expectations.
- **Mac App Store distribution.** Notarization is doable; App Store sandboxing would conflict with arbitrary file access patterns we may need for templates and exports.
- **Cross-platform builds.** The CMake structure preserves this option; we don't actively test on Linux/Windows until macOS is stable.
- **Spectrogram overlay** behind the partial view. Useful for tuning analysis parameters; not core workflow.
- **Pitch-class quantization on export** (snap partials to a scale before MIDI/csound emission). Interesting but a separate aesthetic concern from the reduction workflow.

## 11. Build Notes

### Loris Vendoring

Clone `musictheory/loris` into `third_party/loris/`. The project's top-level `CMakeLists.txt` builds Loris's C++ source files as a static library `loris_static`. Excluded:

- `src/loris.i` and SWIG-generated Python bindings
- `utils/` command-line tools
- `test/` Loris's own tests
- FFTW dependency (we use `juce::dsp::FFT`, so Loris should be configured with `--without-fftw` equivalent — set the `HAVE_FFTW3_H` macro to 0 or provide a stub)

If Loris's internal use of FFTW proves hard to stub, alternative: build with FFTW vendored as a separate static lib. Adds another dependency but is straightforward.

### JUCE Vendoring

```cmake
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4    # pin to a specific tag
)
FetchContent_MakeAvailable(JUCE)
```

JUCE 8 is CMake-first and self-configures cleanly.

### macOS Deployment

Deployment target: macOS 11.0 (Big Sur). Practical minimum for current Xcode and modern C++20 standard library features.

Universal binary for distribution:
```cmake
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
```

Code signing and notarization deferred until first binary distribution. See `docs/SIGNING.md` (to be written) when that milestone arrives.

## 12. Open Questions Deferred to First Working Build

These are deliberate non-decisions that will be answered empirically:

- **Default `Analyzer` parameters** (frequency resolution, hop time, amplitude floor). Will be tuned against known SPEAR analyses of the same source files for sanity comparison.
- **OpenGL vs Metal** for the partial view. JUCE 8's `OpenGLContext` works on macOS, but Metal is the future. Start with OpenGL; switch if performance demands or JUCE deprecates GL on macOS.
- **Stereo source handling.** Analyze each channel separately and merge? Mid/side decomposition? Sum to mono? Initial answer: sum to mono with a warning displayed; channel-wise analysis is a future option.
- **Project file format.** Native serialization of the `Project` object for save/load. Likely JSON or a JUCE `ValueTree` XML for human-readability. Locked in before the first save/load milestone.
- **Undo granularity.** Per-edit (each click is one undo step) vs grouped (a slider drag is one step). Likely grouped for slider gestures, per-click for everything else.
