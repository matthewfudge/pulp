# Overview

## What Is Pulp

Pulp is a cross-platform framework for building audio plugins and applications. It is MIT-licensed with no royalties, revenue thresholds, or copyleft obligations.

Pulp targets VST3, Audio Unit v2, and CLAP plugin formats today, with standalone application support. macOS is the primary development platform. Windows and Linux have build stubs but are not yet validated.

## What It Does Today

On the current branch, Pulp provides:

- **Three plugin formats** -- VST3, AU v2, CLAP -- validated with pluginval, auval, and clap-validator
- **Effects, instruments, and MIDI effects** -- all plugin types supported across formats
- **A thread-safe parameter system** -- atomic values, normalization, modulation offsets, state serialization
- **Audio I/O** -- CoreAudio on macOS, with device enumeration and buffer management
- **MIDI I/O** -- CoreMIDI on macOS, with message parsing and device access
- **DSP primitives** -- oscillators, filters (biquad, SVF, ladder, Linkwitz-Riley), delay lines, compressor, reverb, chorus, phaser, waveshaper, FFT, oversampling, ADSR, panning, noise gate, windowing, smoothed values
- **GPU rendering** -- Dawn (WebGPU) and Skia Graphite integration for 2D GPU-accelerated UIs
- **A view/widget system** -- JS-scriptable widgets, themes, hot-reload, screenshot capture, drag-and-drop, animation, inspector
- **Canvas abstraction** -- CoreGraphics and Skia backends, SVG support, post-processing effects
- **OSC support** -- Open Sound Control messaging
- **Headless processing** -- test and batch-process plugins without a DAW
- **A CLI tool** -- `pulp build`, `pulp test`, `pulp validate`, `pulp ship`, `pulp status`, `pulp clean`
- **Six example plugins** -- PulpGain, PulpTone, PulpEffect, PulpCompressor, PulpDrums, PulpSynth, plus a UI preview app
- **270 automated tests** -- unit tests, golden-file audio tests, format validators

## How It Is Organized

Pulp's code is organized into independent subsystems under `core/`:

| Subsystem | Purpose |
|-----------|---------|
| `runtime` | Lock-free primitives, logging, assertions |
| `events`  | Event loop, timers, async dispatch |
| `audio`   | Audio device I/O, buffers, file I/O |
| `midi`    | MIDI device I/O, message parsing, file I/O |
| `signal`  | DSP algorithms -- filters, oscillators, effects |
| `state`   | Parameters, state serialization, change notification |
| `format`  | Plugin format adapters (VST3, AU, CLAP, standalone, headless) |
| `platform`| Platform detection, native dialogs, clipboard |
| `canvas`  | 2D drawing abstraction (CoreGraphics, Skia), SVG, effects |
| `render`  | GPU surface management (Dawn/Skia Graphite) |
| `view`    | Widgets, layout, themes, JS scripting, hot-reload, inspector |
| `osc`     | Open Sound Control messaging |

Supporting directories:

| Directory   | Purpose |
|-------------|---------|
| `tools/`    | CLI, CMake modules, build utilities |
| `test/`     | Test framework and harnesses |
| `ship/`     | Code signing, notarization, packaging |
| `examples/` | Example plugins |
| `external/` | Third-party dependencies |
| `apple/`    | Swift subsystems (Apple only) |

## Five Principles

Pulp is built on five principles:

1. **Permissive licensing** -- MIT, no strings attached
2. **Modular architecture** -- use only what you need
3. **Native platform experiences** -- Swift on Apple, C++ core everywhere
4. **GPU-first rendering** -- WebGPU via Dawn, Skia Graphite for 2D
5. **Modern development workflows** -- AI-friendly, hot-reload, inspectable
