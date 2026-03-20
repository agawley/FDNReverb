// Template for a pedal effect on the Hothouse DIY DSP Platform
// Copyright (c) 2026 Alex Gawley
//
//
// MIT License - see LICENSE file for details.

#include "daisysp.h"
#include "hothouse.h"

using clevelandmusicco::Hothouse;
using daisy::AudioHandle;
using daisy::Led;

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
Hothouse hw;

// ---------------------------------------------------------------------------
// Bypass / LEDs
// ---------------------------------------------------------------------------
Led led_bypass;
bool bypass = true;

// ---------------------------------------------------------------------------
// Audio callback
// ---------------------------------------------------------------------------
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
  hw.ProcessAllControls();

  // --- Master bypass (footswitch 2) ---
  if (hw.switches[Hothouse::FOOTSWITCH_2].RisingEdge()) {
    bypass = !bypass;
  }

  // --- Process audio ---
  if (!bypass) {
    for (size_t i = 0; i < size; i++) {
      // Do something awesome with in[0][i] and in[1][i], write to out[0][i] and out[1][i]
      float l = in[0][i];
      float r = in[1][i];

      out[0][i] = l;
      out[1][i] = r;
    }
  } else {
    // Fully bypassed — dry pass through
    for (size_t i = 0; i < size; i++) {
      out[0][i] = in[0][i];
      out[1][i] = in[1][i];
    }
  }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
  hw.Init();
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

  // LEDs
  led_bypass.Init(hw.seed.GetPin(Hothouse::LED_2), false);

  hw.StartAdc();
  hw.StartAudio(AudioCallback);

  while (true) {
    hw.DelayMs(10);

    // LED 2: master bypass
    led_bypass.Set(bypass ? 0.0f : 1.0f);
    led_bypass.Update();

    hw.CheckResetToBootloader();
  }
  return 0;
}
