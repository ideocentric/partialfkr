# CLAUDE.md

> Context file read by Claude Code at the start of each session. Keep dense.
> Longer rationale lives in `docs/DESIGN.md` — pull it in with `@docs/DESIGN.md` when relevant.

## Project: PartialEditor

Interactive sinusoidal analysis, editing, and resynthesis application. Spiritual successor to Michael Klingbeil's SPEAR. The core workflow is *qualitative partial reduction* — mute or delete partials until the source is no longer recognizable, then export the reduced data to drive other instruments (csound scores, MIDI for sample-based ensembles).

The aesthetic goal is **re-orchestration**: hearing an implied source signal (its onsets, phonetic gestures, melodic shape) refracted through the timbre of an unrelated instrumental ensemble. Fidelity is *not* the goal; characteristic preservation through aggressive reduction is.

Primary platform: macOS (Apple Silicon and Intel). Cross-platform structure preserved for future Linux/Windows.

## Core Workflow Loop

1. Load audio file → sinusoidal analysis → list of partials with breakpoints (time, frequency, amplitude).
2. Display partials in an editable OpenGL canvas (frequency vs time, colored by amplitude or partial ID).
3. User selects partials, mutes/deletes them, hears the difference within ~100 ms.
4. Apply reduction operations (top-N loudest, duration threshold, frequency band, etc.) with live audio preview.
5. Export reduced partial set as audio resynthesis, MIDI (MPE), csound score, SDIF, or JSON.

**The interactive "toggle mute, hear result, decide" loop is the load-bearing UX path.** Architectural decisions are subordinate to keeping it instant.

## Stack

- **Framework**: JUCE 8 (AGPLv3), CMake build, fetched via `FetchContent` from `juce-framework/JUCE` at tag `8.0.x`.
- **Analysis**: Loris C++ library (GPL v2+), vendored from the `musictheory/loris` fork in `third_party/loris/`. Built as static lib `loris_static` from project CMakeLists. Exclude Loris's Python bindings, CLI tools, and tests.
- **FFT**: `juce::dsp::FFT` (uses Apple Accelerate/vDSP on macOS automatically). Do NOT pull in FFTW directly.
- **Audio I/O**: `juce::AudioFormatReader` / `juce::AudioFormatWriter`.
- **GUI canvas**: OpenGL via JUCE's `OpenGLContext` attached to a `Component`. JUCE's 2D renderer is not fast enough for the partial view.
- **MIDI**: `juce::MidiFile` / `juce::MidiMessage`.
- **Language**: C++20.
- **Tests**: Catch2 v3 via `FetchContent`.
- **Build**: CMake 3.22+, Ninja generator, clang from current Xcode.

## License

Distributed under **AGPLv3**. All third-party licenses tracked in `THIRD_PARTY_LICENSES.md`. Source file headers carry `SPDX-License-Identifier: AGPL-3.0-or-later`. Loris is GPL v2+ which is upward-compatible with AGPLv3.

## Project Layout

```
PartialEditor/
├── CMakeLists.txt
├── LICENSE                          (AGPLv3 full text)
├── THIRD_PARTY_LICENSES.md
├── README.md
├── CLAUDE.md                        (this file)
├── docs/
│   └── DESIGN.md                    (detailed design rationale)
├── third_party/
│   ├── JUCE/                        (FetchContent, gitignored)
│   └── loris/                       (vendored source, committed)
├── Source/
│   ├── Main.cpp
│   ├── Analysis/
│   │   ├── Analyzer.{h,cpp}         Loris wrapper
│   │   └── PartialData.{h,cpp}      core data model
│   ├── Synthesis/
│   │   └── PartialSynth.{h,cpp}     real-time oscillator bank
│   ├── Model/
│   │   ├── Project.{h,cpp}          top-level document
│   │   ├── Selection.{h,cpp}
│   │   ├── ReductionController.{h,cpp}
│   │   └── EditHistory.{h,cpp}      juce::UndoManager-backed
│   ├── UI/
│   │   ├── MainComponent.{h,cpp}
│   │   ├── PartialView.{h,cpp}      OpenGL canvas
│   │   ├── ReductionPanel.{h,cpp}
│   │   ├── TransportBar.{h,cpp}
│   │   └── InspectorPanel.{h,cpp}
│   └── Export/
│       ├── CsoundExporter.{h,cpp}
│       ├── MidiExporter.{h,cpp}
│       ├── SdifExporter.{h,cpp}
│       └── JsonExporter.{h,cpp}
└── Tests/
    └── ...
```

## Architectural Invariants

These are load-bearing. **Do not change without explicit discussion in chat.**

1. **The audio thread never allocates and never locks.** It reads from a stable `std::vector<std::unique_ptr<Partial>>`, checks `std::atomic<bool>` mute/solo flags, and writes to the output buffer. List-altering edits (delete, split) take a brief `juce::CriticalSection` between audio blocks; flag toggles do not.
2. **Mute is a flag, not a deletion.** Reduction operations and selection-mutes both set flags. Deletion is a separate, undoable operation. The source partial data is preserved through experimentation.
3. **One source of truth for partials.** `Project` owns them via `unique_ptr`. UI and synth hold pointers/references, never copies.
4. **`Partial::id` is stable across the partial's lifetime.** UI keys selection state by ID, not by index or pointer.
5. **Loris's `bandwidth` field is ignored at synthesis.** Resynthesis is pure sinusoidal. Bandwidth-enhanced noise re-introduction is a future feature, deliberately out of scope.
6. **Analysis runs off the message thread.** `juce::ThreadPool` with progress reporting via `juce::AsyncUpdater` or `MessageManager::callAsync`.

## MIDI Export Specification (Locked)

- **One MIDI note per partial.** Each partial = exactly one note from first to last breakpoint. No re-articulation.
- **Bend range fixed per export**, chosen by user after a pre-pass that computes per-partial maximum deviation and shows a histogram. Allowed values: ±2, ±12, ±24, ±48 semitones.
- **MPE by default.** One partial per channel (channels 2-16, rotating). Channel 1 is the MPE manager channel.
- **Velocity** from initial amplitude via log mapping (`-80 dB → 1`, `0 dB → 127`).
- **CC11 (expression)** updated frame-by-frame to track amplitude envelope.
- **14-bit pitch bend** updated frame-by-frame.
- **`splitOnSaturation = false` by default.** Onset events are sacred for this use case. If bend range is insufficient, warn user; do not silently split.

Full math in `docs/DESIGN.md` § "MIDI Export Specification (Detailed)".

## Coding Conventions

- JUCE-style naming: `PascalCase` types, `camelCase` methods and variables. **No** Hungarian-notation prefixes (no `mFoo`, no `_foo`).
- `#pragma once` in headers, not include guards.
- `clang-format` with the project's `.clang-format` (JUCE defaults). Format on save in CLion.
- `juce::` types at API boundaries; `std::` types in internal implementation when equivalent or clearer.
- Doxygen-style comments on all public methods; private methods commented only where non-obvious.
- No `using namespace` in headers. Limited use in `.cpp` files inside function scope only.
- Prefer `[[nodiscard]]` on accessor returns and factory functions.
- All `Partial`-accessing code from the audio thread must be lock-free; comment it `// audio thread` for clarity.

## Build & Run

```bash
# First clone
git clone <repo> PartialEditor && cd PartialEditor
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
ninja

# Run
./PartialEditor.app/Contents/MacOS/PartialEditor   # macOS app bundle
# or
./PartialEditor                                     # if built as standalone exe
```

Release build: `-DCMAKE_BUILD_TYPE=Release`. Universal binary: add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.

## Confirm Before Doing

Stop and ask in chat before:

- Adding any new third-party dependency beyond JUCE, Loris, Catch2.
- Changing the FFT backend, the audio threading model, or the MIDI export spec above.
- Modifying any of the Architectural Invariants.
- Renaming top-level directories or restructuring `Source/`.
- Changing the license posture.
- Bumping JUCE major version or switching the Loris fork.

## Just Do (no confirmation needed)

- Adding new exporters in `Source/Export/` (file formats are isolated).
- Adding new reduction operations in `ReductionController`.
- Adding new UI panels or polishing existing ones.
- Writing tests.
- Fixing build warnings.
- Improving comments, documentation, error messages.
- Adding logging behind `juce::Logger`.

## Current Status

Project is in initial scaffolding phase. Next milestone: load a WAV, analyze with Loris, display partials in `PartialView`, resynthesize through `PartialSynth`. See `docs/DESIGN.md` § "Open Questions Deferred to First Working Build" for outstanding decisions.
