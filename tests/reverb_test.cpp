#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>
#include <numeric>
#include "Reverb.h"

static constexpr float kSampleRate = 48000.f;

// NOTE: FdnReverb's delay lines are file-scope statics in Reverb.cpp, so all
// test instances share the same buffers. DelayLine::Init() zeroes them, so
// this is safe as long as each test creates a fresh FdnReverb and calls Init().
// Do not rely on delay line state carrying over between tests.

// --- Test: Hadamard matrix preserves energy ---
// Uses friend access (via UNIT_TEST guard in Reverb.h) to test the real method.
class HadamardTest : public ::testing::Test {
protected:
    static void Hadamard4(float* s) { FdnReverb::Hadamard4(s); }
};

TEST_F(HadamardTest, EnergyPreservation) {
    float input[] = {1.f, 2.f, 3.f, 4.f};
    float energy_in = 0.f;
    for (int i = 0; i < 4; i++) energy_in += input[i] * input[i];

    Hadamard4(input);

    float energy_out = 0.f;
    for (int i = 0; i < 4; i++) energy_out += input[i] * input[i];

    EXPECT_NEAR(energy_in, energy_out, 1e-5f);
}

TEST_F(HadamardTest, IdentityVector) {
    // H4 * [1,1,1,1] = [2,0,0,0] with the 0.5 normalisation factor
    float v[] = {1.f, 1.f, 1.f, 1.f};
    Hadamard4(v);
    EXPECT_NEAR(v[0], 2.f, 1e-6f);
    EXPECT_NEAR(v[1], 0.f, 1e-6f);
    EXPECT_NEAR(v[2], 0.f, 1e-6f);
    EXPECT_NEAR(v[3], 0.f, 1e-6f);
}

// --- Test: Silence in -> silence out ---
TEST(FdnReverbTest, SilenceInSilenceOut) {
    FdnReverb rev;
    rev.Init(kSampleRate);

    // Run 2 seconds of silence
    float wl = 0.f, wr = 0.f;
    for (int i = 0; i < (int)(kSampleRate * 2); i++) {
        rev.Process(0.f, 0.f, &wl, &wr);
    }

    EXPECT_NEAR(wl, 0.f, 1e-6f);
    EXPECT_NEAR(wr, 0.f, 1e-6f);
}

// --- Test: Impulse response decays ---
TEST(FdnReverbTest, ImpulseDecays) {
    FdnReverb rev;
    rev.Init(kSampleRate);
    rev.SetDecay(1.5f);

    // Feed a single impulse
    float wl, wr;
    rev.Process(1.f, 1.f, &wl, &wr);

    // Let it ring for 4 seconds (well beyond RT60 of 1.5s)
    int total = (int)(kSampleRate * 4);
    int tail_window = 1000;
    for (int i = 0; i < total - tail_window; i++) {
        rev.Process(0.f, 0.f, &wl, &wr);
    }

    // Check RMS over final 1000 samples to avoid lucky-zero-crossing false pass
    float rms_l = 0.f, rms_r = 0.f;
    for (int i = 0; i < tail_window; i++) {
        rev.Process(0.f, 0.f, &wl, &wr);
        rms_l += wl * wl;
        rms_r += wr * wr;
    }
    rms_l = std::sqrt(rms_l / tail_window);
    rms_r = std::sqrt(rms_r / tail_window);

    EXPECT_LT(rms_l, 1e-4f);
    EXPECT_LT(rms_r, 1e-4f);
}

// --- Test: Output is bounded (no NaN/inf, SoftClip keeps range) ---
TEST(FdnReverbTest, StabilityExtremeParams) {
    FdnReverb rev;
    rev.Init(kSampleRate);

    // Extreme settings: max decay, low damping, full modulation
    rev.SetDecay(10.f);
    rev.SetDamping(500.f);
    rev.SetModDepth(1.f);
    rev.SetErLevel(1.f);
    rev.SetTone(20000.f);
    rev.SetRoomSize(2.f);

    float wl, wr;
    // Feed continuous loud signal
    for (int i = 0; i < (int)(kSampleRate * 3); i++) {
        float input = (i < (int)kSampleRate) ? 0.8f : 0.f;
        rev.Process(input, input, &wl, &wr);

        ASSERT_FALSE(std::isnan(wl)) << "NaN at sample " << i;
        ASSERT_FALSE(std::isnan(wr)) << "NaN at sample " << i;
        ASSERT_FALSE(std::isinf(wl)) << "Inf at sample " << i;
        ASSERT_FALSE(std::isinf(wr)) << "Inf at sample " << i;
        // SoftClip should keep output in [-1, 1]
        ASSERT_LE(std::fabs(wl), 1.0f + 1e-6f) << "Out of range at sample " << i;
        ASSERT_LE(std::fabs(wr), 1.0f + 1e-6f) << "Out of range at sample " << i;
    }
}

// --- Test: Parameter change doesn't produce large discontinuity ---
TEST(FdnReverbTest, ParameterSmoothing) {
    FdnReverb rev;
    rev.Init(kSampleRate);
    rev.SetDecay(2.f);
    rev.SetDamping(6000.f);

    float wl, wr, prev_l = 0.f;

    // Feed steady signal to get reverb going
    for (int i = 0; i < (int)(kSampleRate * 0.5f); i++) {
        rev.Process(0.3f, 0.3f, &wl, &wr);
        prev_l = wl;
    }

    // Abrupt parameter change
    rev.SetDecay(8.f);
    rev.SetDamping(1000.f);
    rev.SetModDepth(1.f);

    float max_jump = 0.f;
    for (int i = 0; i < (int)(kSampleRate * 0.1f); i++) {
        rev.Process(0.3f, 0.3f, &wl, &wr);
        float jump = std::fabs(wl - prev_l);
        max_jump = std::max(max_jump, jump);
        prev_l = wl;
    }

    // Per-sample jump should stay small (smoothing prevents clicks)
    // A reasonable threshold for sample-to-sample change
    EXPECT_LT(max_jump, 0.1f) << "Large discontinuity detected after parameter change";
}

// --- Test: Bypass tail decay ---
TEST(FdnReverbTest, BypassTailDecays) {
    FdnReverb rev;
    rev.Init(kSampleRate);
    rev.SetDecay(2.f);

    float wl, wr;
    // Build up reverb tail
    for (int i = 0; i < (int)(kSampleRate * 0.5f); i++) {
        rev.Process(0.5f, 0.5f, &wl, &wr);
    }

    // Verify there IS a tail (non-zero output)
    rev.Process(0.f, 0.f, &wl, &wr);
    float tail_energy = wl * wl + wr * wr;
    EXPECT_GT(tail_energy, 1e-6f) << "Expected non-zero tail after signal";

    // Feed silence (simulating bypass), tail should decay
    int total = (int)(kSampleRate * 5);
    int tail_window = 1000;
    for (int i = 0; i < total - tail_window; i++) {
        rev.Process(0.f, 0.f, &wl, &wr);
    }

    // Check RMS over final 1000 samples
    float rms_l = 0.f, rms_r = 0.f;
    for (int i = 0; i < tail_window; i++) {
        rev.Process(0.f, 0.f, &wl, &wr);
        rms_l += wl * wl;
        rms_r += wr * wr;
    }
    rms_l = std::sqrt(rms_l / tail_window);
    rms_r = std::sqrt(rms_r / tail_window);

    EXPECT_LT(rms_l, 1e-4f);
    EXPECT_LT(rms_r, 1e-4f);
}
