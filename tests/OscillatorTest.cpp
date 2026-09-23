#include "components/Oscillator.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace {

constexpr int kSinePeak = 9830;
constexpr int kSquarePeak = 6553;
constexpr int kTrianglePeak = 13106;

int countRisingZeroCrossings(const std::vector<int> &samples) {
    int crossings = 0;
    for (std::size_t index = 1; index < samples.size(); ++index) {
        if (samples[index - 1] < 0 && samples[index] >= 0) {
            ++crossings;
        }
    }
    return crossings;
}

constexpr int toOutput(int tableValue) { return tableValue << 9; }

std::vector<int> render(Oscillator::Type type, float hertz, unsigned int numSamples) {
    Oscillator oscillator;
    oscillator.setType(type);
    oscillator.setFrequency(hertz);
    std::vector<int> buffer(numSamples);
    oscillator.generate(buffer.data(), numSamples);
    return buffer;
}

} // namespace

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
    EXPECT_NEAR(peak, toOutput(kSinePeak), 1024.0);
    EXPECT_NEAR(rms / peak, 0.7071, 0.01);
}

TEST(OscillatorPlayback, PitchRatioMultipliesTheFrequency) {
    Oscillator oscillator;
    oscillator.setType(Oscillator::Type::Sine);
    oscillator.setFrequency(441.0f);
    oscillator.setPitchRatio(2.0f);
    std::vector<int> buffer(44100);
    oscillator.generate(buffer.data(), 44100);
    EXPECT_GE(countRisingZeroCrossings(buffer), 881);
    EXPECT_LE(countRisingZeroCrossings(buffer), 882);
}

TEST(OscillatorPlayback, PhaseWrapsWithoutDrifting) {
    Oscillator oscillator;
    oscillator.setType(Oscillator::Type::Sine);
    oscillator.setFrequency(1000.0f);
    std::vector<int> buffer(44100 * 4);
    oscillator.generate(buffer.data(), static_cast<unsigned int>(buffer.size()));
    EXPECT_LE(oscillator.phase(), Oscillator::kPhaseMask);
}

TEST(OscillatorNoise, MatchesTheEngineGenerator) {
    const int *noise = Oscillator::noiseTable();
    std::uint32_t a = 0xEFCDAB89u;
    std::uint32_t b = 0x67452301u;
    const float scale = 0.4f * 4.656612873077393e-10f;
    for (int index = 0; index < 64; ++index) {
        const auto value = static_cast<float>(static_cast<std::int32_t>(a));
        b ^= a;
        a += b;
        EXPECT_EQ(noise[index], static_cast<int>(value * scale * 16777215.0f));
    }
}

TEST(OscillatorNoise, IsCopiedStraightToTheOutput) {
    const auto samples = render(Oscillator::Type::Noise, 440.0f, 256);
    const int *noise = Oscillator::noiseTable();
    for (int index = 0; index < 256; ++index) {
        EXPECT_EQ(samples[static_cast<std::size_t>(index)], noise[index]);
    }
}

TEST(OscillatorNoise, StaysWithinItsLevel) {
    const auto samples = render(Oscillator::Type::Noise, 0.0f, 44100);
    int peak = 0;
    for (const int value : samples) {
        peak = std::max(peak, std::abs(value));
    }
    EXPECT_LE(peak, static_cast<int>(0.4f * 16777215.0f) + 1);
    EXPECT_GT(peak, static_cast<int>(0.3f * 16777215.0f));
}

TEST(OscillatorPlayback, InterpolatesBetweenTableEntries) {
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
    const double fundamental = magnitudeAt(limited, 2000.0);
    EXPECT_GT(magnitudeAt(naive, 2000.0), toOutput(1000));
    EXPECT_GT(fundamental, toOutput(1000));
    for (const double probe : {1400.0, 3100.0}) {
        EXPECT_GT(magnitudeAt(naive, probe), toOutput(10));
        EXPECT_LT(magnitudeAt(limited, probe), fundamental * 3e-5);
        EXPECT_LT(magnitudeAt(limited, probe), magnitudeAt(naive, probe) / 1000.0);
    }
}

TEST(OscillatorBands, BandLimitedSquareKeepsOnlyOddHarmonics) {
    const auto samples = render(Oscillator::Type::SquareHQ, 440.0f, 44100);
    const double first = magnitudeAt(samples, 440.0);
    const double second = magnitudeAt(samples, 880.0);
    const double third = magnitudeAt(samples, 1320.0);
    EXPECT_GT(first, toOutput(1000));
    EXPECT_LT(second, first * 1e-5);
    EXPECT_NEAR(third, first / 3.0, first * 0.05);
}

TEST(OscillatorVoice, KeepsEachOscillatorsPhaseInItsOwnSlot) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(441.0f, 69, 0.0f, 1.0f);
    Oscillator oscillator;
    std::vector<int> buffer(100);
    oscillator.generate(voltage, 1, buffer.data(), 100, Oscillator::Modulation{});
    EXPECT_EQ(voltage.phase[0], 0u);
    EXPECT_NE(voltage.phase[1], 0u);
    EXPECT_LE(voltage.phase[1], Oscillator::kPhaseMask);
}

TEST(OscillatorVoice, BendScalesThePitch) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(441.0f, 69, 0.0f, 1.0f);
    Oscillator oscillator;
    std::vector<int> buffer(44100);
    oscillator.generate(voltage, 0, buffer.data(), 44100, Oscillator::Modulation{}, 0.5f);
    EXPECT_GE(countRisingZeroCrossings(buffer), 219);
    EXPECT_LE(countRisingZeroCrossings(buffer), 221);
}

TEST(OscillatorVoice, GlideIsLinearInHertzAndClearsItself) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(220.0f, 57, 0.0f, 1.0f);
    voltage.beginGlide(440.0f, 1000);
    Oscillator oscillator;
    std::vector<int> buffer(1);
    voltage.samplesSinceNoteOn = 500;
    oscillator.generate(voltage, 0, buffer.data(), 1, Oscillator::Modulation{});
    EXPECT_FLOAT_EQ(voltage.currentPitch / ControlVoltage::kPitchScale, 330.0f);
    EXPECT_NE(voltage.glideSamples, 0u);
    voltage.samplesSinceNoteOn = 1000;
    oscillator.generate(voltage, 0, buffer.data(), 1, Oscillator::Modulation{});
    EXPECT_FLOAT_EQ(voltage.currentPitch / ControlVoltage::kPitchScale, 440.0f);
    EXPECT_EQ(voltage.glideSamples, 0u);
}

TEST(OscillatorVoice, PitchSweepRaisesThenDecays) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(441.0f, 69, 0.0f, 1.0f, 1.0f, 0.999f);
    Oscillator oscillator;
    std::vector<int> buffer(44100);
    oscillator.generate(voltage, 0, buffer.data(), 44100, Oscillator::Modulation{});
    EXPECT_GT(countRisingZeroCrossings(buffer), 441);
    EXPECT_LT(countRisingZeroCrossings(buffer), 882);
    EXPECT_LT(voltage.pitchSweep[0], 1e-6f);
}

TEST(OscillatorModulation, OctaveInputIsFlooredToWholeOctaves) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(441.0f, 69, 0.0f, 1.0f);
    std::vector<int> octave(44100, static_cast<int>(0.3 * (1 << 24)));
    Oscillator::Modulation modulation;
    modulation.octave = octave.data();
    Oscillator oscillator;
    std::vector<int> buffer(44100);
    oscillator.generate(voltage, 0, buffer.data(), 44100, modulation);
    EXPECT_GE(countRisingZeroCrossings(buffer), 881);
    EXPECT_LE(countRisingZeroCrossings(buffer), 882);
}

TEST(OscillatorModulation, FrequencyModulationFollowsTheInput) {
    ControlVoltage plainVoice = ControlVoltage::makeDefault();
    plainVoice.noteOn(441.0f, 69, 0.0f, 1.0f);
    ControlVoltage modulatedVoice = plainVoice;
    std::vector<int> input(44100, 1 << 23);
    Oscillator::Modulation fm;
    fm.fmInput = input.data();
    fm.fmAmount = (1 << 24) - 1;
    Oscillator oscillator;
    std::vector<int> plain(44100);
    std::vector<int> modulated(44100);
    oscillator.generate(plainVoice, 0, plain.data(), 44100, Oscillator::Modulation{});
    oscillator.generate(modulatedVoice, 0, modulated.data(), 44100, fm);
    EXPECT_NEAR(countRisingZeroCrossings(modulated), 661, 2);
    EXPECT_NEAR(countRisingZeroCrossings(plain), 441, 1);
}

TEST(OscillatorModulation, UnreconstructedModesStaySilentButKeepPhase) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(441.0f, 69, 0.0f, 1.0f);
    Oscillator oscillator;
    oscillator.setModulationMode(Oscillator::ModulationMode::Alternate2);
    std::vector<int> buffer(64, 123);
    oscillator.generate(voltage, 0, buffer.data(), 64, Oscillator::Modulation{});
    for (const int value : buffer) {
        EXPECT_EQ(value, 0);
    }
    EXPECT_NE(voltage.phase[0], 0u);
}
