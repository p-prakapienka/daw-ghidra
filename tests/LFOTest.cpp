#include "components/LFO.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace {

std::vector<int> renderCycle(LFO &lfo) {
    std::vector<int> samples;
    for (unsigned int position = 0; position < lfo.periodSamples(); position += LFO::kBlockSize) {
        lfo.generate(position, LFO::kBlockSize);
        samples.insert(samples.end(), lfo.output(), lfo.output() + LFO::kBlockSize);
    }
    samples.resize(lfo.periodSamples());
    return samples;
}

LFO makeLFO(LFO::Waveform waveform) {
    LFO lfo;
    lfo.setRate(9.0f); // four cycles per beat
    lfo.SetBPM(120.0f);
    lfo.setWaveform(waveform);
    return lfo;
}

double fraction(int value) { return static_cast<double>(value) / LFO::kUnity; }

} // namespace

TEST(LFORate, PeriodIsBeatLengthOverCyclesPerBeat) {
    LFO lfo;
    lfo.setRate(5.0f); // one cycle per beat
    lfo.SetBPM(120.0f);
    EXPECT_EQ(lfo.periodSamples(), 22050u);

    lfo.setRate(9.0f); // four per beat
    lfo.OnParamModified();
    EXPECT_EQ(lfo.periodSamples(), 5512u);

    lfo.setRate(0.0f); // one per sixteen beats
    lfo.OnParamModified();
    EXPECT_EQ(lfo.periodSamples(), 352800u);
}

TEST(LFORate, IndicesPastTheTableUseOneCyclePerBeat) {
    LFO lfo;
    lfo.setRate(40.0f);
    lfo.SetBPM(120.0f);
    EXPECT_EQ(lfo.periodSamples(), 22050u);
}

TEST(LFORate, DefaultsToOneCyclePerEightBeatsAt120) {
    // The constructor leaves rate 1.0, which indexes 0.125.
    const LFO lfo;
    EXPECT_EQ(lfo.periodSamples(), 176400u);
}

TEST(LFOShapes, SineSpansFullScaleAndStartsRising) {
    LFO lfo = makeLFO(LFO::Waveform::Sine);
    const auto samples = renderCycle(lfo);

    EXPECT_NEAR(fraction(samples.front()), 0.0, 0.01);
    EXPECT_NEAR(fraction(samples[samples.size() / 4]), 1.0, 0.01);
    EXPECT_NEAR(fraction(samples[3 * samples.size() / 4]), -1.0, 0.01);
    EXPECT_NEAR(fraction(*std::max_element(samples.begin(), samples.end())), 1.0, 0.001);
}

TEST(LFOShapes, TriangleStartsAtTheTop) {
    LFO lfo = makeLFO(LFO::Waveform::Triangle);
    const auto samples = renderCycle(lfo);

    EXPECT_NEAR(fraction(samples.front()), 1.0, 0.01);
    EXPECT_NEAR(fraction(samples[samples.size() / 2]), -1.0, 0.05);
}

TEST(LFOShapes, SawtoothFallsAndIsSmoothedAtTheReset) {
    LFO lfo = makeLFO(LFO::Waveform::Sawtooth);
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
    LFO lfo = makeLFO(LFO::Waveform::Square);
    const auto samples = renderCycle(lfo);

    EXPECT_LT(fraction(samples[samples.size() / 8]), -0.9);
    EXPECT_GT(fraction(samples[samples.size() / 2]), 0.9);
    EXPECT_LT(fraction(samples[7 * samples.size() / 8]), -0.9);
}

TEST(LFODepth, ScalesTheOutput) {
    LFO lfo = makeLFO(LFO::Waveform::Sine);
    lfo.setDepth(0.25f);
    const auto samples = renderCycle(lfo);

    EXPECT_NEAR(fraction(*std::max_element(samples.begin(), samples.end())), 0.25, 0.002);
}

TEST(LFOSync, IsPhaseLockedToTheSongPosition) {
    LFO lfo = makeLFO(LFO::Waveform::Sine);

    lfo.generate(1000, 128);
    const std::vector<int> first(lfo.output(), lfo.output() + 128);

    lfo.generate(40000, 128);
    lfo.generate(1000, 128);
    const std::vector<int> again(lfo.output(), lfo.output() + 128);

    EXPECT_EQ(first, again);
}

TEST(LFOSync, PhaseOffsetShiftsTheCycle) {
    LFO plain = makeLFO(LFO::Waveform::Sine);
    LFO shifted = makeLFO(LFO::Waveform::Sine);
    shifted.setPhaseOffset(LFO::kHalfCycle);

    plain.generate(0, 1);
    shifted.generate(0, 1);

    // Half a cycle later a rising zero crossing becomes a falling one; both
    // sit near zero, with opposite slopes. Compare a quarter in.
    const unsigned int quarter = plain.periodSamples() / 4;
    plain.generate(quarter, 1);
    shifted.generate(quarter, 1);
    EXPECT_NEAR(fraction(plain.output()[0]), 1.0, 0.01);
    EXPECT_NEAR(fraction(shifted.output()[0]), -1.0, 0.01);
}

TEST(LFOOff, WritesZeros) {
    LFO lfo = makeLFO(LFO::Waveform::Off);
    lfo.generate(0, 128);
    for (unsigned int index = 0; index < 128; ++index) {
        EXPECT_EQ(lfo.output()[index], 0);
    }
}
