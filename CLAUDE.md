# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

SideMk is a stereo **sidechain compressor** audio plugin built with JUCE 8 and the `juce::dsp` module. It builds as **VST3** and **Standalone** (AU can be enabled on macOS — see `CMakeLists.txt`).

## Build

CMake project, MSVC / Visual Studio 17 2022 generator. **Note:** the existing `build/` cache points at VS 2022 BuildTools, which is currently **not installed** on this machine (no MSVC toolchain found) — the build fails at the generator step until a C++ toolchain is installed. Ask before installing anything. From the repo root:

```sh
cmake -B build                          # configure (only needed after CMakeLists.txt changes)
cmake --build build --config Debug      # or --config Release
```

Build a single target instead of everything:

```sh
cmake --build build --config Debug --target SideMk_VST3         # VST3 only
cmake --build build --config Debug --target SideMk_Standalone   # standalone app (fastest debug loop)
```

The Standalone target produces a runnable `.exe` — use it to iterate on DSP/UI without loading a DAW.

### Release / distribution scripts

- **`build_windows.ps1`** — configure + Release build, then copies `SideMk.vst3` and `SideMk.exe` into `%USERPROFILE%\Desktop\dist`. **It deletes and recreates that `dist` folder** on every run.
- **`build_linux.sh`** — headless Release build into `build-linux/`; copies artefacts to `/workspace/out` if that directory exists.
- **`Dockerfile`** (Ubuntu 24.04) — Linux CI image whose entrypoint is `build_linux.sh`: `docker build -t sidemk-build .` then `docker run --rm -v "$(pwd)/dist:/workspace/out" sidemk-build`. Produces Linux artefacts only. `.dockerignore` excludes `build/`, `JUCE/` etc., so JUCE is fetched inside the container.

### JUCE dependency

JUCE is resolved in two ways (see the top of `CMakeLists.txt`):
1. **Local checkout** at `./JUCE` — declared as a git submodule but currently **not initialized** (empty). Run `git submodule update --init --recursive` to populate it, or clone JUCE there manually.
2. **FetchContent fallback** — if `./JUCE` is empty, CMake auto-downloads JUCE 8.0.13 (~250 MB) into `build/_deps` at configure time. This is what the existing `build/` used.

Force the local checkout with `-DJUCE_PATH=/path/to/JUCE`.

There are no unit tests in this repo; verify changes by building the Standalone target or loading the VST3 in a host.

## Architecture

Flat, four-file JUCE plugin. No `Source/` subdirectory.

- **`PluginProcessor.{h,cpp}`** — `SidechainCompressorAudioProcessor`. Audio engine + parameters.
- **`PluginEditor.{h,cpp}`** — `SidechainCompressorAudioProcessorEditor` + `ChromePinkLookAndFeel` + `GainReductionMeter`. The GUI.
- **`JucePluginDefines.h`** — canonical `JucePlugin_*` macros. Must be included **before** `<JuceHeader.h>`. `juce_add_plugin()` also injects these as compiler flags; the `#ifndef` guards in the header make it a safe fallback for non-CMake builds / IntelliSense.

### Signal flow

The plugin uses a **two-bus** layout (`PluginProcessor.cpp` constructor):
- Bus 0: main stereo in/out (required)
- Bus 1: sidechain stereo in (optional, disabled by default)

Buses are accessed with `getBusBuffer()` (not raw channel indices). When the sidechain bus is enabled it is the detection source; when disabled, the main signal drives detection (standard feedforward compression).

There is **no `juce::dsp::Compressor`** — it was replaced by a hand-rolled detector + gain computer (documented in the `processBlock` comment block). Pipeline per block:
1. **Control layer** — read params from cached atomics; feed attack/release to the envelope filter; set the makeup-gain smoother's target.
2. **Detection** — stereo-linked peak: `max(|L|, |R|)` of the detection source per sample (avoids L+R phase cancellation).
3. **Gain computer** — `juce::dsp::BallisticsFilter<float>` (mono, `peak` level type) produces the envelope; convert to dB and apply a **hard-knee** curve: `grDb = (envDb - threshold) * (1/ratio - 1)` above threshold, else 0. No division-based gain recovery.
4. **Apply** — per-sample `gain(grDb) × makeupGainSmoothed` is written to the pre-allocated `gainBuffer`, then multiplied into the **main** channels only with SIMD `AudioBlock::multiplyBy`. Sidechain channels are never written.

Makeup gain uses `SmoothedValue<…, Multiplicative>` with a 20 ms ramp (safe because the range never reaches 0 linear).

`processBlock` is strictly real-time safe: no heap allocation, no locks, `ScopedNoDenormals`, parameters read through cached `std::atomic<float>*` pointers. All DSP state (`envelopeFilter`, smoother, `gainBuffer`) is prepared in `prepareToPlay`.

`gainBuffer` is sized to `samplesPerBlock` from `prepareToPlay`, but hosts may deliver larger blocks, so steps 3–4 run in slices of at most `gainBuffer.getNumSamples()` samples. Keep this chunking if you touch the loop — never index `gainBuffer` with the host block size.

### Metering (audio → GUI)

`processBlock` tracks the block's peak gain reduction and publishes it as a **positive dB magnitude** in `std::atomic<float> currentGainReductionDb` (`memory_order_relaxed`). The editor reads it via `getGainReductionDb()`. This is the pattern to follow for any further audio→GUI data: single atomics, no locks, no allocation.

### Parameters

All state lives in a single `AudioProcessorValueTreeState` (`apvts`). Parameters are defined once in `createParameterLayout()` and referenced everywhere by string ID: `threshold`, `ratio`, `attack`, `release`, `gain` (makeup / labeled "SIDECHAIN LEVEL" in the UI). Skewed `NormalisableRange`s give musical control resolution. State is persisted as XML via `get/setStateInformation`.

To add a parameter: add it to `createParameterLayout()`, cache its raw pointer in the constructor, add a `KnobGroup` + `SliderAttachment` in the editor, and wire it in `resized()`.

### GUI

Window is 580×300. `ChromePinkLookAndFeel` overrides `drawRotarySlider` for a custom chrome/pink-to-purple rotary look. Five rotary knobs in an equal-width grid laid out in `resized()` via `Rectangle::removeFromLeft`; a right-hand strip (`kMeterStripW` + `kMeterGap`) is reserved for the GR meter, and the column dividers in `paint()` are computed against that narrower grid — keep the two in sync if the layout changes.

`GainReductionMeter` is a self-contained `Component` with no timer of its own: the editor's single `juce::Timer` (**30 Hz**) calls `grMeter.refresh()`, which applies visual ballistics (instant rise, eased fall), and repaints only the meter's bounds when the bar visibly moves. Full scale is 24 dB, growing downward from the top.

The same timer polls the sidechain bus state and repaints **only** the top-right LED indicator region when it changes — never the whole window. Colors and layout constants are anonymous-namespace constants at the top of `PluginEditor.cpp`.

## Conventions

- Code comments are a mix of English and Spanish (especially `CMakeLists.txt`). Commit messages are in Spanish.
- `PLAN_ESCALADO_SIDEMK.md` holds the product roadmap (next milestone: DSP validation + basic tests, then an installable beta).
- Plugin identity is set for release: `COMPANY_NAME` `"Lily"`, `COMPANY_WEBSITE` `"https://lily.audio"`, `PLUGIN_MANUFACTURER_CODE` `"Lily"` (0x4C696C79), `PLUGIN_CODE` `"Smk1"` (0x536D6B31). These are mirrored in `JucePluginDefines.h` (incl. `CFBundleIdentifier` `com.lily.sidemk`) — keep the two files in sync if any value changes.

- Este es un plugin de audio VST/AU construido con JUCE y CMake. El procesamiento DSP ocurre en PluginProcessor.cpp y la interfaz en PluginEditor.cpp. Usa C++17. Prioriza código libre de bloqueos (lock-free) en el hilo de audio. No modifiques CMakeLists.txt ni instales dependencias externas sin preguntar