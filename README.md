# PartialFKR

There are many applications and libraries built around sinusoidal analysis — most of them optimised for high-fidelity reproduction of the input, or clean modification of it. partialfkr is not one of those.

This application is designed to facilitate the aggressive reduction of a signal to its core components and then export that reduced data in a format useful for composition. The aesthetic reference points are works like Gerard Grisey's *Partiels* or James Tenney's *Three Indigenous Songs* — pieces where spectral analysis of a source becomes raw material for an entirely different instrumental reality. You analyse something, strip it down until it barely holds together, and hand the skeleton to an ensemble.

The workflow is a sort of f'd up convolution: the onsets, contours, and phonetic gestures of a source signal refracted through whatever timbre receives the exported data. Hence the name.

## What it does

partialfkr analyses audio into a cloud of sinusoidal partial tracks (frequency × time × amplitude), displays them in an interactive canvas, and lets you mute or delete partials while listening in real time. The goal is to find the minimum set that still carries the recognisable character of the source — then export that skeleton as MIDI/MPE, a Csound score, SDIF, or JSON to drive whatever instrument or system you actually want to hear.

Fidelity is not the goal. Characteristic preservation through reduction is.

See [`docs/DESIGN.md`](docs/DESIGN.md) for full design rationale.

## Requirements

- macOS 11.0+ (Apple Silicon or Intel)
- Xcode 14+ with Command Line Tools
- CMake 3.22+
- Ninja (`brew install ninja`)

## Build

```bash
git clone <repo> partialfkr && cd partialfkr
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
ninja
```

Run the app:

```bash
./PartialEditor_artefacts/Debug/PartialEditor.app/Contents/MacOS/PartialEditor
```

Release build:

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

Universal binary (arm64 + x86_64):

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" ..
ninja
```

## Run tests

```bash
ctest --output-on-failure
```

## Project status

Initial scaffolding. First milestone: load a WAV, analyse with Loris, display partials in the OpenGL canvas, resynthesize interactively.

## License

AGPLv3. See [`LICENSE`](LICENSE) and [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md).