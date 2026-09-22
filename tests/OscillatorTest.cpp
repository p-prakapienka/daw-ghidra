#include "components/Oscillator.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

namespace {

constexpr int kSinePeak = 9830;   // 0.3 * 32767
constexpr int kSquarePeak = 6553;    // 0.2 * 32767
constexpr int kTrianglePeak = 13106; // 0.4 * 32767

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

    // The triangle is built at level 0.4, unlike the 0.3 of its siblings.
    EXPECT_EQ(triangle[0], 0);
    EXPECT_EQ(triangle[Oscillator::kTableSize / 4], kTrianglePeak);
    EXPECT_LT(std::abs(triangle[Oscillator::kTableSize / 2]), 32);
    EXPECT_LT(triangle[3 * Oscillator::kTableSize / 4], -kTrianglePeak + 32);
}

TEST(OscillatorBands, HQTypesHaveTheirOwnInterleavedTable) {
    EXPECT_NE(Oscillator::bandLimitedTable(Oscillator::Type::SawtoothHQ), nullptr);
    EXPECT_NE(Oscillator::bandLimitedTable(Oscillator::Type::SquareHQ), nullptr);
    EXPECT_EQ(Oscillator::bandLimitedTable(Oscillator::Type::Sine), nullptr);
}

TEST(OscillatorBands, HarmonicLimitsHalveAcrossTheBands) {
    const int expected[] = {512, 337, 169, 84, 42, 21, 10, 6};
    for (unsigned int band = 0; band < Oscillator::kBandCount; ++band) {
        EXPECT_EQ(Oscillator::kBandHarmonics[band], expected[band]);
    }
}

TEST(OscillatorBands, HigherPitchesSelectNarrowerBands) {
    EXPECT_LT(Oscillator::bandForFrequency(110.0f), Oscillator::bandForFrequency(2000.0f));
    EXPECT_EQ(Oscillator::bandForFrequency(8000.0f), Oscillator::kBandCount - 1);

    // Whatever band is chosen, its top harmonic stays below Nyquist, unless
    // the pitch is so high that even the narrowest band cannot manage it.
    for (const float hertz : {55.0f, 220.0f, 440.0f, 1000.0f, 4000.0f}) {
        const unsigned int band = Oscillator::bandForFrequency(hertz);
        if (band == Oscillator::kBandCount - 1) {
            continue;
        }
        EXPECT_LE(static_cast<float>(Oscillator::kBandHarmonics[band]) * hertz,
                  Oscillator::kSampleRate * 0.5f);
    }
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

    // Noise is built at level 0.4, like the triangle.
    EXPECT_GT(peak, 1000);
    EXPECT_LE(peak, kTrianglePeak);

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


namespace {

// Magnitude at one frequency, by direct correlation.
double magnitudeAt(const std::vector<int> &samples, double hertz) {
    double real = 0.0;
    double imaginary = 0.0;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const double phase = 2.0 * std::numbers::pi * hertz * static_cast<double>(index)
                             / static_cast<double>(Oscillator::kSampleRate);
        real += samples[index] * std::cos(phase);
        imaginary += samples[index] * std::sin(phase);
    }
    return 2.0 * std::sqrt(real * real + imaginary * imaginary)
           / static_cast<double>(samples.size());
}

} // namespace

TEST(OscillatorBands, BandLimitingRemovesAliasing) {
    const auto naive = render(Oscillator::Type::Sawtooth, 2000.0f, 44100);
    const auto limited = render(Oscillator::Type::SawtoothHQ, 2000.0f, 44100);

    // Both keep the fundamental.
    EXPECT_GT(magnitudeAt(naive, 2000.0), 1000.0);
    EXPECT_GT(magnitudeAt(limited, 2000.0), 1000.0);

    // Only the naive table puts energy at frequencies that are not harmonics.
    EXPECT_GT(magnitudeAt(naive, 1400.0), 10.0);
    EXPECT_LT(magnitudeAt(limited, 1400.0), 1.0);
    EXPECT_GT(magnitudeAt(naive, 3100.0), 10.0);
    EXPECT_LT(magnitudeAt(limited, 3100.0), 1.0);
}

TEST(OscillatorBands, BandLimitedSquareKeepsOnlyOddHarmonics) {
    const auto samples = render(Oscillator::Type::SquareHQ, 440.0f, 44100);

    const double first = magnitudeAt(samples, 440.0);
    const double second = magnitudeAt(samples, 880.0);
    const double third = magnitudeAt(samples, 1320.0);

    EXPECT_GT(first, 1000.0);
    EXPECT_LT(second, 1.0);

    // A square falls off as 1/h across its odd harmonics.
    EXPECT_NEAR(third, first / 3.0, first * 0.05);
}
