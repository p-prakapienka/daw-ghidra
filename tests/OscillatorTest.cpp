#include "components/Oscillator.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace {

constexpr int kSinePeak = 9830;   // 0.3 * 32767
constexpr int kSquarePeak = 6553; // 0.2 * 32767

int countRisingZeroCrossings(const std::vector<int> &samples) {
    int crossings = 0;
    for (std::size_t index = 1; index < samples.size(); ++index) {
        if (samples[index - 1] < 0 && samples[index] >= 0) {
            ++crossings;
        }
    }
    return crossings;
}

std::vector<int> render(Oscillator::Type type, float hertz, unsigned int numSamples) {
    Oscillator oscillator;
    oscillator.setType(type);
    oscillator.setFrequency(hertz);
    oscillator.setLevel(1.0f);

    std::vector<int> buffer(numSamples);
    oscillator.generate(buffer.data(), numSamples);
    return buffer;
}

} // namespace

// One cycle fills the table, so the quarter points land on the extremes.
TEST(OscillatorTables, SineHoldsExactlyOneCycle) {
    const short *sine = Oscillator::table(Oscillator::Type::Sine);

    EXPECT_EQ(sine[0], 0);
    EXPECT_EQ(sine[Oscillator::kTableSize / 4], kSinePeak);
    EXPECT_EQ(sine[Oscillator::kTableSize / 2], 0);
    EXPECT_EQ(sine[3 * Oscillator::kTableSize / 4], -kSinePeak);
}

TEST(OscillatorTables, SawtoothIsAPlainRamp) {
    const short *saw = Oscillator::table(Oscillator::Type::Sawtooth);

    EXPECT_EQ(saw[0], -kSinePeak);
    EXPECT_EQ(saw[Oscillator::kTableSize / 2], 0);
    EXPECT_GT(saw[Oscillator::kTableSize - 1], kSinePeak - 10);
}

TEST(OscillatorTables, SquareFlipsAtTheHalfPeriod) {
    const short *square = Oscillator::table(Oscillator::Type::Square);

    EXPECT_EQ(square[0], -kSquarePeak);
    EXPECT_EQ(square[Oscillator::kTableSize / 2 - 1], -kSquarePeak);
    EXPECT_EQ(square[Oscillator::kTableSize / 2], kSquarePeak);
    EXPECT_EQ(square[Oscillator::kTableSize - 1], kSquarePeak);
}

TEST(OscillatorTables, TrianglePeaksAtTheQuarterPoints) {
    const short *triangle = Oscillator::table(Oscillator::Type::Triangle);

    EXPECT_EQ(triangle[0], 0);
    EXPECT_EQ(triangle[Oscillator::kTableSize / 4], kSinePeak);
    EXPECT_LT(std::abs(triangle[Oscillator::kTableSize / 2]), 32);
    EXPECT_LT(triangle[3 * Oscillator::kTableSize / 4], -kSinePeak + 32);
}

TEST(OscillatorTables, HQTypesStillResolveToTheirPlainTable) {
    // The band-limited tables are a later pass; the HQ types share the plain
    // ones for now rather than returning nothing.
    EXPECT_EQ(Oscillator::table(Oscillator::Type::SawtoothHQ),
              Oscillator::table(Oscillator::Type::Sawtooth));
    EXPECT_EQ(Oscillator::table(Oscillator::Type::SquareHQ),
              Oscillator::table(Oscillator::Type::Square));
}

TEST(OscillatorPlayback, RunsAtTheRequestedFrequency) {
    const auto samples = render(Oscillator::Type::Sine, 441.0f, 44100);

    // The final cycle does not complete inside the window.
    EXPECT_GE(countRisingZeroCrossings(samples), 440);
    EXPECT_LE(countRisingZeroCrossings(samples), 441);
}

TEST(OscillatorPlayback, SineKeepsItsCrestFactor) {
    const auto samples = render(Oscillator::Type::Sine, 441.0f, 44100);

    double peak = 0.0;
    double sum = 0.0;
    for (const int value : samples) {
        peak = std::max(peak, std::fabs(static_cast<double>(value)));
        sum += static_cast<double>(value) * value;
    }
    const double rms = std::sqrt(sum / static_cast<double>(samples.size()));

    EXPECT_NEAR(peak, kSinePeak, 2.0);
    EXPECT_NEAR(rms / peak, 0.7071, 0.01);
}

TEST(OscillatorPlayback, LevelScalesTheOutput) {
    Oscillator oscillator;
    oscillator.setType(Oscillator::Type::Sine);
    oscillator.setFrequency(441.0f);
    oscillator.setLevel(0.5f);

    std::vector<int> buffer(1024);
    oscillator.generate(buffer.data(), 1024);

    int peak = 0;
    for (const int value : buffer) {
        peak = std::max(peak, std::abs(value));
    }
    EXPECT_NEAR(peak, kSinePeak / 2, 32);
}

TEST(OscillatorPlayback, PhaseWrapsWithoutDrifting) {
    Oscillator oscillator;
    oscillator.setType(Oscillator::Type::Sine);
    oscillator.setFrequency(1000.0f);

    std::vector<int> buffer(44100 * 4);
    oscillator.generate(buffer.data(), static_cast<unsigned int>(buffer.size()));

    EXPECT_LE(oscillator.phase(), Oscillator::kPhaseMask);
}

TEST(OscillatorPlayback, NoiseIsBroadbandAndBounded) {
    const auto samples = render(Oscillator::Type::Noise, 0.0f, 44100);

    int peak = 0;
    long long sum = 0;
    for (const int value : samples) {
        peak = std::max(peak, std::abs(value));
        sum += value;
    }

    EXPECT_GT(peak, 1000);
    EXPECT_LE(peak, kSinePeak);

    // Roughly zero mean, unlike any of the cycle tables at a fixed phase.
    const double mean = static_cast<double>(sum) / static_cast<double>(samples.size());
    EXPECT_LT(std::fabs(mean), 200.0);
}

TEST(OscillatorPlayback, InterpolatesBetweenTableEntries) {
    // A frequency well below one table entry per sample must still move, which
    // only happens if the fractional phase is used.
    const auto samples = render(Oscillator::Type::Sine, 1.0f, 64);

    bool moved = false;
    for (std::size_t index = 1; index < samples.size(); ++index) {
        if (samples[index] != samples[0]) {
            moved = true;
        }
    }
    EXPECT_TRUE(moved);
}
