# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A guitar pedal effect for the **Hothouse DIY DSP Platform** (Cleveland Music Co.), built on the **Daisy Seed** microcontroller using the **libDaisy** hardware abstraction and **DaisySP** DSP library.

## Build & Flash

Requires the [Daisy toolchain](https://daisy.audio/tutorials/cpp-dev-env/). The build system is the standard Daisy Makefile.

```bash
make              # Build firmware (output: build/effect.bin)
make clean        # Clean build artifacts
make program-dfu  # Flash via USB (DFU mode — hold BOOT+RESET on Daisy, or long-press footswitch 1)
```

There are no tests or linters configured.

## Architecture

- **`main.cpp`** — Main effect implementation. Contains `AudioCallback` (real-time audio processing) and `main()` (hardware init, main loop). This is where DSP code and control mappings go.
- **`hothouse.h` / `hothouse.cpp`** — Hardware abstraction for the Hothouse pedal (namespace `clevelandmusicco`). Maps physical controls (6 knobs, 3 toggle switches, 2 footswitches, 2 LEDs) to the Daisy Seed pins. Provides `ProcessAllControls()`, `GetKnobValue()`, `GetToggleswitchPosition()`, and footswitch callback registration for normal/double/long press detection.
- **`libDaisy/`**, **`DaisySP/`** — Git submodules. Do not modify.

## Key Conventions

- Audio runs at 48kHz, block size 48 samples. The `AudioCallback` is called at ~1kHz and must be real-time safe (no allocations, no blocking).
- Knob values are `0.0–1.0` floats via `hw.GetKnobValue(Hothouse::KNOB_1)` through `KNOB_6`.
- Toggle switches have 3 positions: `TOGGLESWITCH_UP`, `TOGGLESWITCH_MIDDLE`, `TOGGLESWITCH_DOWN` (2-position toggles never return MIDDLE).
- Footswitch 2 is conventionally used for bypass toggle. Footswitch 1 long-press (2s) resets to bootloader.
- DaisySP provides ready-made DSP modules (reverb, delay, filters, oscillators, etc.) — prefer these over hand-rolled implementations.
