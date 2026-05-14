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

### 4. Undo / Redo
**Priority: High — should land alongside or before edit operations**

- **⌘Z** — undo last action
- **⌘⇧Z** — redo
- Backed by `EditHistory` / `juce::UndoManager` (already stubbed in `Source/Model/EditHistory`)
- Undo granularity: per discrete action (click, delete, paste). Slider drags group as a single transaction.

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

### 7. Breakpoint-Level Selection & Editing
**Priority: Medium — fine-grained editing, needs design refinement**

Analogous to Illustrator's direct selection (white arrow) tool. Within one or more selected partials, individual breakpoints become selectable:

- Small hit-target handles appear on breakpoints when a partial is in "point edit" mode
- Drag selected breakpoints with:
  - **Horizontal constraint** (shift): shift in time only
  - **Vertical constraint** (cmd): shift frequency only
  - **Free drag**: shift both
- Multi-select breakpoints across partials

> **Needs more design work** — interaction model (how to enter/exit point-edit mode, handle target sizes at varying zoom levels) to be resolved before implementation.

---

### 8. Temporal Scrubbing
**Priority: Medium — performance/exploration tool**

Holding a modifier key (TBD — e.g., Option) while moving the cursor over the canvas activates scrub mode:

- Playback stops; the synth freezes at the cursor's current time position
- All active partials at that moment synthesize continuously at their instantaneous frequency and amplitude — a held "snapshot" of the spectrum
- As the cursor moves left/right, the snapshot updates with a short **slew time** (5–10 ms smoothing on frequency and amplitude) to prevent audible discontinuities from frame-to-frame jumps
- Releasing the modifier returns to normal transport state

---

## Export Features (from CLAUDE.md spec)

All exporters have stub implementations in `Source/Export/`. These need UI wiring and full implementation:

- [ ] **MIDI / MPE export** — one note per partial, 14-bit pitch bend, CC11 expression, bend-range histogram dialog
- [ ] **Csound score export** — Mode A (GEN02 tables) and Mode B (inline linseg), optional orchestra template
- [ ] **SDIF export** — 1TRC or RBEP frame type, via Loris's `SdifFile`
- [ ] **JSON export** — versioned schema, optional phase/bandwidth fields

---

## Open Design Decisions

| # | Decision | Context |
|---|----------|---------|
| 1 | Destructive shrink handling | Time-range compress (Feature #6) — warn, preview, or rely on undo? |
| 2 | Point-edit mode entry/exit | Breakpoint selection (Feature #7) — modifier key, double-click, toolbar toggle? |
| 3 | Scrub modifier key | Temporal scrubbing (Feature #8) — Option, Space+drag, dedicated mode button? |
| 4 | Clipboard scope | Does the clipboard persist across file loads? Across sessions? |
| 5 | Selection highlight colour | Currently white; should differ from muted/soloed state colours |
| 6 | Output gain UI | Master gain is in the synth but has no knob in the UI yet |