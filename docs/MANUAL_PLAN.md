# PartialFKR — User Manual Production Plan

---

## Screenshot Automation Assessment

### What CAN be automated (JUCE component snapshots)

JUCE provides `Component::createComponentSnapshot()` which renders any component to a
`juce::Image` without OS involvement. A hidden `--screenshots` launch mode could:

1. Load `samples/` test data into a known state
2. Call `createComponentSnapshot()` on specific components (main window, transport bar,
   right panel, partial view at various zoom levels)
3. Save PNGs to `docs/screenshots/auto/`

This covers roughly **40%** of the needed images — all static in-window layouts.

### What CANNOT be automated

| Category | Reason |
|---|---|
| macOS menu bar | Rendered by the OS (NSMenu), outside JUCE's component tree |
| Native file dialogs | OS sheets, not JUCE components |
| Native alert dialogs | OS windows |
| PopupMenu | Renders as a transient OS window |

### Recommendation

- **Automate** the main window, panels, and component state screenshots
- **Manual capture** for all menus, submenus, and OS dialogs (~15 screenshots)

### macOS manual screenshot tips

| Shortcut | Effect |
|---|---|
| Cmd+Shift+4 → drag | Region select to file |
| Cmd+Shift+4 → Space → click window | Window with drop shadow |
| Cmd+Shift+4 → Space → Option+click | Window without shadow (cleaner for docs) |
| Cmd+Shift+3 | Full screen |

For menus: open the menu, then `Cmd+Shift+4` and drag to capture just the menu.

---

## Manual Outline

```
1. Introduction
   1.1  What is PartialFKR?
   1.2  The re-orchestration workflow
   1.3  Supported platforms

2. Getting Started
   2.1  Installation
   2.2  First launch
   2.3  Interface overview

3. Loading and Saving
   3.1  Analyzing an audio file
   3.2  Opening a project (.pfkr)
   3.3  Opening from Finder (double-click / drag)
   3.4  Save and Save As
   3.5  Close and Quit (unsaved changes)

4. The Partial Canvas
   4.1  Reading the display (time, frequency, amplitude)
   4.2  Navigating: zoom and pan
   4.3  The playhead and time display

5. Playback
   5.1  Play, Pause, Stop
   5.2  Loop mode
   5.3  Scrubbing
   5.4  Output gain and level meter
   5.5  Selection-based solo

6. Selection
   6.1  Select mode (whole partials)
   6.2  Direct Select mode (individual breakpoints)
   6.3  Rubber-band drag selection
   6.4  Select All, Deselect All, Invert Selection

7. Editing
   7.1  Copy, Cut, Paste (including across windows)
   7.2  Delete
   7.3  Undo and Redo
   7.4  Stretch / Compress
   7.5  Normalize
   7.6  Scale Amplitude

8. Reduction Filters
   8.1  Overview and workflow
   8.2  Top-N partials
   8.3  Minimum duration
   8.4  Minimum amplitude
   8.5  Frequency band (low / high)
   8.6  Minimum energy
   8.7  Reset filters

9. Gap Operations
   9.1  Bridge Partials (Cmd+J)
   9.2  Crossfade Overlap (Cmd+Shift+J)

10. The Inspector
    10.1  Partial count, frequency range, duration, amplitude

11. Export
    11.1  Export as MIDI/MPE
    11.2  Export MIDI Package (folder + BOM + readme)
    11.3  Export as Csound CSD
    11.4  Export as SuperCollider (.scd)
    11.5  Export as SDIF
    11.6  Export as JSON

12. Keyboard Shortcuts Reference
```

---

## Screenshot Index

Each entry lists: **ID · Description · State required · Method (Auto / Manual)**

### Section 2 — Getting Started

| ID | Description | State | Method |
|---|---|---|---|
| S-01 | Full application window — empty (just launched) | No file loaded | Auto |
| S-02 | Full application window — analysis loaded | Sample .pfkr loaded | Auto |
| S-03 | Interface overview — annotated callouts on S-02 | Same as S-02 | Auto + annotation |
| S-04 | Right panel hidden (View > Hide Panel) | Panel hidden | Auto |

### Section 3 — Loading and Saving

| ID | Description | State | Method |
|---|---|---|---|
| S-05 | File menu open | — | **Manual** |
| S-06 | File > Export submenu open | — | **Manual** |
| S-07 | Analyze Audio file picker dialog | Dialog open | **Manual** |
| S-08 | Open Project file picker dialog | Dialog open | **Manual** |
| S-09 | Save As dialog | Dialog open | **Manual** |
| S-10 | Unsaved changes dialog (Save / Don't Save / Cancel) | Dirty window, close triggered | **Manual** |

### Section 4 — The Partial Canvas

| ID | Description | State | Method |
|---|---|---|---|
| S-11 | Partial canvas — default zoom, many partials visible | Sample loaded | Auto |
| S-12 | Partial canvas — zoomed in (individual partial detail) | Zoomed | Auto |
| S-13 | Partial canvas — zoomed out (full time span) | Zoom to fit | Auto |
| S-14 | Playhead at mid-point | Playhead positioned | Auto |
| S-15 | Zoom buttons (+/−) annotated | — | Auto |

### Section 5 — Playback

| ID | Description | State | Method |
|---|---|---|---|
| S-16 | Transport bar — stopped state | Stopped | Auto |
| S-17 | Transport bar — playing state | Playing | Auto |
| S-18 | Transport bar — paused state | Paused | Auto |
| S-19 | Transport bar — loop enabled (green Loop button) | Loop on | Auto |
| S-20 | Level meter — active signal | Playing with signal | Auto |
| S-21 | Gain fader and dB scale | — | Auto |
| S-22 | Transport menu open | — | **Manual** |

### Section 6 — Selection

| ID | Description | State | Method |
|---|---|---|---|
| S-23 | Canvas — no selection (Select mode) | Select mode active | Auto |
| S-24 | Canvas — several partials selected (highlighted) | 3–5 partials selected | Auto |
| S-25 | Canvas — rubber-band drag in progress | Mid-drag | Auto |
| S-26 | Edit Mode buttons — Select active | Select mode | Auto |
| S-27 | Edit Mode buttons — Direct Select active | Direct Select mode | Auto |
| S-28 | Canvas — breakpoints selected (Direct Select) | BP selection | Auto |
| S-29 | Edit menu open (showing enabled/disabled state) | Some selection | **Manual** |

### Section 7 — Editing

| ID | Description | State | Method |
|---|---|---|---|
| S-30 | Stretch dialog | T key pressed, selection exists | **Manual** |
| S-31 | Scale Amplitude dialog | Edit > Scale Amplitude | **Manual** |
| S-32 | Before/after Stretch (two-image comparison) | Before + after states | Auto |

### Section 8 — Reduction Filters

| ID | Description | State | Method |
|---|---|---|---|
| S-33 | Filters panel — all sliders at default (reset state) | Defaults | Auto |
| S-34 | Filters panel — Top-N set to reduce partials | Top-N = 10 | Auto |
| S-35 | Filters panel — frequency band filter active | Freq range set | Auto |
| S-36 | Canvas — before reduction (all partials) | No filters | Auto |
| S-37 | Canvas — after reduction (fewer partials visible) | Filters applied | Auto |

### Section 9 — Gap Operations

| ID | Description | State | Method |
|---|---|---|---|
| S-38 | Canvas — two partials with a gap (before Bridge) | Gap present | Auto |
| S-39 | Canvas — after Bridge Partials (gap filled) | Joined | Auto |
| S-40 | Canvas — two overlapping partials (before Crossfade) | Overlap present | Auto |
| S-41 | Canvas — after Crossfade Overlap | Merged | Auto |

### Section 10 — Inspector

| ID | Description | State | Method |
|---|---|---|---|
| S-42 | Inspector panel — nothing selected | Loaded, no selection | Auto |
| S-43 | Inspector panel — partials selected, data shown | 3 partials selected | Auto |

### Section 11 — Export

| ID | Description | State | Method |
|---|---|---|---|
| S-44 | MIDI/MPE export dialog (bend range selector) | Dialog open | **Manual** |
| S-45 | MIDI Package folder picker | Dialog open | **Manual** |
| S-46 | SDIF export dialog (frame type selector) | Dialog open | **Manual** |
| S-47 | Csound CSD save dialog | Dialog open | **Manual** |
| S-48 | SuperCollider save dialog | Dialog open | **Manual** |

### Section 12 — Menus (remaining)

| ID | Description | Method |
|---|---|---|
| S-49 | View menu open | **Manual** |
| S-50 | Window menu open | **Manual** |
| S-51 | Help menu open | **Manual** |
| S-52 | About dialog | **Manual** |

---

## Summary

| Category | Count | Method |
|---|---|---|
| Window / component states | 37 | Auto (JUCE snapshots) |
| Menus and OS dialogs | 15 | Manual |
| **Total** | **52** | — |

---

## JUCE Automated Screenshot Implementation Plan

When ready to implement, add to `Source/`:

```
Source/
└── Dev/
    └── ScreenshotHelper.h   (header-only, #ifdef JUCE_DEBUG only)
```

Launch with: `PartialFKR --screenshots path/to/sample.pfkr`

The helper would:
1. Load the provided .pfkr file
2. Wait one frame for layout to settle
3. Export component snapshots for all Auto entries above
4. Write PNGs to `docs/screenshots/auto/`
5. Exit

```cpp
// Rough interface (implementation deferred)
class ScreenshotHelper {
public:
    static bool isShotMode(const juce::String& cmdLine);
    static void run(MainComponent& mc, const juce::File& sampleFile,
                    const juce::File& outputDir);
};
```

---

## Recommended Production Order

1. **Implement ScreenshotHelper** → generates 37 images automatically
2. **Take 15 manual screenshots** (menus + dialogs) — ~30 minutes with the app running
3. **Annotate S-03** (interface overview callout image)
4. **Write manual text** against the outline above
5. **Compile to PDF/HTML** (Pandoc from Markdown, or Affinity Publisher)