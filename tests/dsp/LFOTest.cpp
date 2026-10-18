#include "dsp/LFO.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace {

std::vector<int> renderCycle(LFO &lfo) {
    std::vector<int> samples;
    for (unsigned int position = 0; position < lfo.m_nPeriod; position += LFO::kBlockSize) {
        lfo.m_nSongPosition = position;
        lfo.GenerateInternal(LFO::kBlockSize);
        samples.insert(samples.end(), lfo.m_pnOutput, lfo.m_pnOutput + LFO::kBlockSize);
    }
    samples.resize(lfo.m_nPeriod);
    return samples;
}

// An LFO owns its output block and cannot be copied, so the helper sets one
// up in place.
void setUp(LFO &lfo, int waveform) {
    lfo.m_fRate = 9.0f; // four cycles per beat
    lfo.SetBPM(120.0f);
    lfo.m_nWaveform = waveform;
}

double fraction(int value) { return static_cast<double>(value) / LFO::kUnity; }

} // namespace

TEST(LFORate, PeriodIsBeatLengthOverCyclesPerBeat) {
    LFO lfo;
    lfo.m_fRate = 5.0f; // one cycle per beat
    lfo.SetBPM(120.0f);
    EXPECT_EQ(lfo.m_nPeriod, 22050u);

    lfo.m_fRate = 9.0f; // four per beat
    lfo.OnParamModified();
    EXPECT_EQ(lfo.m_nPeriod, 5512u);

    lfo.m_fRate = 0.0f; // one per sixteen beats
    lfo.OnParamModified();
    EXPECT_EQ(lfo.m_nPeriod, 352800u);
}

TEST(LFORate, IndicesPastTheTableUseOneCyclePerBeat) {
    LFO lfo;
    lfo.m_fRate = 40.0f;
    lfo.SetBPM(120.0f);
    EXPECT_EQ(lfo.m_nPeriod, 22050u);
}

TEST(LFORate, DefaultsToOneCyclePerEightBeatsAt120) {
    // The constructor leaves rate 1.0, which indexes 0.125.
    const LFO lfo;
    EXPECT_EQ(lfo.m_nPeriod, 176400u);
}

TEST(LFOShapes, SineSpansFullScaleAndStartsRising) {
    LFO lfo;
    setUp(lfo, LFO::kSine);
    const auto samples = renderCycle(lfo);

    EXPECT_NEAR(fraction(samples.front()), 0.0, 0.01);
    EXPECT_NEAR(fraction(samples[samples.size() / 4]), 1.0, 0.01);
    EXPECT_NEAR(fraction(samples[3 * samples.size() / 4]), -1.0, 0.01);
    EXPECT_NEAR(fraction(*std::max_element(samples.begin(), samples.end())), 1.0, 0.001);
}

TEST(LFOShapes, TriangleStartsAtTheTop) {
    LFO lfo;
    setUp(lfo, LFO::kTriangle);
    const auto samples = renderCycle(lfo);

    EXPECT_NEAR(fraction(samples.front()), 1.0, 0.01);
    EXPECT_NEAR(fraction(samples[samples.size() / 2]), -1.0, 0.05);
}

TEST(LFOShapes, SawtoothFallsAndIsSmoothedAtTheReset) {
    LFO lfo;
    setUp(lfo, LFO::kSawtooth);
    const auto samples = renderCycle(lfo);

    // A one-pole at 0.99 lags a ramp by about 99 samples, which on a
    // 5512-sample cycle falling two units is 0.036 behind the raw shape.
    const double lag = 99.0 * 2.0 / 5512.0;
    EXPECT_NEAR(fraction(samples[samples.size() / 4]), -0.5 + lag, 0.005);
    EXPECT_NEAR(fraction(samples[3 * samples.size() / 4]), 0.5 + lag, 0.005);

    // The one-pole smoothing means the jump never reaches the full rail.
    const int peak = *std::max_element(samples.begin(), samples.end());
    EXPECT_LT(fraction(peak), 0.95);
}

TEST(LFOShapes, SquareIsHighInTheMiddleHalf) {
    LFO lfo;
    setUp(lfo, LFO::kSquare);
    const auto samples = renderCycle(lfo);

    EXPECT_LT(fraction(samples[samples.size() / 8]), -0.9);
    EXPECT_GT(fraction(samples[samples.size() / 2]), 0.9);
    EXPECT_LT(fraction(samples[7 * samples.size() / 8]), -0.9);
}

TEST(LFODepth, ScalesTheOutput) {
    LFO lfo;
    setUp(lfo, LFO::kSine);
    lfo.m_nDepth = static_cast<int>(0.25f * 16777215.0f);
    const auto samples = renderCycle(lfo);

    EXPECT_NEAR(fraction(*std::max_element(samples.begin(), samples.end())), 0.25, 0.002);
}

TEST(LFOSync, IsPhaseLockedToTheSongPosition) {
    LFO lfo;
    setUp(lfo, LFO::kSine);

    lfo.m_nSongPosition = 1000;
    lfo.GenerateInternal(128);
    const std::vector<int> first(lfo.m_pnOutput, lfo.m_pnOutput + 128);

    lfo.m_nSongPosition = 40000;
    lfo.GenerateInternal(128);
    lfo.m_nSongPosition = 1000;
    lfo.GenerateInternal(128);
    const std::vector<int> again(lfo.m_pnOutput, lfo.m_pnOutput + 128);

    EXPECT_EQ(first, again);
}

TEST(LFOSync, PhaseOffsetShiftsTheCycle) {
    LFO plain;
    setUp(plain, LFO::kSine);
    LFO shifted;
    setUp(shifted, LFO::kSine);
    shifted.m_nPhaseOffset = LFO::kHalfCycle;

    plain.m_nSongPosition = 0;
    plain.GenerateInternal(1);
    shifted.m_nSongPosition = 0;
    shifted.GenerateInternal(1);

    // Half a cycle later a rising zero crossing becomes a falling one; both
    // sit near zero, with opposite slopes. Compare a quarter in.
    const unsigned int quarter = plain.m_nPeriod / 4;
    plain.m_nSongPosition = quarter;
    plain.GenerateInternal(1);
    shifted.m_nSongPosition = quarter;
    shifted.GenerateInternal(1);
    EXPECT_NEAR(fraction(plain.m_pnOutput[0]), 1.0, 0.01);
    EXPECT_NEAR(fraction(shifted.m_pnOutput[0]), -1.0, 0.01);
}

TEST(LFOOff, WritesZeros) {
    LFO lfo;
    setUp(lfo, LFO::kOff);
    lfo.m_nSongPosition = 0;
    lfo.GenerateInternal(128);
    for (unsigned int index = 0; index < 128; ++index) {
        EXPECT_EQ(lfo.m_pnOutput[index], 0);
    }
}
