// FDN Reverb with Early Reflections & LFO Modulation
// Copyright (c) 2026 Alex Gawley
//
// MIT License - see LICENSE file for details.

#include "Reverb.h"
#ifdef UNIT_TEST
#define DSY_SDRAM_BSS
#else
#include "daisy_seed.h"
#endif
#include <cmath>

using namespace daisysp;

// --- SDRAM delay lines ---
// GCC requires __attribute__((section)) after the declarator for template types
static DelayLine<float, 1024>  predelay_l  DSY_SDRAM_BSS;
static DelayLine<float, 1024>  predelay_r  DSY_SDRAM_BSS;
static DelayLine<float, 12288> er_delay_l  DSY_SDRAM_BSS;
static DelayLine<float, 12288> er_delay_r  DSY_SDRAM_BSS;
static DelayLine<float, 8192>  fdn_delay_0 DSY_SDRAM_BSS;
static DelayLine<float, 8192>  fdn_delay_1 DSY_SDRAM_BSS;
static DelayLine<float, 8192>  fdn_delay_2 DSY_SDRAM_BSS;
static DelayLine<float, 8192>  fdn_delay_3 DSY_SDRAM_BSS;
static DelayLine<float, 8192>* fdn_delay[4];
static DelayLine<float, 1024>  chorus_dl_l DSY_SDRAM_BSS;
static DelayLine<float, 1024>  chorus_dl_r DSY_SDRAM_BSS;

// --- Constants ---
static const int   kFdnBaseDelay[4] = {1453, 1871, 2467, 3259};
static const float kLfoBaseRate[4]  = {0.37f, 0.53f, 0.71f, 0.97f};
static const float kLfoMaxExc[4]    = {12.f, 15.f, 10.f, 13.f};
static const int   kPredelaySamp    = 960; // 20ms @ 48kHz
static const float kChorusCenterSamp = 336.f; // ~7ms @ 48kHz
static const float kChorusDepthSamp  = 144.f; // ~3ms excursion
static const float kChorusMix        = 0.2f;  // subtle blend
static const float kSmoothCoeff     = 0.002f;

static const int kErTapDelay[18] = {
    58, 168, 278, 389, 562, 734, 922, 1090, 1301,
    1512, 1766, 1978, 2299, 2563, 2789, 3106, 3422, 3792
};
static const float kErTapGain[18] = {
    0.85f, 0.75f, 0.70f, 0.65f, 0.60f, 0.55f, 0.50f, 0.45f, 0.42f,
    0.38f, 0.35f, 0.32f, 0.28f, 0.25f, 0.22f, 0.19f, 0.16f, 0.13f
};

void FdnReverb::Init(float sample_rate) {
    sample_rate_ = sample_rate;
    room_mult_      = 1.0f;
    mod_speed_mult_ = 1.0f;
    char_mult_      = 1.0f;

    // Default parameter targets and smoothed values
    t_decay_ = p_decay_ = 2.0f;
    t_damping_ = p_damping_ = 8000.f;
    t_mod_depth_ = p_mod_depth_ = 0.0f;
    t_er_level_ = p_er_level_ = 0.5f;
    t_tone_ = p_tone_ = 10000.f;

    // Set up FDN delay pointer array
    fdn_delay[0] = &fdn_delay_0;
    fdn_delay[1] = &fdn_delay_1;
    fdn_delay[2] = &fdn_delay_2;
    fdn_delay[3] = &fdn_delay_3;

    // Input chorus
    chorus_dl_l.Init();
    chorus_dl_r.Init();
    chorus_lfo_l_.Init(sample_rate_);
    chorus_lfo_l_.SetWaveform(Oscillator::WAVE_SIN);
    chorus_lfo_l_.SetFreq(0.6f);
    chorus_lfo_l_.SetAmp(1.0f);
    chorus_lfo_r_.Init(sample_rate_);
    chorus_lfo_r_.SetWaveform(Oscillator::WAVE_SIN);
    chorus_lfo_r_.SetFreq(0.77f);
    chorus_lfo_r_.SetAmp(1.0f);

    // Pre-delay
    predelay_l.Init();
    predelay_r.Init();

    // ER delay
    er_delay_l.Init();
    er_delay_r.Init();

    // FDN delays + damping + LFOs
    for (int i = 0; i < kFdnSize; i++) {
        fdn_delay[i]->Init();

        fdn_damp_[i].Init();
        fdn_damp_[i].SetFilterMode(OnePole::FILTER_MODE_LOW_PASS);
        fdn_damp_[i].SetFrequency(p_damping_ / sample_rate_);

        lfo_[i].Init(sample_rate_);
        lfo_[i].SetWaveform(Oscillator::WAVE_SIN);
        lfo_[i].SetFreq(kLfoBaseRate[i]);
        lfo_[i].SetAmp(1.0f);
    }

    // Output tone filters
    tone_l_.Init();
    tone_l_.SetFilterMode(OnePole::FILTER_MODE_LOW_PASS);
    tone_l_.SetFrequency(p_tone_ / sample_rate_);
    tone_r_.Init();
    tone_r_.SetFilterMode(OnePole::FILTER_MODE_LOW_PASS);
    tone_r_.SetFrequency(p_tone_ / sample_rate_);

    // DC blockers
    dc_l_.Init(sample_rate_);
    dc_r_.Init(sample_rate_);

    UpdateFeedbackGains();
}

void FdnReverb::SetDecay(float s)     { t_decay_ = s; }
void FdnReverb::SetDamping(float f)   { t_damping_ = f; }
void FdnReverb::SetModDepth(float d)  { t_mod_depth_ = d; }
void FdnReverb::SetErLevel(float l)   { t_er_level_ = l; }
void FdnReverb::SetTone(float f)      { t_tone_ = f; }
void FdnReverb::SetRoomSize(float m)  { room_mult_ = m; }
void FdnReverb::SetModSpeed(float m)  { mod_speed_mult_ = m; }
void FdnReverb::SetCharacter(float m) { char_mult_ = m; }

void FdnReverb::UpdateFeedbackGains() {
    for (int i = 0; i < kFdnSize; i++) {
        float delay_sec = (kFdnBaseDelay[i] * room_mult_) / sample_rate_;
        feedback_gain_[i] = powf(10.f, -3.f * delay_sec / p_decay_);
    }
}

void FdnReverb::Hadamard4(float* s) {
    float a = s[0] + s[1], b = s[0] - s[1];
    float c = s[2] + s[3], d = s[2] - s[3];
    s[0] = 0.5f * (a + c);
    s[1] = 0.5f * (b + d);
    s[2] = 0.5f * (a - c);
    s[3] = 0.5f * (b - d);
}

void FdnReverb::Process(float in_l, float in_r,
                         float* wet_l, float* wet_r) {
    // 1. Smooth parameters
    fonepole(p_decay_,     t_decay_,     kSmoothCoeff);
    fonepole(p_damping_,   t_damping_,   kSmoothCoeff);
    fonepole(p_mod_depth_, t_mod_depth_, kSmoothCoeff);
    fonepole(p_er_level_,  t_er_level_,  kSmoothCoeff);
    fonepole(p_tone_,      t_tone_,      kSmoothCoeff);

    // Update dependent parameters
    UpdateFeedbackGains();

    float damp_freq = p_damping_ * char_mult_ / sample_rate_;
    damp_freq = fclamp(damp_freq, 0.0001f, 0.499f);
    for (int i = 0; i < kFdnSize; i++) {
        fdn_damp_[i].SetFrequency(damp_freq);
        lfo_[i].SetFreq(kLfoBaseRate[i] * mod_speed_mult_);
    }
    float tone_freq = fclamp(p_tone_ / sample_rate_, 0.0001f, 0.499f);
    tone_l_.SetFrequency(tone_freq);
    tone_r_.SetFrequency(tone_freq);

    // 2. Input chorus
    chorus_dl_l.Write(in_l);
    chorus_dl_r.Write(in_r);
    float cho_l = chorus_dl_l.ReadHermite(kChorusCenterSamp + chorus_lfo_l_.Process() * kChorusDepthSamp);
    float cho_r = chorus_dl_r.ReadHermite(kChorusCenterSamp + chorus_lfo_r_.Process() * kChorusDepthSamp);
    in_l = in_l * (1.f - kChorusMix) + cho_l * kChorusMix;
    in_r = in_r * (1.f - kChorusMix) + cho_r * kChorusMix;

    // 3. Pre-delay
    predelay_l.Write(in_l);
    predelay_r.Write(in_r);
    float pd_l = predelay_l.Read(kPredelaySamp);
    float pd_r = predelay_r.Read(kPredelaySamp);

    // 4. Early reflections
    er_delay_l.Write(pd_l);
    er_delay_r.Write(pd_r);

    float er_l = 0.f, er_r = 0.f;
    for (int t = 0; t < 18; t++) {
        float delay_samp = kErTapDelay[t] * room_mult_;
        if (t & 1) {
            er_r += er_delay_r.Read(delay_samp) * kErTapGain[t];
        } else {
            er_l += er_delay_l.Read(delay_samp) * kErTapGain[t];
        }
    }

    // 5. FDN: read with LFO modulation, damp, Hadamard, feedback + ER inject
    float s[4];
    for (int i = 0; i < kFdnSize; i++) {
        float base_delay = kFdnBaseDelay[i] * room_mult_;
        float lfo_val = lfo_[i].Process();
        float mod_offset = lfo_val * kLfoMaxExc[i] * p_mod_depth_;
        float read_pos = base_delay + mod_offset;
        if (read_pos < 1.f) read_pos = 1.f;

        s[i] = fdn_delay[i]->ReadHermite(read_pos);
        s[i] = fdn_damp_[i].Process(s[i]);
        s[i] *= feedback_gain_[i];
    }

    Hadamard4(s);

    // FDN output (before writing back)
    float fdn_l = s[0] + s[1];
    float fdn_r = s[2] + s[3];

    // Write back to delay lines with ER injection
    fdn_delay[0]->Write(s[0] + er_l * 0.5f);
    fdn_delay[1]->Write(s[1] + er_l * 0.5f);
    fdn_delay[2]->Write(s[2] + er_r * 0.5f);
    fdn_delay[3]->Write(s[3] + er_r * 0.5f);

    // 6. Combine: wet = ER * er_level + FDN output
    float out_l = er_l * p_er_level_ + fdn_l;
    float out_r = er_r * p_er_level_ + fdn_r;

    // 7. Tone filter, DC block, soft clip
    out_l = tone_l_.Process(out_l);
    out_r = tone_r_.Process(out_r);
    out_l = dc_l_.Process(out_l);
    out_r = dc_r_.Process(out_r);
    out_l = SoftClip(out_l);
    out_r = SoftClip(out_r);

    *wet_l = out_l;
    *wet_r = out_r;
}
