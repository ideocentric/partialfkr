# Contributing to PartialFKR

## Prerequisites

### macOS

| Tool | Version | Install |
|------|---------|---------|
| Xcode Command Line Tools | 14+ | `xcode-select --install` |
| CMake | 3.22+ | `brew install cmake` |
| Ninja | any | `brew install ninja` |
| Python 3 | 3.8+ | system or `brew install python3` — *release packaging only* |

Xcode itself is not required; the Command Line Tools provide `clang`, `codesign`, and `hdiutil`.

### Linux (Ubuntu 22.04 / Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake ninja-build rpm \
    libasound2-dev \
    libx11-dev libxinerama-dev libxrandr-dev libxcursor-dev libxext-dev \
    libfreetype6-dev libfontconfig1-dev \
    libglu1-mesa-dev libgl1-mesa-dev mesa-common-dev \
    libgtk-3-dev
```

CMake 3.22+ is required. Ubuntu 22.04 ships 3.22; earlier releases need a manual install or the Kitware APT repo.

### Windows

| Tool | Version | Notes |
|------|---------|-------|
| Visual Studio 2022 | Community or higher | Include the **Desktop development with C++** workload |
| CMake | 3.22+ | Bundled with VS, or install separately from cmake.org |
| NSIS | 3.x | `winget install NSIS.NSIS` — *packaging only* |

All other dependencies are fetched automatically at configure time.

---

## Getting the source

```bash
git clone <repo> partialfkr
cd partialfkr
```

**Dependencies are handled automatically:**

| Dependency | How it arrives |
|-----------|---------------|
| JUCE 8 | Fetched by CMake `FetchContent` at configure time (gitignored) |
| Catch2 v3 | Fetched by CMake `FetchContent` at configure time (gitignored) |
| Loris | Vendored in `third_party/loris/`, compiled as a static library |

No manual `git submodule` steps or separate downloads are needed.

---

## Building

### macOS

Debug (fastest iteration):
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Release:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Universal binary (arm64 + x86_64, used for distribution):
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build
```

### Linux

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows

From a **Developer Command Prompt for VS 2022** (or PowerShell with the VS environment loaded):

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

ARM64:
```
cmake -B build -G "Visual Studio 17 2022" -A ARM64
cmake --build build --config Release
```

---

## Running the app

### macOS
```bash
open build/PartialFKR_artefacts/Debug/PartialFKR.app
```

### Linux
```bash
./build/PartialFKR_artefacts/Release/PartialFKR
```

### Windows
```
build\PartialFKR_artefacts\Release\PartialFKR.exe
```

---

## Running tests

```bash
ctest --test-dir build --output-on-failure
```

---

## Packaging

Packaging produces the distributable artifact. It is separate from a regular development build and may require additional tools.

### macOS — drag-and-drop DMG

Install dmgbuild once:
```bash
pip3 install dmgbuild
```

Build the DMG:
```bash
cmake --build build --target dmg
# Output: build/PartialFKR-0.1.0-macOS.dmg
```

The DMG layout (background image, icon positions, window size) is defined entirely in
`packaging/macos/dmgbuild-settings.py`. No Finder configuration or AppleScript is involved.
See [`docs/DISTRIBUTION.md`](docs/DISTRIBUTION.md) for the full signed-and-notarized release flow.

### Linux — .deb and .rpm

```bash
cd build && cpack
# Output: PartialFKR-0.1.0-Linux-<arch>.deb and .rpm
```

### Windows — NSIS installer

With NSIS installed:
```
cd build
cpack -C Release
# Output: PartialFKR-0.1.0-Windows-<arch>.exe
```

---

## Architecture overview

PartialFKR is built around a real-time audio thread that reads from a stable list of `Partial`
objects and writes directly to the output buffer. Mute and solo state are toggled via
`std::atomic<bool>` flags — no lock is required on the audio path. Edits that structurally alter
the partial list (delete, split) take a brief `juce::CriticalSection` between audio blocks.

The core workflow:
1. Audio file → Loris analysis → list of `Partial` objects (time × frequency × amplitude breakpoints)
2. OpenGL canvas renders partials as coloured line segments (brightness and weight from dB amplitude)
3. User mutes/deletes partials; the synthesizer reflects changes within ~100 ms
4. Reduced partial set exported as MIDI/MPE, Csound score, SDIF, or JSON

See [`docs/DESIGN.md`](docs/DESIGN.md) for the full rationale, data model, synthesis algorithm,
and MIDI export specification.

---

## Project layout

```
partialfkr/
├── CMakeLists.txt
├── Source/
│   ├── Analysis/         Loris wrapper (Analyzer.h/cpp) + data model (PartialData.h/cpp)
│   ├── Synthesis/        Real-time oscillator bank (PartialSynth.h/cpp)
│   ├── Model/            Project document, selection, reduction, undo history
│   ├── UI/               All JUCE components — main window, OpenGL canvas, panels
│   └── Export/           MIDI, Csound, SDIF, JSON exporters
├── third_party/
│   ├── loris/            Vendored Loris source (compiled as loris_static)
│   └── JUCE/             Auto-fetched by CMake (gitignored)
├── packaging/
│   ├── macos/            background.png, dmgbuild-settings.py, make-dmg.sh
│   └── linux/            deb/rpm post-install and post-remove scripts
├── Resources/
│   ├── dist/             Platform icons (.icns, .ico, .png), .desktop files, metainfo XML
│   └── license.rtf       RTF license shown as click-through SLA inside the DMG
├── scripts/
│   └── distribute.sh     Full macOS sign → notarize → staple → package flow
├── docs/
│   ├── DESIGN.md         Full design rationale and algorithms
│   └── DISTRIBUTION.md   Code signing and release packaging (macOS + Windows)
└── Tests/
```

---

## Code conventions

- **Naming:** `PascalCase` types, `camelCase` methods and variables. No Hungarian prefixes (`mFoo`, `_foo`).
- **Headers:** `#pragma once`.
- **Audio thread:** Must never allocate heap memory or acquire a lock. Mark audio-thread code `// audio thread`.
- **Partial identity:** `Partial::id` is stable for the object's lifetime. Key all selection and display state by ID, not by pointer or index.
- **Mute vs. delete:** Mute is a flag flip; delete is a separate undoable operation. Reduction operations set mute flags; they do not delete.

Full conventions and architectural invariants are in [`CLAUDE.md`](CLAUDE.md).

---

## Signing and distribution

For a signed, notarized, distributable release build, see [`docs/DISTRIBUTION.md`](docs/DISTRIBUTION.md).