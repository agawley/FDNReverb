# FDN Reverb

A stereo reverb effect for the [Hothouse DIY DSP Platform](https://www.clevelandmusicco.com/) built on the Daisy Seed. Features a 4x4 feedback delay network with early reflections, LFO modulation, and a Hadamard mixing matrix for a dense, natural-sounding reverb tail.

## Features

- **4x4 FDN** with mutually-prime delay lines and energy-preserving Hadamard matrix
- **18-tap early reflections** with exponential decay, split across stereo channels
- **Per-line LFO modulation** (4 sine LFOs at different rates) for chorus-like movement in the tail
- **Stereo processing** — no L+R summing; preserves stereo image with natural cross-mixing in the feedback path
- **Trails bypass** — reverb tail rings out when bypassed

## Controls

### Knobs

| Knob | Parameter | Range |
|------|-----------|-------|
| 1 | Decay | 0.2s – 45s RT60 |
| 2 | Damping | 1kHz – 16kHz lowpass cutoff in feedback |
| 3 | Mod Depth | 0–100% LFO excursion |
| 4 | ER Level | 0–100% early reflections in wet mix |
| 5 | Tone | 1kHz – 18kHz output lowpass |
| 6 | Mix | 0–100% dry/wet |

### Toggle Switches

| Toggle | UP | MIDDLE | DOWN |
|--------|-----|--------|------|
| 1 — Room Size | Enormous (2.5x) | Cathedral (1.8x) | Large Room (1.0x) |
| 2 — Mod Speed | Fast (2x) | Normal (1x) | Slow (0.5x) |
| 3 — Character | Bright (+1.5x damp) | Normal | Dark (0.5x damp) |

### Footswitches

- **Footswitch 1** — Long-press (2s) to enter DFU bootloader mode
- **Footswitch 2** — Bypass toggle

## Building & Flashing

Requires the [Daisy toolchain](https://daisy.audio/tutorials/cpp-dev-env/).

```bash
# First time: clone with submodules and build libraries
git clone --recursive <repo-url>
(cd libDaisy && make)
(cd DaisySP && make)

# Build and flash
make              # Build firmware
make clean        # Clean build artifacts
make program-dfu  # Flash via USB DFU
```

To enter DFU mode, long-press footswitch 1 (2 seconds) or hold BOOT + press RESET on the Daisy Seed.

## Architecture

- **`main.cpp`** — Hardware control reading, bypass logic, audio callback
- **`Reverb.h` / `Reverb.cpp`** — `FdnReverb` class: pre-delay, early reflections, FDN with LFO modulation, tone filtering, DC blocking
- **`hothouse.h` / `hothouse.cpp`** — Hardware abstraction for the Hothouse pedal

## Signal Flow

```
Input L/R → Pre-delay (20ms) → 18-tap Early Reflections (stereo split)
  → ER feeds into 4x4 FDN (L→lines 0,1 / R→lines 2,3)
  → Hadamard matrix cross-mixes all 4 lines in feedback
  → Wet = ER×level + FDN output → Tone filter → DC block → Soft clip
  → Output = Dry×(1-mix) + Wet×mix
```

## Libraries

- [libDaisy](https://github.com/electro-smith/libDaisy) — Hardware abstraction
- [DaisySP](https://github.com/electro-smith/DaisySP) — DSP modules (DelayLine, OnePole, Oscillator, DcBlock)
