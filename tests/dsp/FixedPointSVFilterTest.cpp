#include "dsp/FixedPointSVFilter.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "dsp/ADSR.h"

namespace {

constexpr int kUnit = 1 << 20;
constexpr double kSampleRate = 44100.0;
constexpr int kFullScale = 16777215;

ADSR sustainedEnvelope(float sustain) {
    ADSR envelope;
    envelope.m_fAttack = 0.0f;
    envelope.m_fDecay = 0.0f;
    envelope.m_fSustain = sustain;
    envelope.m_fRelease = 0.1f;
    envelope.OnParamModified();
    return envelope;
}

ControlVoltage heldVoice(float hertz, float tracking = 0.0f) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(hertz, 60, tracking, 1.0f);
    // Past the attack and decay minimums so the envelope sits at sustain.
    voltage.advance(1000);
    return voltage;
}

// A filter wired the way SubSynth wires a voice: an envelope, a modulation
// source left at zero, and a held note.
struct Rig {
    explicit Rig(float sustain = 1.0f, bool extended = true)
        : filter(extended), envelope(sustainedEnvelope(sustain)), voltage(heldVoice(500.0f)) {
        filter.m_pEnvelope = &envelope;
        filter.m_pModulation = &modulation;
    }

    // Run one control block (one sample) and leave the coefficients behind.
    void controlBlock() {
        int sample = 0;
        filter.ProcessCV(&voltage, &sample, 1, 1);
    }

    FixedPointSVFilter filter;
    ADSR envelope;
    LFO modulation;
    ControlVoltage voltage;
};

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
double response(int mode, float cutoff, float resonance, double frequency) {
    Rig rig;
    rig.filter.m_nMode = mode;
    rig.filter.m_fCutoff = cutoff;
    rig.filter.m_fResonance = resonance;

    auto buffer = sine(frequency, 22050);
    for (std::size_t offset = 0; offset < buffer.size(); offset += 128) {
        const auto count = static_cast<unsigned int>(std::min<std::size_t>(128, buffer.size() - offset));
        rig.filter.ProcessCV(&rig.voltage, buffer.data() + offset, count, 1);
    }

    double sum = 0.0;
    const std::size_t start = buffer.size() / 2;
    for (std::size_t index = start; index < buffer.size(); ++index) {
        const double value = static_cast<double>(buffer[index]) / kUnit;
        sum += value * value;
    }
    return std::sqrt(sum / static_cast<double>(buffer.size() - start));
}

// The engine's own float arithmetic for the input gain, truncated by vcvt.
int engineGain(float base, float term) { return static_cast<int>((base + term) * 16777215.0f); }

} // namespace

TEST(FixedPointSVFilterTables, MatchTheConstructorFormula) {
    const FixedPointSVFilter filter(true);
    EXPECT_EQ(filter.m_anCutoffTable[0], 482443);
    EXPECT_EQ(filter.m_anCutoffTable[128], 2845005);
    EXPECT_EQ(filter.m_anCutoffTable[255], 16546238);
}

TEST(FixedPointSVFilterTables, ExtendedResonanceStartsLowerThanStandard) {
    const FixedPointSVFilter extended(true);
    const FixedPointSVFilter standard(false);
    // 0.5^1.5 against 0.5^0, both in Q24.
    EXPECT_EQ(extended.m_anDampingTable[0], 5931641);
    EXPECT_EQ(standard.m_anDampingTable[0], kFullScale);
    // Damping falls as the control rises, which is what sharpens the filter.
    EXPECT_LT(extended.m_anDampingTable[255], extended.m_anDampingTable[0]);
}

TEST(FixedPointSVFilterConstruction, StartsAsTheEngineDoes) {
    const FixedPointSVFilter extended(true);
    EXPECT_EQ(extended.m_nMode, 0);
    EXPECT_EQ(extended.m_pInput, nullptr);
    EXPECT_EQ(extended.m_pModulation, nullptr);
    EXPECT_EQ(extended.m_pEnvelope, &extended.m_DefaultEnvelope);
    EXPECT_EQ(extended.m_fCutoff, 1.0f);
    EXPECT_EQ(extended.m_fResonance, 0.0f);
    // Coefficients resolved from those bases, as at 0x948a4-0x9494c.
    EXPECT_EQ(extended.m_nCutoff, extended.m_anCutoffTable[255]);
    EXPECT_EQ(extended.m_nDamping, extended.m_anDampingTable[0]);
    EXPECT_EQ(extended.m_nInputGain, engineGain(1.0f, -0.0f));

    const FixedPointSVFilter standard(false);
    EXPECT_EQ(standard.m_nInputGain, engineGain(1.25f, -0.0f));
}

TEST(FixedPointSVFilterCoefficients, CutoffBelowTheFloorIsPinnedToEntryTwelve) {
    FixedPointSVFilter filter(true);
    for (const float cutoff : {0.0f, 0.025f, 0.05f}) {
        filter.UpdateCoefficients(cutoff, 0.0f);
        // 0.05 * 255 truncates to 12, so the floor is the entry the threshold
        // itself selects.
        EXPECT_EQ(filter.m_nCutoff, filter.m_anCutoffTable[12]) << cutoff;
    }
    filter.UpdateCoefficients(2.0f, 0.0f);
    EXPECT_EQ(filter.m_nCutoff, filter.m_anCutoffTable[255]);
}

TEST(FixedPointSVFilterCoefficients, ExtendedGainRunsFromUnityDown) {
    FixedPointSVFilter filter(true);
    filter.UpdateCoefficients(1.0f, 0.0f);
    EXPECT_EQ(filter.m_nInputGain, kFullScale);
    filter.UpdateCoefficients(1.0f, 0.9f); // SubSynth's slider limit
    EXPECT_EQ(filter.m_nInputGain, engineGain(1.0f, 0.9f * -0.75f));
    filter.UpdateCoefficients(1.0f, 2.0f); // above one the term pins to -0.75
    EXPECT_EQ(filter.m_nInputGain, engineGain(1.0f, -0.75f));
}

TEST(FixedPointSVFilterCoefficients, StandardGainStartsAboveUnity) {
    FixedPointSVFilter filter(false);
    filter.UpdateCoefficients(1.0f, 0.0f);
    EXPECT_EQ(filter.m_nInputGain, engineGain(1.25f, -0.0f));
    filter.UpdateCoefficients(1.0f, 1.0f);
    EXPECT_EQ(filter.m_nInputGain, engineGain(1.25f, -0.75f));
}

TEST(FixedPointSVFilterModes, OtherModesLeaveEverythingAlone) {
    for (const int mode : {0, 7}) {
        Rig rig(0.5f);
        rig.filter.m_nMode = mode;
        rig.filter.m_fCutoff = 0.3f;
        rig.filter.m_fResonance = 0.6f;
        const int gain = rig.filter.m_nInputGain;
        const int cutoff = rig.filter.m_nCutoff;
        const int damping = rig.filter.m_nDamping;

        std::vector<int> buffer(32, kUnit);
        rig.filter.ProcessCV(&rig.voltage, buffer.data(), 32, 1);

        EXPECT_EQ(rig.filter.m_nInputGain, gain);
        EXPECT_EQ(rig.filter.m_nCutoff, cutoff);
        EXPECT_EQ(rig.filter.m_nDamping, damping);
        for (const int value : buffer) {
            EXPECT_EQ(value, kUnit);
        }
    }
}

TEST(FixedPointSVFilterModes, LowPassKeepsLowFrequenciesAndRemovesHigh) {
    EXPECT_GT(response(1, 0.5f, 0.2f, 500.0), 0.5);
    EXPECT_LT(response(1, 0.5f, 0.2f, 8000.0), 0.05);
}

TEST(FixedPointSVFilterModes, HighPassKeepsHighFrequenciesAndCutsLow) {
    // input - low. The integrators leak and the input gain sits below unity
    // with resonance, so the low end is attenuated rather than removed.
    const double low = response(2, 0.5f, 0.2f, 100.0);
    const double high = response(2, 0.5f, 0.2f, 2000.0);
    EXPECT_GT(high, 0.5);
    EXPECT_LT(low, high * 0.25);
}

TEST(FixedPointSVFilterModes, BandPassPeaksBetweenTheExtremes) {
    const double low = response(3, 0.5f, 0.2f, 100.0);
    const double middle = response(3, 0.5f, 0.2f, 500.0);
    const double high = response(3, 0.5f, 0.2f, 8000.0);
    EXPECT_GT(middle, low);
    EXPECT_GT(middle, high);
}

TEST(FixedPointSVFilterModes, StaysBoundedAtFullResonanceAndCutoff) {
    Rig rig;
    rig.filter.m_nMode = 1;
    rig.filter.m_fCutoff = 1.0f;
    rig.filter.m_fResonance = 1.0f;
    auto buffer = sine(1000.0, 44100);
    for (std::size_t offset = 0; offset < buffer.size(); offset += 128) {
        const auto count = static_cast<unsigned int>(std::min<std::size_t>(128, buffer.size() - offset));
        rig.filter.ProcessCV(&rig.voltage, buffer.data() + offset, count, 1);
    }
    for (const int value : buffer) {
        EXPECT_LT(std::abs(value), 64 * kUnit);
    }
}

TEST(FixedPointSVFilterChannels, StereoFiltersBothChannelsIndependently) {
    Rig rig;
    rig.filter.m_nMode = 1;
    rig.filter.m_fCutoff = 0.5f;
    rig.filter.m_fResonance = 0.2f;

    // Left carries the signal, right is silent.
    std::vector<int> buffer(128 * 2, 0);
    for (std::size_t index = 0; index < 128; ++index) {
        buffer[index * 2] = kUnit;
    }
    rig.filter.ProcessCV(&rig.voltage, buffer.data(), 128, 2);

    bool leftMoved = false;
    for (std::size_t index = 0; index < 128; ++index) {
        EXPECT_EQ(buffer[index * 2 + 1], 0);
        leftMoved = leftMoved || buffer[index * 2] != 0;
    }
    EXPECT_TRUE(leftMoved);
    EXPECT_EQ(rig.filter.m_anBand[1], 0);
    EXPECT_EQ(rig.filter.m_anLow[1], 0);
}

TEST(FixedPointSVFilterControlBlock, FullSustainReproducesTheBaseCutoff) {
    Rig rig;
    rig.filter.m_fCutoff = 0.6f;
    rig.filter.m_nMode = 1;
    rig.controlBlock();
    FixedPointSVFilter plain(true);
    plain.UpdateCoefficients(0.6f, 0.0f);
    EXPECT_EQ(rig.filter.m_nCutoff, plain.m_nCutoff);
}

TEST(FixedPointSVFilterControlBlock, InvertFlipsTheEnvelope) {
    Rig rig;
    rig.filter.m_nMode = 1;
    rig.filter.m_fCutoff = 0.8f;
    rig.filter.m_bInvertEnvelope = true;
    rig.controlBlock();
    // 1 - 1 = 0, below the floor.
    EXPECT_EQ(rig.filter.m_nCutoff, rig.filter.m_anCutoffTable[12]);
}

TEST(FixedPointSVFilterControlBlock, KeyboardTrackingReachesTheCutoff) {
    Rig rig;
    rig.voltage = heldVoice(2000.0f, 1.0f); // two octaves above the pivot: x2
    rig.filter.m_nMode = 1;
    rig.filter.m_fCutoff = 0.3f;
    rig.controlBlock();
    FixedPointSVFilter plain(true);
    plain.UpdateCoefficients(0.6f, 0.0f);
    EXPECT_EQ(rig.filter.m_nCutoff, plain.m_nCutoff);
}

TEST(FixedPointSVFilterControlBlock, ModulationLiftsTheCutoffMultiplicatively) {
    Rig rig;
    rig.modulation.output()[0] = kFullScale; // +1.0 in Q24 roughly doubles it
    rig.filter.m_nMode = 1;
    rig.filter.m_fCutoff = 0.3f;
    rig.controlBlock();
    const float lift = 1.0f + static_cast<float>(kFullScale) * (1.0f / 16777215.0f);
    FixedPointSVFilter plain(true);
    plain.UpdateCoefficients(lift * 0.3f, 0.0f);
    EXPECT_EQ(rig.filter.m_nCutoff, plain.m_nCutoff);
}

TEST(FixedPointSVFilterControlBlock, ReleaseUsesTheReleasePosition) {
    Rig rig;
    rig.voltage.noteOff();
    rig.voltage.advance(20000); // well past the release: the envelope reads zero
    rig.filter.m_nMode = 1;
    rig.filter.m_fCutoff = 0.9f;
    rig.controlBlock();
    EXPECT_EQ(rig.filter.m_nCutoff, rig.filter.m_anCutoffTable[12]);
}

TEST(FixedPointSVFilterControlBlock, RefreshesEverySixteenSamples) {
    Rig rig;
    rig.filter.m_nMode = 1;
    rig.filter.m_fCutoff = 0.3f;
    // The modulation doubles the cutoff from sample 16 on.
    for (int index = 16; index < 64; ++index) {
        rig.modulation.output()[index] = kFullScale;
    }
    std::vector<int> buffer(16, kUnit);
    rig.filter.ProcessCV(&rig.voltage, buffer.data(), 16, 1);
    const int closed = rig.filter.m_nCutoff;
    Rig longer;
    longer.filter.m_nMode = 1;
    longer.filter.m_fCutoff = 0.3f;
    for (int index = 16; index < 64; ++index) {
        longer.modulation.output()[index] = kFullScale;
    }
    std::vector<int> more(17, kUnit);
    longer.filter.ProcessCV(&longer.voltage, more.data(), 17, 1);
    EXPECT_GT(longer.filter.m_nCutoff, closed);
}
