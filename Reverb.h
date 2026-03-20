// FDN Reverb with Early Reflections & LFO Modulation
// Copyright (c) 2026 Alex Gawley
//
// MIT License - see LICENSE file for details.

#pragma once
#include "daisysp.h"

class FdnReverb {
public:
    void Init(float sample_rate);

    void SetDecay(float seconds);
    void SetDamping(float freq_hz);
    void SetModDepth(float depth);
    void SetErLevel(float level);
    void SetTone(float freq_hz);
    void SetRoomSize(float mult);
    void SetModSpeed(float mult);
    void SetCharacter(float damp_mult);

    // Process one stereo sample pair, returns wet only
    void Process(float in_l, float in_r, float* wet_l, float* wet_r);

#ifdef UNIT_TEST
    friend class HadamardTest;
#endif

private:
    static const int kFdnSize   = 4;
    static const int kErNumTaps = 18;

    float sample_rate_;
    float room_mult_, mod_speed_mult_, char_mult_;

    // Smoothed parameters (current)
    float p_decay_, p_damping_, p_mod_depth_, p_er_level_, p_tone_;

    // Target parameters (set by Set* methods)
    float t_decay_, t_damping_, t_mod_depth_, t_er_level_, t_tone_;

    // FDN state
    float feedback_gain_[kFdnSize];
    daisysp::OnePole fdn_damp_[kFdnSize];
    daisysp::Oscillator lfo_[kFdnSize];

    // Input chorus
    daisysp::Oscillator chorus_lfo_l_, chorus_lfo_r_;

    // Output filtering
    daisysp::OnePole tone_l_, tone_r_;
    daisysp::DcBlock dc_l_, dc_r_;

    void UpdateFeedbackGains();
    static void Hadamard4(float* s);
};
