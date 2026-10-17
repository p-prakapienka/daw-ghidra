#include "dsp/Oscillator.h"

#include <gtest/gtest.h>

#include "../support/Voice.h"
#include "dsp/WaveTables.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace {

constexpr int kSinePeak = 9830;
constexpr int kSquarePeak = 6553;
constexpr int kTrianglePeak = 13106;
constexpr unsigned int kBlock = LFO::kBlockSize;

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

// An oscillator wired as SubSynth wires one: five modulation sources (LFOs
// whose blocks the test fills; by default the "off" waveform and zeros, as
// SubSynth's default source) and a modulator input with an amount.
struct Rig {
    Rig() {
        for (LFO &source : sources) {
            source.m_nWaveform = LFO::kOff;
            std::fill(source.m_pnOutput, source.m_pnOutput + kBlock, 0);
        }
        osc.m_pVibrato = &sources[0];
        osc.m_pPhase = &sources[1];
        osc.m_pOctave = &sources[2];
        osc.m_pSemitones = &sources[3];
        osc.m_pFMDepth = &sources[4];
    }

    // Hold one source at a constant value, connected (not the default).
    void hold(LFO *source, int value) {
        source->m_nWaveform = LFO::kSine;
        std::fill(source->m_pnOutput, source->m_pnOutput + kBlock, value);
    }

    // Render in engine-sized blocks; the modulator input is fmInput, or
    // silence, at fmAmount.
    std::vector<int> render(ControlVoltage &cv, int index, unsigned int numSamples,
                            float bend = 1.0f, int fmInputValue = 0, int fmAmount = 0) {
        std::vector<int> output(numSamples);
        std::vector<int> fmInput(kBlock, fmInputValue);
        for (unsigned int offset = 0; offset < numSamples; offset += kBlock) {
            const unsigned int count = std::min(kBlock, numSamples - offset);
            osc.GenerateSignal(&cv, index, output.data() + offset, count, fmInput.data(), fmAmount,
                               bend);
        }
        return output;
    }

    LFO sources[5];
    Oscillator osc;
};

std::vector<int> render(Oscillator::OscillatorType type, float hertz, unsigned int numSamples) {
    Rig rig;
    rig.osc.SetOscillatorType(type);
    ControlVoltage cv;
    noteOn(cv, hertz, 69, 0.0f, 1.0f);
    return rig.render(cv, 0, numSamples);
}

// Building any oscillator builds the shared tables.
const Oscillator kTableHolder;

} // namespace

TEST(OscillatorTables, SineHoldsExactlyOneCycle) {
    const short *sine = Oscillator::m_psSinewaveTable;
    EXPECT_EQ(sine[0], 0);
    EXPECT_EQ(sine[Oscillator::kTableSize / 4], kSinePeak);
    EXPECT_EQ(sine[Oscillator::kTableSize / 2], 0);
    EXPECT_EQ(sine[3 * Oscillator::kTableSize / 4], -kSinePeak);
}

TEST(OscillatorTables, SawtoothIsAPlainRamp) {
    const short *saw = Oscillator::m_psSawtoothTable;
    EXPECT_EQ(saw[0], -kSinePeak);
    EXPECT_EQ(saw[Oscillator::kTableSize / 2], 0);
    EXPECT_GT(saw[Oscillator::kTableSize - 1], kSinePeak - 10);
}

TEST(OscillatorTables, SquareFlipsAtTheHalfPeriod) {
    const short *square = Oscillator::m_psSquarewaveTable;
    EXPECT_EQ(square[0], -kSquarePeak);
    EXPECT_EQ(square[Oscillator::kTableSize / 2 - 1], -kSquarePeak);
    EXPECT_EQ(square[Oscillator::kTableSize / 2], kSquarePeak);
    EXPECT_EQ(square[Oscillator::kTableSize - 1], kSquarePeak);
}

TEST(OscillatorTables, TrianglePeaksAtTheQuarterPoints) {
    const short *triangle = Oscillator::m_psTrianglewaveTable;
    EXPECT_EQ(triangle[0], 0);
    EXPECT_EQ(triangle[Oscillator::kTableSize / 4], kTrianglePeak);
    EXPECT_LT(std::abs(triangle[Oscillator::kTableSize / 2]), 32);
    EXPECT_LT(triangle[3 * Oscillator::kTableSize / 4], -kTrianglePeak + 32);
}

TEST(OscillatorBands, FrequencyPicksTheLowerOfTwoBandsAndAWeight) {
    float fraction = -1.0f;
    // Each band starts at its threshold and is blended towards the next over
    // one octave.
    EXPECT_EQ(WaveTables::FrequencyToWaveTable(440.0f, &fraction), 3);
    EXPECT_EQ(fraction, (440.0f - 261.0f) / 261.0f);
    EXPECT_EQ(WaveTables::FrequencyToWaveTable(65.0f, &fraction), 1);
    EXPECT_EQ(fraction, 0.0f);
    EXPECT_EQ(WaveTables::FrequencyToWaveTable(2093.0f, &fraction), 6);
    EXPECT_EQ(fraction, 0.0f);
    // Above twice the top threshold the weight stops at one.
    EXPECT_EQ(WaveTables::FrequencyToWaveTable(8000.0f, &fraction), 6);
    EXPECT_EQ(fraction, 1.0f);
    // Below 33 Hz the lowest band is used unblended.
    EXPECT_EQ(WaveTables::FrequencyToWaveTable(20.0f, &fraction), 0);
    EXPECT_EQ(fraction, 0.0f);
    EXPECT_EQ(WaveTables::FrequencyToWaveTable(49.5f, &fraction), 0);
    EXPECT_EQ(fraction, 0.5f);
}

TEST(OscillatorTables, SineUsesTheEnginesFrequencyLiteral) {
    // GenerateWavetables passes 10.7666f (0x412C43FE), not 44100 / 4096; the
    // difference moves a handful of entries by one.
    EXPECT_EQ(Oscillator::m_psSinewaveTable[1961], 1308);
}

TEST(OscillatorPlayback, RunsAtTheRequestedFrequency) {
    const auto samples = render(Oscillator::kSine, 441.0f, 44100);
    EXPECT_GE(countRisingZeroCrossings(samples), 440);
    EXPECT_LE(countRisingZeroCrossings(samples), 441);
}

TEST(OscillatorPlayback, SineKeepsItsCrestFactor) {
    const auto samples = render(Oscillator::kSine, 441.0f, 44100);
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
    Rig rig;
    rig.osc.m_fPitchRatio = 2.0f;
    ControlVoltage cv;
    noteOn(cv, 441.0f, 69, 0.0f, 1.0f);
    const auto buffer = rig.render(cv, 0, 44100);
    EXPECT_GE(countRisingZeroCrossings(buffer), 881);
    EXPECT_LE(countRisingZeroCrossings(buffer), 882);
}

TEST(OscillatorPlayback, PhaseWrapsWithoutDrifting) {
    Rig rig;
    ControlVoltage cv;
    noteOn(cv, 1000.0f, 69, 0.0f, 1.0f);
    rig.render(cv, 0, 44100 * 4);
    EXPECT_LE(cv.m_anPhase[0], 0xFFFFFFu);
}

TEST(OscillatorNoise, MatchesTheEngineGenerator) {
    const int *noise = Oscillator::m_pasWhitenoiseTable;
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
    const auto samples = render(Oscillator::kNoise, 440.0f, 256);
    const int *noise = Oscillator::m_pasWhitenoiseTable;
    for (int index = 0; index < 256; ++index) {
        EXPECT_EQ(samples[static_cast<std::size_t>(index)], noise[index]);
    }
}

TEST(OscillatorNoise, StaysWithinItsLevel) {
    const auto samples = render(Oscillator::kNoise, 0.0f, 44100);
    int peak = 0;
    for (const int value : samples) {
        peak = std::max(peak, std::abs(value));
    }
    EXPECT_LE(peak, static_cast<int>(0.4f * 16777215.0f) + 1);
    EXPECT_GT(peak, static_cast<int>(0.3f * 16777215.0f));
}

TEST(OscillatorPlayback, InterpolatesBetweenTableEntries) {
    const auto samples = render(Oscillator::kSine, 1.0f, 64);
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
                             / 44100.0;
        real += samples[index] * std::cos(phase);
        imaginary += samples[index] * std::sin(phase);
    }
    return 2.0 * std::sqrt(real * real + imaginary * imaginary)
           / static_cast<double>(samples.size());
}

} // namespace

TEST(OscillatorBands, BandLimitingRemovesAliasing) {
    const auto naive = render(Oscillator::kSawtooth, 2000.0f, 44100);
    const auto limited = render(Oscillator::kSawtoothHQ, 2000.0f, 44100);
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
    const auto samples = render(Oscillator::kSquareHQ, 440.0f, 44100);
    const double first = magnitudeAt(samples, 440.0);
    const double second = magnitudeAt(samples, 880.0);
    const double third = magnitudeAt(samples, 1320.0);
    EXPECT_GT(first, toOutput(1000));
    // Blending two separately rounded bands leaves even-harmonic leakage of
    // about 1e-5 of the fundamental.
    EXPECT_LT(second, first * 1e-4);
    EXPECT_NEAR(third, first / 3.0, first * 0.05);
}

TEST(OscillatorVoice, KeepsEachOscillatorsPhaseInItsOwnSlot) {
    ControlVoltage cv;
    noteOn(cv, 441.0f, 69, 0.0f, 1.0f);
    Rig rig;
    rig.render(cv, 1, 100);
    EXPECT_EQ(cv.m_anPhase[0], 0u);
    EXPECT_NE(cv.m_anPhase[1], 0u);
    EXPECT_LE(cv.m_anPhase[1], 0xFFFFFFu);
}

TEST(OscillatorVoice, BendScalesThePitch) {
    ControlVoltage cv;
    noteOn(cv, 441.0f, 69, 0.0f, 1.0f);
    Rig rig;
    const auto buffer = rig.render(cv, 0, 44100, 0.5f);
    EXPECT_GE(countRisingZeroCrossings(buffer), 219);
    EXPECT_LE(countRisingZeroCrossings(buffer), 221);
}

TEST(OscillatorVoice, GlideIsLinearInHertzAndClearsItself) {
    ControlVoltage cv;
    noteOn(cv, 220.0f, 57, 0.0f, 1.0f);
    beginGlide(cv, 440.0f, 1000);
    Rig rig;
    cv.m_nSamplesSinceNoteOn = 500;
    rig.render(cv, 0, 1);
    EXPECT_FLOAT_EQ(cv.m_fCurrentPitch / ControlVoltage::kPitchScale, 330.0f);
    EXPECT_NE(cv.m_nGlideSamples, 0u);
    cv.m_nSamplesSinceNoteOn = 1000;
    rig.render(cv, 0, 1);
    EXPECT_FLOAT_EQ(cv.m_fCurrentPitch / ControlVoltage::kPitchScale, 440.0f);
    EXPECT_EQ(cv.m_nGlideSamples, 0u);
}

TEST(OscillatorVoice, PitchSweepRaisesThenDecays) {
    ControlVoltage cv;
    noteOn(cv, 441.0f, 69, 0.0f, 1.0f, 1.0f, 0.999f);
    Rig rig;
    const auto buffer = rig.render(cv, 0, 44100);
    EXPECT_GT(countRisingZeroCrossings(buffer), 441);
    EXPECT_LT(countRisingZeroCrossings(buffer), 882);
    EXPECT_LT(cv.m_afPitchSweep[0], 1e-6f);
}

TEST(OscillatorModulation, OctaveInputIsFlooredToWholeOctaves) {
    ControlVoltage cv;
    noteOn(cv, 441.0f, 69, 0.0f, 1.0f);
    Rig rig;
    rig.hold(&rig.sources[2], static_cast<int>(0.3 * (1 << 24))); // 1.2 octaves: floored to 1
    const auto buffer = rig.render(cv, 0, 44100);
    EXPECT_GE(countRisingZeroCrossings(buffer), 881);
    EXPECT_LE(countRisingZeroCrossings(buffer), 882);
}

TEST(OscillatorModulation, StepsAreSkippedWhileBothSourcesAreTheDefault) {
    // The octave and semitone work runs only when either source is not the
    // default source, whose waveform is kOff. A default source holding a
    // value (which the engine's never does) is therefore ignored.
    ControlVoltage cv;
    noteOn(cv, 441.0f, 69, 0.0f, 1.0f);
    Rig rig;
    std::fill(rig.sources[2].m_pnOutput, rig.sources[2].m_pnOutput + kBlock,
              static_cast<int>(0.3 * (1 << 24)));
    const auto buffer = rig.render(cv, 0, 44100);
    EXPECT_GE(countRisingZeroCrossings(buffer), 440);
    EXPECT_LE(countRisingZeroCrossings(buffer), 441);
}

TEST(OscillatorModulation, FrequencyModulationFollowsTheInput) {
    ControlVoltage plainVoice;
    noteOn(plainVoice, 441.0f, 69, 0.0f, 1.0f);
    ControlVoltage modulatedVoice = plainVoice;
    Rig plain;
    Rig modulated;
    const auto plainOutput = plain.render(plainVoice, 0, 44100);
    const auto modulatedOutput = modulated.render(modulatedVoice, 0, 44100, 1.0f, 1 << 23, (1 << 24) - 1);
    EXPECT_NEAR(countRisingZeroCrossings(modulatedOutput), 661, 2);
    EXPECT_NEAR(countRisingZeroCrossings(plainOutput), 441, 1);
}

TEST(OscillatorModulation, PhaseModeShiftsTheTableIndex) {
    ControlVoltage shiftedVoice;
    noteOn(shiftedVoice, 441.0f, 69, 0.0f, 1.0f);
    ControlVoltage offsetVoice = shiftedVoice;

    // Input 0x400001 at unity amount gives a product of exactly 0x400000: a
    // quarter of the table, 1024 entries, with the fraction untouched.
    Rig modulated;
    modulated.osc.SetModulationMode(Oscillator::kPhaseModulation);
    Rig offset;
    offset.osc.m_nPhaseOffset = 1024;

    const auto shifted = modulated.render(shiftedVoice, 0, 4096, 1.0f, 0x400001, 1 << 24);
    const auto reference = offset.render(offsetVoice, 0, 4096);

    EXPECT_EQ(shifted, reference);
    EXPECT_EQ(shiftedVoice.m_anPhase[0], offsetVoice.m_anPhase[0]);
}

TEST(OscillatorModulation, AmplitudeModeScalesTheSample) {
    ControlVoltage plainVoice;
    noteOn(plainVoice, 441.0f, 69, 0.0f, 1.0f);
    ControlVoltage modulatedVoice = plainVoice;

    // A constant -0.5 at unity amount halves the output and leaves the pitch.
    Rig plain;
    Rig modulated;
    modulated.osc.SetModulationMode(Oscillator::kAmplitudeModulation);

    const auto reference = plain.render(plainVoice, 0, 4096);
    const auto scaled = modulated.render(modulatedVoice, 0, 4096, 1.0f, -(1 << 23), 1 << 24);

    for (std::size_t index = 0; index < scaled.size(); ++index) {
        const auto expected = static_cast<int>(
            (static_cast<std::int64_t>(reference[index]) * ((1 << 24) - 1 - (1 << 23))) >> 24);
        ASSERT_EQ(scaled[index], expected);
    }
    EXPECT_EQ(plainVoice.m_anPhase[0], modulatedVoice.m_anPhase[0]);
}

TEST(OscillatorModulation, OtherModesLeavePhaseAndOutputAlone) {
    ControlVoltage cv;
    noteOn(cv, 441.0f, 69, 0.0f, 1.0f);
    Rig rig;
    rig.osc.SetModulationMode(static_cast<Oscillator::ModulationMode>(3));
    std::vector<int> buffer(64, 123);
    std::vector<int> fmInput(64, 0);
    rig.osc.GenerateSignal(&cv, 0, buffer.data(), 64, fmInput.data(), 0, 1.0f);
    for (const int value : buffer) {
        EXPECT_EQ(value, 123);
    }
    EXPECT_EQ(cv.m_anPhase[0], 0u);
}

TEST(OscillatorTypes, SetOscillatorTypeFollowsTheEnginesSwitch) {
    Oscillator osc;
    osc.SetOscillatorType(Oscillator::kSawtoothHQ);
    EXPECT_TRUE(osc.m_bHQ);
    EXPECT_EQ(osc.m_psTable, Oscillator::m_psHQSawtoothTable);
    // Noise leaves the table and only clears the HQ flag.
    osc.SetOscillatorType(Oscillator::kNoise);
    EXPECT_FALSE(osc.m_bHQ);
    EXPECT_EQ(osc.m_psTable, Oscillator::m_psHQSawtoothTable);
    // Outside 0..8 only the type is stored.
    osc.SetOscillatorType(static_cast<Oscillator::OscillatorType>(12));
    EXPECT_EQ(static_cast<int>(osc.m_nType), 12);
    EXPECT_EQ(osc.m_psTable, Oscillator::m_psHQSawtoothTable);
}

TEST(OscillatorTables, AreSharedAndCounted) {
    const int before = Oscillator::m_nWaveTableRefCount;
    {
        Oscillator a;
        Oscillator b;
        EXPECT_EQ(Oscillator::m_nWaveTableRefCount, before + 2);
        EXPECT_NE(Oscillator::m_psSinewaveTable, nullptr);
    }
    EXPECT_EQ(Oscillator::m_nWaveTableRefCount, before);
}
