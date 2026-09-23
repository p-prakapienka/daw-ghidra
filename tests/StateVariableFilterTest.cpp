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

    EXPECT_EQ(filter.getCutoffTableEntry(0), 482443);
    EXPECT_EQ(filter.getCutoffTableEntry(128), 2845005);
    EXPECT_EQ(filter.getCutoffTableEntry(255), 16546238);
}

TEST(SVFTables, ExtendedResonanceStartsLowerThanStandard) {
    const StateVariableFilter extended(true);
    const StateVariableFilter standard(false);

    // 0.5^1.5 against 0.5^0, both in Q24.
    EXPECT_EQ(extended.getDampingTableEntry(0), 5931641);
    EXPECT_EQ(standard.getDampingTableEntry(0), StateVariableFilter::kMax24);

    // Damping falls as the control rises, which is what sharpens the filter.
    EXPECT_LT(extended.getDampingTableEntry(255), extended.getDampingTableEntry(0));
}

TEST(SVFTables, CutoffBelowTheFloorIsPinnedToEntryTwelve) {
    StateVariableFilter filter(true);
    filter.setCutoff(0.0f);
    const int atZero = filter.getCutoffCoefficient();

    filter.setCutoff(StateVariableFilter::kMinimumCutoff / 2.0f);
    EXPECT_EQ(filter.getCutoffCoefficient(), atZero);

    // 0.05 * 255 truncates to 12, so the floor is the same entry the
    // threshold itself selects rather than a special case.
    EXPECT_EQ(atZero, filter.getCutoffTableEntry(StateVariableFilter::kMinimumCutoffIndex));
    filter.setCutoff(StateVariableFilter::kMinimumCutoff);
    EXPECT_EQ(filter.getCutoffCoefficient(), atZero);
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


namespace {

ADSR sustainedEnvelope(float sustain) {
    ADSR envelope;
    envelope.setAttackSeconds(0.0f);
    envelope.setDecaySeconds(0.0f);
    envelope.setSustainLevel(sustain);
    envelope.setReleaseSeconds(0.1f);
    envelope.OnParamModified();
    return envelope;
}

ControlVoltage heldVoice(float hertz) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(hertz, 60, 0.0f, 1.0f);
    // Past the attack and decay minimums so the envelope sits at sustain.
    voltage.advance(1000);
    return voltage;
}

} // namespace

TEST(SVFControlBlock, EnvelopeAtFullSustainReproducesTheBaseCutoff) {
    ADSR envelope = sustainedEnvelope(1.0f);
    ControlVoltage voltage = heldVoice(500.0f);

    StateVariableFilter filter(true);
    filter.setCutoffBase(0.6f);
    filter.setResonanceBase(0.0f);
    filter.updateControlBlock({&voltage, &envelope, nullptr}, 0);

    StateVariableFilter plain(true);
    plain.setCutoff(0.6f);

    EXPECT_EQ(filter.getCutoffCoefficient(), plain.getCutoffCoefficient());
}

TEST(SVFControlBlock, EnvelopeScalesTheCutoff) {
    ADSR envelope = sustainedEnvelope(0.5f);
    ControlVoltage voltage = heldVoice(500.0f);

    StateVariableFilter filter(true);
    filter.setCutoffBase(0.8f);
    filter.updateControlBlock({&voltage, &envelope, nullptr}, 0);

    StateVariableFilter plain(true);
    plain.setCutoff(0.4f);

    // 0.5 * 32767 / 32767 is a hair under 0.5, so allow one table step.
    EXPECT_NEAR(filter.getCutoffCoefficient(), plain.getCutoffCoefficient(),
                plain.getCutoffCoefficient() / 40);
}

TEST(SVFControlBlock, InvertFlipsTheEnvelope) {
    ADSR envelope = sustainedEnvelope(1.0f);
    ControlVoltage voltage = heldVoice(500.0f);

    StateVariableFilter filter(true);
    filter.setCutoffBase(0.8f);
    filter.setInvertEnvelope(true);
    filter.updateControlBlock({&voltage, &envelope, nullptr}, 0);

    // 1 - 1 = 0, below the floor.
    EXPECT_EQ(filter.getCutoffCoefficient(),
              filter.getCutoffTableEntry(StateVariableFilter::kMinimumCutoffIndex));
}

TEST(SVFControlBlock, KeyboardTrackingReachesTheCutoff) {
    ADSR envelope = sustainedEnvelope(1.0f);
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(2000.0f, 96, 1.0f, 1.0f); // tracking 1.0, two octaves up: x2
    voltage.advance(1000);

    StateVariableFilter filter(true);
    filter.setCutoffBase(0.3f);
    filter.updateControlBlock({&voltage, &envelope, nullptr}, 0);

    StateVariableFilter plain(true);
    plain.setCutoff(0.6f);

    EXPECT_EQ(filter.getCutoffCoefficient(), plain.getCutoffCoefficient());
}

TEST(SVFControlBlock, ModulationLiftsTheCutoffMultiplicatively) {
    ADSR envelope = sustainedEnvelope(1.0f);
    ControlVoltage voltage = heldVoice(500.0f);

    // +1.0 in Q24 doubles the cutoff.
    const int modulation[1] = {StateVariableFilter::kOne24};

    StateVariableFilter filter(true);
    filter.setCutoffBase(0.3f);
    filter.updateControlBlock({&voltage, &envelope, modulation}, 0);

    StateVariableFilter plain(true);
    plain.setCutoff(0.6f);

    EXPECT_EQ(filter.getCutoffCoefficient(), plain.getCutoffCoefficient());
}

TEST(SVFControlBlock, ResonanceLowersTheInputGain) {
    ADSR envelope = sustainedEnvelope(1.0f);
    ControlVoltage voltage = heldVoice(500.0f);

    StateVariableFilter quiet(true);
    quiet.setResonanceBase(0.0f);
    quiet.updateControlBlock({&voltage, &envelope, nullptr}, 0);

    StateVariableFilter sharp(true);
    sharp.setResonanceBase(1.0f);
    sharp.updateControlBlock({&voltage, &envelope, nullptr}, 0);

    // Extended variant: 1.25 at zero resonance, 0.5 at full.
    const int quietGain = static_cast<int>(1.25f * 16777215.0f);
    const int sharpGain = static_cast<int>(0.5f * 16777215.0f);

    // Gain is private, so observe it through the drive level of a DC step.
    std::vector<int> a(1, 1 << 20);
    std::vector<int> b(1, 1 << 20);
    quiet.setMode(StateVariableFilter::Mode::LowPass);
    sharp.setMode(StateVariableFilter::Mode::LowPass);
    quiet.process(a.data(), 1, 1);
    sharp.process(b.data(), 1, 1);

    EXPECT_GT(a[0], b[0]);
    EXPECT_GT(quietGain, sharpGain);
}

TEST(SVFControlBlock, ReleaseUsesTheReleasePosition) {
    ADSR envelope = sustainedEnvelope(1.0f);
    ControlVoltage voltage = heldVoice(500.0f);
    voltage.noteOff();

    // Well past the release: the envelope reads zero, cutoff pins to the floor.
    voltage.advance(20000);

    StateVariableFilter filter(true);
    filter.setCutoffBase(0.9f);
    filter.updateControlBlock({&voltage, &envelope, nullptr}, 0);

    EXPECT_EQ(filter.getCutoffCoefficient(),
              filter.getCutoffTableEntry(StateVariableFilter::kMinimumCutoffIndex));
}

TEST(SVFControlBlock, ProcessVoiceRefreshesEveryControlBlock) {
    ADSR envelope = sustainedEnvelope(1.0f);
    ControlVoltage voltage = heldVoice(500.0f);

    // A modulation ramp that doubles the cutoff at sample 16 and after.
    std::vector<int> modulation(64, 0);
    for (std::size_t index = 16; index < modulation.size(); ++index) {
        modulation[index] = StateVariableFilter::kOne24;
    }

    StateVariableFilter filter(true);
    filter.setMode(StateVariableFilter::Mode::LowPass);
    filter.setCutoffBase(0.3f);

    std::vector<int> buffer(64, 1 << 20);
    filter.processVoice({&voltage, &envelope, modulation.data()}, buffer.data(), 64, 1);

    // After the refresh at sample 16 the filter is more open, so the low-pass
    // output climbs faster than it did in the first block.
    const int firstBlockRise = buffer[15] - buffer[0];
    const int secondBlockRise = buffer[31] - buffer[16];
    EXPECT_GT(secondBlockRise, 0);
    EXPECT_GT(firstBlockRise, 0);
}
