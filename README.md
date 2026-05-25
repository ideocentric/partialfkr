# PartialFKR

There are many applications and libraries built around sinusoidal analysis — most of them optimised for high-fidelity reproduction of the input, or clean modification of it. partialfkr is not one of those.

This application is designed to facilitate the aggressive reduction of a signal to its core components and then export that reduced data in a format useful for composition. The aesthetic reference points are works like Gerard Grisey's *Partiels* or James Tenney's *Three Indigenous Songs* — pieces where spectral analysis of a source becomes raw material for an entirely different instrumental reality. You analyse something, strip it down until it barely holds together, and hand the skeleton to an ensemble.

The workflow is a sort of f'd up convolution: the onsets, contours, and phonetic gestures of a source signal refracted through whatever timbre receives the exported data. Hence the name.

## What it does

partialfkr analyses audio into a cloud of sinusoidal partial tracks (frequency × time × amplitude), displays them in an interactive canvas, and lets you mute or delete partials while listening in real time. The goal is to find the minimum set that still carries the recognisable character of the source — then export that skeleton as MIDI/MPE, a Csound score, SDIF, or JSON to drive whatever instrument or system you actually want to hear.

Fidelity is not the goal. Characteristic preservation through reduction is.

See [`docs/DESIGN.md`](docs/DESIGN.md) for full design rationale.

## Requirements

- **macOS** 11.0+ — Xcode 14+ Command Line Tools, CMake 3.22+, Ninja
- **Linux** — CMake 3.22+, Ninja, ALSA/GTK/GL dev headers (see [CONTRIBUTING.md](CONTRIBUTING.md))
- **Windows** — Visual Studio 2022 with C++ workload, CMake 3.22+

JUCE and Catch2 are fetched automatically at configure time. Loris is vendored.

## Build

```bash
git clone <repo> partialfkr && cd partialfkr
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Run the app:

```bash
open build/PartialFKR_artefacts/Debug/PartialFKR.app   # macOS
./build/PartialFKR_artefacts/Release/PartialFKR         # Linux
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for release builds, universal binaries, Windows,
packaging, and signing.

## Run tests

```bash
ctest --test-dir build --output-on-failure
```

## Project status

Initial scaffolding. First milestone: load a WAV, analyse with Loris, display partials in the OpenGL canvas, resynthesize interactively.

## License

AGPLv3. See [`LICENSE`](LICENSE) and [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md).