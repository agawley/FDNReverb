// FDN Reverb with Early Reflections & LFO Modulation
// for the Hothouse DIY DSP Platform
// Copyright (c) 2026 Alex Gawley
//
// MIT License - see LICENSE file for details.

#include "daisysp.h"
#include "hothouse.h"
#include "Reverb.h"

using clevelandmusicco::Hothouse;
using daisy::AudioHandle;
using daisy::Led;
using daisy::SaiHandle;
using namespace daisysp;

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
static Hothouse hw;
static FdnReverb reverb;
static Led led_bypass;
static bool bypass = true;

// Toggle lookup tables (indexed by ToggleswitchPosition: UP=0, MIDDLE=1, DOWN=2)
static const float kRoomSize[3] = {2.5f, 1.8f, 1.0f};
static const float kModSpeed[3] = {2.0f, 1.0f, 0.5f};
static const float kCharMult[3] = {1.5f, 1.0f, 0.5f};

// ---------------------------------------------------------------------------
// Audio callback
// ---------------------------------------------------------------------------
void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out, size_t size) {
    hw.ProcessAllControls();

    // Bypass toggle
    if (hw.switches[Hothouse::FOOTSWITCH_2].RisingEdge()) {
        bypass = !bypass;
    }

    // Read knobs
    reverb.SetDecay(fmap(hw.GetKnobValue(Hothouse::KNOB_1),
                         0.2f, 45.f, Mapping::LOG));
    reverb.SetDamping(fmap(hw.GetKnobValue(Hothouse::KNOB_2),
                           1000.f, 16000.f, Mapping::LOG));
    reverb.SetModDepth(hw.GetKnobValue(Hothouse::KNOB_3));
    reverb.SetErLevel(hw.GetKnobValue(Hothouse::KNOB_4));
    reverb.SetTone(fmap(hw.GetKnobValue(Hothouse::KNOB_5),
                        1000.f, 18000.f, Mapping::LOG));
    float mix = hw.GetKnobValue(Hothouse::KNOB_6);

    // Read toggles
    reverb.SetRoomSize(kRoomSize[hw.GetToggleswitchPosition(
        Hothouse::TOGGLESWITCH_1)]);
    reverb.SetModSpeed(kModSpeed[hw.GetToggleswitchPosition(
        Hothouse::TOGGLESWITCH_2)]);
    reverb.SetCharacter(kCharMult[hw.GetToggleswitchPosition(
        Hothouse::TOGGLESWITCH_3)]);

    // Process audio
    for (size_t i = 0; i < size; i++) {
        float dry_l = in[0][i];
        float dry_r = in[1][i];

        // Trails bypass: feed silence when bypassed so tail rings out
        float wet_l, wet_r;
        reverb.Process(bypass ? 0.f : dry_l,
                       bypass ? 0.f : dry_r,
                       &wet_l, &wet_r);

        out[0][i] = dry_l * (1.f - mix) + wet_l * mix;
        out[1][i] = dry_r * (1.f - mix) + wet_r * mix;
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    hw.Init();
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    reverb.Init(hw.AudioSampleRate());

    led_bypass.Init(hw.seed.GetPin(Hothouse::LED_2), false);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while (true) {
        hw.DelayMs(10);
        led_bypass.Set(bypass ? 0.0f : 1.0f);
        led_bypass.Update();
        hw.CheckResetToBootloader();
    }
}
