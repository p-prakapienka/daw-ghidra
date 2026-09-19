#include "components/StateVariableFilter.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

namespace {

constexpr int kUnit = 1 << 20;
constexpr double kSampleRate = 44100.0;

std::vector<int> sine(double frequency, unsigned int numSamples) {
    std::vector<int> buffer(numSamples);
    for (unsigned int index = 0; index < numSamples; ++index) {
        const double phase =
            2.0 * std::numbers::pi * frequency * static_cast<double>(index) / kSampleRate;
        buffer[index] = static_cast<int>(std::sin(phase) * kUnit);
    }
    return buffer;
}

// Level of the settled half of a filtered sine, relative to full scale.
double response(StateVariableFilter::Mode mode, float cutoff, float resonance, double frequency) {
    StateVariableFilter filter(true);
    filter.setMode(mode);
    filter.setCutoff(cutoff);
    filter.setResonance(resonance);
    filter.setInputGain(1.0f);

    auto buffer = sine(frequency, 22050);
    filter.process(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);

    double sum = 0.0;
    const std::size_t start = buffer.size() / 2;
    for (std::size_t index = start; index < buffer.size(); ++index) {
        const double value = static_cast<double>(buffer[index]) / kUnit;
        sum += value * value;
    }
    return std::sqrt(sum / static_cast<double>(buffer.size() - start));
}

} // namespace

TEST(SVFTables, MatchTheConstructorFormula) {
    const StateVariableFilter filter(true);

    EXPECT_EQ(filter.cutoffTableEntry(0), 482443);
    EXPECT_EQ(filter.cutoffTableEntry(128), 2845005);
    EXPECT_EQ(filter.cutoffTableEntry(255), 16546238);
}

TEST(SVFTables, ExtendedResonanceStartsLowerThanStandard) {
    const StateVariableFilter extended(true);
    const StateVariableFilter standard(false);

    // 0.5^1.5 against 0.5^0, both in Q24.
    EXPECT_EQ(extended.dampingTableEntry(0), 5931641);
    EXPECT_EQ(standard.dampingTableEntry(0), StateVariableFilter::kMax24);

    // Damping falls as the control rises, which is what sharpens the filter.
    EXPECT_LT(extended.dampingTableEntry(255), extended.dampingTableEntry(0));
}

TEST(SVFTables, CutoffBelowTheFloorUsesTheFirstEntry) {
    StateVariableFilter filter(true);
    filter.setCutoff(0.0f);
    const int atZero = filter.cutoffCoefficient();

    filter.setCutoff(StateVariableFilter::kMinimumCutoff / 2.0f);

    EXPECT_EQ(filter.cutoffCoefficient(), atZero);
    EXPECT_EQ(atZero, filter.cutoffTableEntry(0));
}

TEST(SVFModes, BypassLeavesTheBufferUntouched) {
    StateVariableFilter filter(true);
    filter.setMode(StateVariableFilter::Mode::Bypass);

    std::vector<int> buffer{1, 2, 3, 4};
    const auto original = buffer;
    filter.process(buffer.data(), 4, 1);

    EXPECT_EQ(buffer, original);
}

TEST(SVFModes, LowPassKeepsLowFrequenciesAndRemovesHigh) {
    const double low = response(StateVariableFilter::Mode::LowPass, 0.5f, 0.2f, 500.0);
    const double high = response(StateVariableFilter::Mode::LowPass, 0.5f, 0.2f, 8000.0);

    EXPECT_GT(low, 0.5);
    EXPECT_LT(high, 0.05);
}

TEST(SVFModes, HighPassKeepsHighFrequenciesAndRemovesLow) {
    const double low = response(StateVariableFilter::Mode::HighPass, 0.5f, 0.2f, 100.0);
    const double high = response(StateVariableFilter::Mode::HighPass, 0.5f, 0.2f, 2000.0);

    EXPECT_LT(low, 0.05);
    EXPECT_GT(high, 0.5);
}

TEST(SVFModes, BandPassPeaksBetweenTheExtremes) {
    const double low = response(StateVariableFilter::Mode::BandPass, 0.5f, 0.2f, 100.0);
    const double middle = response(StateVariableFilter::Mode::BandPass, 0.5f, 0.2f, 500.0);
    const double high = response(StateVariableFilter::Mode::BandPass, 0.5f, 0.2f, 8000.0);

    EXPECT_GT(middle, low);
    EXPECT_GT(middle, high);
}

TEST(SVFStability, StaysBoundedAtFullResonanceAndCutoff) {
    StateVariableFilter filter(true);
    filter.setMode(StateVariableFilter::Mode::LowPass);
    filter.setCutoff(1.0f);
    filter.setResonance(1.0f);
    filter.setInputGain(1.0f);

    auto buffer = sine(1000.0, 44100);
    filter.process(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);

    for (const int value : buffer) {
        EXPECT_LT(std::abs(value), 64 * kUnit);
    }
}

TEST(SVFChannels, StereoFiltersBothChannelsIndependently) {
    StateVariableFilter filter(true);
    filter.setMode(StateVariableFilter::Mode::LowPass);
    filter.setCutoff(0.5f);
    filter.setResonance(0.2f);
    filter.setInputGain(1.0f);

    // Left carries the signal, right is silent.
    std::vector<int> buffer(256 * 2, 0);
    for (int index = 0; index < 256; ++index) {
        buffer[static_cast<std::size_t>(index) * 2] = kUnit;
    }

    filter.process(buffer.data(), 256, 2);

    bool leftMoved = false;
    for (int index = 0; index < 256; ++index) {
        EXPECT_EQ(buffer[static_cast<std::size_t>(index) * 2 + 1], 0);
        if (buffer[static_cast<std::size_t>(index) * 2] != 0) {
            leftMoved = true;
        }
    }
    EXPECT_TRUE(leftMoved);
}

TEST(SVFChannels, ResetClearsTheIntegrators) {
    StateVariableFilter filter(true);
    filter.setMode(StateVariableFilter::Mode::LowPass);
    filter.setCutoff(0.5f);
    filter.setResonance(0.2f);
    filter.setInputGain(1.0f);

    std::vector<int> excite(64, kUnit);
    filter.process(excite.data(), 64, 1);

    filter.reset();

    std::vector<int> silence(8, 0);
    filter.process(silence.data(), 8, 1);

    for (const int value : silence) {
        EXPECT_EQ(value, 0);
    }
}
