# partialfkr — Feature Roadmap

> Living document. Features are ordered by development priority. Status: `[ ]` not started · `[~]` in progress · `[x]` complete.

---

## Completed

- [x] Loris sinusoidal analysis (WAV → partial tracks)
- [x] 2D partial canvas (frequency × time, green brightness + line weight by amplitude)
- [x] Basic playback (oscillator bank, linear Hz interpolation, correct phase model)
- [x] Transport bar (play / stop / seek slider + time display)
- [x] Horizontal time-axis zoom (scroll wheel, anchored to cursor)
- [x] Vertical frequency-axis zoom (shift + scroll, pinned to 0 Hz)
- [x] Horizontal scrollbar (active when zoomed in, disabled at full view)
- [x] Open file + filename display in right panel
- [x] Master output gain (-20 dB default)

---

## Planned Features

### 0. Graphics Abstraction Layer
**Priority: High — do alongside Feature #1**

Extract all drawing from `PartialView::paint()` into a `PartialRenderer` interface owned by `PartialView` via `unique_ptr`. `PartialView` calls `renderer->render(state)` and knows nothing about the backend.

```cpp
struct RenderState {
    // partials, view transform, playhead, selection — no JUCE rendering types
};

class PartialRenderer {
public:
    virtual ~PartialRenderer() = default;
    virtual void render(const RenderState& state) = 0;
};
```

Initial implementation: `Juce2DRenderer` (current CoreGraphics/Direct2D/Cairo path).
Future implementations: `MetalRenderer` (macOS), `OpenGLRenderer` (Windows/Linux).
Swapping backend is a one-liner in `PartialView`. No rendering types leak through the interface.

---

### 1. Time Ruler
**Priority: High — visual foundation for all time-based operations**

A time axis drawn along the bottom of the `PartialView` canvas (above the scrollbar). Tick marks and labels adapt to the current zoom level:

- Finest useful increment: 5 ms (Loris hop time)
- Coarser increments selected automatically so labels never overlap (10 ms → 50 ms → 100 ms → 500 ms → 1 s etc.)
- Minor ticks (unlabelled) subdivide between major ticks
- Playhead position reflected on the ruler

---

### 2. Partial Selection
**Priority: High — gates most editing features below**

- **Click** a partial track to select it; deselects all others
- **Cmd-click** adds to / removes from selection (multi-select)
- **Click on empty background** clears selection
- Selected partials draw in a distinct highlight colour (TBD — currently white at 1.5 px)
- **Playback behaviour:** when one or more partials are selected, only those partials synthesize (implicit solo). Deselecting all returns to full playback.

---

### 3. Partial Edit Operations (Cut / Copy / Paste / Delete)
**Priority: High — depends on #2**

Operate on the current partial selection:

| Operation | Behaviour |
|-----------|-----------|
| **Cut** (⌘X) | Remove selected partials from project; place on clipboard. Undoable. |
| **Copy** (⌘C) | Copy selected partials to clipboard; project unchanged. |
| **Delete** (⌫) | Remove selected partials; no clipboard. Undoable. |
| **Paste** (⌘V) | Place clipboard partials into project. User clicks a time point first; all pasted partial times are offset so the earliest starts at the clicked time. |

Clipboard holds a snapshot of full `Partial` objects (breakpoints, amplitude, frequency). IDs are reassigned on paste to avoid collisions.

---

### 4. Undo / Redo ✅
**Fully implemented.** `EditHistory` wraps `juce::UndoManager` (30 MB / 200 action limit).
Cmd+Z / Cmd+Shift+Z registered via `ApplicationCommandManager`. Menu descriptions update
dynamically. Currently undoable: delete partials, paste, stretch, breakpoint delete, bridge,
crossfade, fade in/out, scale amplitude.

**Three remaining gaps (non-blocking):**
- **Drag transaction grouping** — `beginTransaction()` on mouseDown needed when breakpoint
  drag editing (Feature #7) is implemented, so a full drag gesture is one undo step.
  Hooks exist in `EditHistory`; wire-up is part of Feature #7.
- **Dirty flag on undo-to-saved-state** — window title stays dirty even after undoing back
  to the last-saved version. Minor UX issue.
- **History clear on re-analysis** — old actions remain on the stack after loading new audio;
  they fail gracefully but should be cleared explicitly on `Project::analyze()`.

---

### 5. Time-Range Selection
**Priority: Medium — enables segment-level editing**

A second selection mode triggered by clicking and dragging on the **canvas background** (not on a partial):

- Renders as a dark grey semi-transparent rectangle over the canvas while dragging, and held when released
- Captures the time span `[t_start, t_end]` and operates on **all partial tracks** within that span
- Clears when user clicks outside the rectangle or starts a new drag

**Operations on a time-range selection:**

| Operation | Behaviour |
|-----------|-----------|
| **Delete** | Zeros out content in the range; gap remains (duration unchanged, silence in range) |
| **Cut** | Removes content in range, closes the gap — everything after shifts left by range duration |
| **Copy** | Copies all breakpoint data within the range to clipboard |
| **Paste** | See paste semantics below |

**Paste semantics into a time range:**

The clipboard duration `C` and the selected range duration `R` determine what happens to trailing content:

- `C < R`: paste fills from range start; trailing content shifts **left** by `(R - C)` — net duration shrinks
- `C = R`: straight replacement
- `C > R`: trailing content shifts **right** by `(C - R)` to make room — net duration grows

---

### 6. Time-Range Stretch / Compress
**Priority: Medium — depends on #5**

With a time-range selection active, the user can rescale the selected segment's duration:

- Stretch (extend): breakpoints within the range have their times scaled to the new duration; linear interpolation recalculates values at the Loris 5 ms hop interval to produce evenly-sampled output. Content outside the range shifts right to stay contiguous.
- Compress (shrink): same time rescaling, but breakpoints that fall outside the compressed range are **discarded** — a destructive operation.

**Design decision needed before implementation:** how to handle the destructive shrink case.
- Option A: warn the user with a confirmation dialog
- Option B: provide a non-destructive "preview shrink" that only commits on explicit confirm
- Option C: allow it freely and rely on undo (simple, consistent with other DAWs)

Likely: Option C with a clear undo step, but this should be decided before building.

---

### 7. Breakpoint Drag Editing (Direct Select mode)
**Priority: Medium**

In Direct Select mode, selected breakpoints (single or multi) can be dragged to modify
their time position and/or frequency. A "segment" is any contiguous run of selected
breakpoints within a single partial.

#### Drag axes

| Drag | Behaviour |
|------|-----------|
| Free drag (no modifier) | Moves in both time and frequency simultaneously |
| **Shift** + drag | Locks to dominant axis — horizontal if initial movement is wider than tall, vertical otherwise (Adobe-style) |

#### Horizontal drag — time shift

All selected breakpoints on a partial move by the same time delta (rigid block). Their
frequency values are preserved. The partial track interpolates naturally between the
moved selection and its non-selected neighbors.

**Hard constraint per partial (5 ms Loris hop):**

```
left_limit  = time(nearest non-selected BP to the left)  + 5 ms
right_limit = time(nearest non-selected BP to the right) - 5 ms - selection_duration
```

where `selection_duration = time(last selected BP) - time(first selected BP)`.

For a single selected breakpoint `selection_duration = 0`, so:

```
left_limit  = time(prev BP) + 5 ms
right_limit = time(next BP) - 5 ms
```

When breakpoints from multiple partials are selected, each partial enforces its own
constraint independently. The visual indicator reflects the most restrictive limit
across all selected partials.

**Example:**

```
Partial breakpoints:  10 ms | [50 ms  60 ms  70 ms] | 150 ms
                      ^fixed   ^————selection————^     ^fixed

left_limit  = 10 + 5 = 15 ms  →  max left shift  = 35 ms
right_limit = 150 - 5 - 20 = 125 ms  →  max right shift = 75 ms

Released at 15 ms:  10 ms | [15 ms  25 ms  35 ms] | 150 ms
  • 10 ms → 15 ms segment: interpolated from 10 ms freq to (former 50 ms) freq
  • 35 ms → 150 ms segment: interpolated from (former 70 ms) freq to 150 ms freq
```

#### Vertical drag — frequency shift

A uniform additive Hz delta is applied to all selected breakpoints, preserving the
relative frequency shape within the selection. No time movement occurs. Frequencies
are clamped to > 0 Hz. The partial track interpolates naturally to non-selected
neighbors on both sides.

#### Constraint visualization

When a horizontal drag begins, faint vertical lines appear at the computed left and
right time limits (most restrictive across all selected partials). The lines persist
for the duration of the drag and disappear on mouse release. Style: 1 px, translucent
white, drawn above the partial canvas layer.

#### Commit and undo

On release the move is committed as a single undoable `MoveBreakpointsAction` that
records `{ breakpoint_id → (original_time, original_freq) }` and
`{ breakpoint_id → (new_time, new_freq) }` for every moved breakpoint. Undo restores
all original positions and frequencies in one step.

#### Implementation notes

- Drag state (delta, axis lock, constraint limits) lives in `PartialView` as transient
  member state, cleared on `mouseUp`.
- Constraint lines are drawn in `PartialView::paint()` only when drag state is active,
  using the current view transform to convert ms → pixel positions.
- The undoable action class: `Source/Model/MoveBreakpointsAction.{h,cpp}`.

---

### 8. Temporal Scrubbing
**Priority: Medium — performance/exploration tool**

Holding a modifier key (TBD — e.g., Option) while moving the cursor over the canvas activates scrub mode:

- Playback stops; the synth freezes at the cursor's current time position
- All active partials at that moment synthesize continuously at their instantaneous frequency and amplitude — a held "snapshot" of the spectrum
- As the cursor moves left/right, the snapshot updates with a short **slew time** (5–10 ms smoothing on frequency and amplitude) to prevent audible discontinuities from frame-to-frame jumps
- Releasing the modifier returns to normal transport state

---

### 9. Multi-Document Cross-File Paste
**Priority: Low — requires multi-document architecture and menu bar first**

With two or more analyzed files open simultaneously, the clipboard should be document-agnostic — partials or time-range content copied from one document can be pasted into another.

**Prerequisites:**
- Menu bar with Window menu for document management
- Multi-document support (each open file gets its own `Project`, `PartialView`, and `PartialSynth`)
- A shared application-level clipboard (not per-document) so copy from Document A is visible to Document B

**Behaviour:**
- Copy a partial selection or time-range selection from any open document — these go to the shared clipboard with their breakpoint data and timing intact
- Switch to another document and paste — the paste semantics are identical to single-document paste (Feature #3 / #5), offset to the clicked time point
- The pasted partials get new IDs in the destination document; no link back to the source

**Design note:** The clipboard format is already self-contained (`Partial` objects with breakpoints) so no structural changes are needed to support cross-document paste — it's purely an architecture question of where the clipboard lives (application-level singleton vs. per-document).

---

## Export Features (from CLAUDE.md spec)

All exporters have stub implementations in `Source/Export/`. These need UI wiring and full implementation:

- [ ] **MIDI / MPE export** — one note per partial, 14-bit pitch bend, CC11 expression, bend-range histogram dialog
- [ ] **Csound score export** — Mode A (GEN02 tables) and Mode B (inline linseg), optional orchestra template
- [ ] **SDIF export** — 1TRC or RBEP frame type, via Loris's `SdifFile`
- [ ] **JSON export** — versioned schema, optional phase/bandwidth fields

### Exporters requiring review

**SuperCollider (.scd) — performance review pending**
Initial user feedback indicates the generated `.scd` files are very resource-intensive:
SC struggles to process large partial counts (many `SinOsc` synths + `BufRd` envelopes
running simultaneously). Similar resource pressure was encountered with the Csound exporter
and resolved by refining the orchestra structure.

Awaiting further user feedback before implementing changes. Likely directions:
- Server resource hints at the top of the generated file (`s.options.memSize`,
  `s.options.numWireBufs`, `s.options.maxNodes`) scaled to the partial count
- Optionally group partials into buses or use `Mix.ar` to reduce synth overhead
- Consider a reduced-fidelity mode that merges low-amplitude partials

> **Do not implement yet** — hold for more field reports to understand whether the
> bottleneck is memory, node count, CPU, or wire buffer exhaustion.

---

## Open Design Decisions

| # | Decision | Context |
|---|----------|---------|
| 1 | Destructive shrink handling | Time-range compress (Feature #6) — warn, preview, or rely on undo? |
| 2 | Point-edit mode entry/exit | Breakpoint selection (Feature #7) — modifier key, double-click, toolbar toggle? |
| 3 | Scrub modifier key | Temporal scrubbing (Feature #8) — Option, Space+drag, dedicated mode button? |
| 4 | SC exporter optimization | Resource-intensive output reported by users — awaiting more field reports before deciding approach (server options, bus grouping, partial merging). See Export § "Exporters requiring review". |
| 4 | Clipboard scope | Application-level singleton (required for cross-document paste, Feature #9). Persists across file loads within a session; does not need to survive app restart. |
| 5 | Selection highlight colour | Currently white; should differ from muted/soloed state colours |
| 6 | Output gain UI | Master gain is in the synth but has no knob in the UI yet |