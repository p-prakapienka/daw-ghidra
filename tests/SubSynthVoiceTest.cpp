#include "components/SubSynthVoice.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {

constexpr unsigned int kBlock = 128;

void startNote(SubSynthVoice &voice, float hertz, float velocity) {
    voice.voltage.noteOn(hertz, 69, 0.0f, velocity);
    voice.level.smoothing = SubSynthVoice::kLevelSmoothing;
    voice.amplitudeEnvelope.OnParamModified();
    voice.filterEnvelope.OnParamModified();
}

// Render in engine-sized blocks, adding into the returned buffer.
std::vector<int> render(SubSynthVoice &voice, SubSynthVoice::Shared &shared, unsigned int total,
                        int initial = 0) {
    std::vector<int> output(total, initial);
    for (unsigned int offset = 0; offset < total; offset += kBlock) {
        voice.process(shared, output.data() + offset, std::min(kBlock, total - offset));
    }
    return output;
}

int peak(const std::vector<int> &samples, std::size_t from) {
    int result = 0;
    for (std::size_t index = from; index < samples.size(); ++index) {
        result = std::max(result, std::abs(samples[index]));
    }
    return result;
}

int countRisingZeroCrossings(const std::vector<int> &samples) {
    int crossings = 0;
    for (std::size_t index = 1; index < samples.size(); ++index) {
        if (samples[index - 1] < 0 && samples[index] >= 0) {
            ++crossings;
        }
    }
    return crossings;
}

} // namespace

TEST(SubSynthMix, CrossfadeRunsFromTheIntoBufferToTheFromBuffer) {
    const std::vector<int> from = {1 << 20, -(1 << 20), 0};
    std::vector<int> none = {1000, 2000, 3000};
    std::vector<int> half = none;
    std::vector<int> full = none;

    SubSynthVoice::crossfade(from.data(), none.data(), 0, 3);
    SubSynthVoice::crossfade(from.data(), half.data(), 0x800000, 3);
    SubSynthVoice::crossfade(from.data(), full.data(), 0xFFFFFF, 3);

    EXPECT_EQ(none, (std::vector<int>{1000, 2000, 3000}));
    EXPECT_EQ(half[0], 1000 + ((1 << 20) - 1000) / 2);
    EXPECT_NEAR(full[0], 1 << 20, 1);
    EXPECT_NEAR(full[1], -(1 << 20), 1);
}

TEST(SubSynthControls, ScaleToQ24ByTruncation) {
    SubSynthVoice::Shared shared;
    EXPECT_EQ(shared.mixQ24, 8388607); // the constructor's 0.5

    shared.setMix(1.0f);
    EXPECT_EQ(shared.mixQ24, 16777215);
    shared.setModulationAmount(0.25f);
    EXPECT_EQ(shared.modulationAmountQ24, 4194303);

    EXPECT_EQ(SubSynthVoice::levelTarget(1.0f, 1.0f), 16777215);
    EXPECT_EQ(SubSynthVoice::levelTarget(1.0f, 0.5f), 8388607);
}

TEST(SubSynthControls, Oscillator2WaveformZeroIsOff) {
    SubSynthVoice::Shared shared;
    EXPECT_FALSE(shared.oscillator2Enabled);

    shared.setOscillator2Waveform(3);
    EXPECT_TRUE(shared.oscillator2Enabled);
    EXPECT_EQ(shared.oscillator2.type(), Oscillator::Type::Sawtooth);

    shared.setOscillator2Waveform(0);
    EXPECT_FALSE(shared.oscillator2Enabled);
    EXPECT_EQ(shared.oscillator2.type(), Oscillator::Type::Sawtooth);
}

TEST(SubSynthRouting, DefaultsToNoModulation) {
    LFO lfo1;
    LFO lfo2;
    const SubSynthRouting routing =
        SubSynthRouting::connect(lfo1, LFO::Target::None, lfo2, LFO::Target::None);

    EXPECT_EQ(routing.oscillator1.vibrato, nullptr);
    EXPECT_EQ(routing.oscillator2.phase, nullptr);
    EXPECT_EQ(routing.cutoff, nullptr);
    EXPECT_EQ(routing.volume, nullptr);
}

TEST(SubSynthRouting, LFO2WinsASharedTarget) {
    LFO lfo1;
    LFO lfo2;
    const SubSynthRouting routing =
        SubSynthRouting::connect(lfo1, LFO::Target::Cutoff, lfo2, LFO::Target::Cutoff);
    EXPECT_EQ(routing.cutoff, lfo2.output());
}

TEST(SubSynthRouting, EachTargetReachesItsInput) {
    LFO lfo1;
    LFO lfo2;
    SubSynthRouting routing =
        SubSynthRouting::connect(lfo1, LFO::Target::BothPitch, lfo2, LFO::Target::Volume);
    EXPECT_EQ(routing.oscillator1.vibrato, lfo1.output());
    EXPECT_EQ(routing.oscillator2.vibrato, lfo1.output());
    EXPECT_EQ(routing.volume, lfo2.output());

    routing = SubSynthRouting::connect(lfo1, LFO::Target::Oscillator2Semitones, lfo2,
                                       LFO::Target::Oscillator1Modulation);
    EXPECT_EQ(routing.oscillator2.semitones, lfo1.output());
    EXPECT_EQ(routing.oscillator1.fmDepth, lfo2.output());
    EXPECT_EQ(routing.oscillator1.semitones, nullptr);
}

TEST(SubSynthRouting, OctaveTargetStartsTheLFOHalfACycleIn) {
    LFO shifted;
    LFO plain;
    for (LFO *lfo : {&shifted, &plain}) {
        lfo->setRate(5.0f);
        lfo->SetBPM(120.0f);
        lfo->setWaveform(LFO::Waveform::Triangle);
    }
    SubSynthRouting::connect(shifted, LFO::Target::Oscillator2Octave, plain,
                             LFO::Target::Oscillator2Semitones);

    shifted.generate(0, 1);
    const int shiftedStart = shifted.output()[0];
    plain.generate(0, 1);
    const int plainStart = plain.output()[0];

    // The triangle starts at its top; half a cycle in, it is at the bottom.
    EXPECT_GT(plainStart, 0);
    EXPECT_LT(shiftedStart, 0);
}

TEST(SubSynthVoice, AddsIntoTheOutputBuffer) {
    SubSynthVoice::Shared shared;
    SubSynthVoice clean;
    SubSynthVoice offset;
    startNote(clean, 441.0f, 1.0f);
    startNote(offset, 441.0f, 1.0f);

    const std::vector<int> fromZero = render(clean, shared, 512);
    const std::vector<int> fromHundred = render(offset, shared, 512, 100);

    for (std::size_t index = 0; index < fromZero.size(); ++index) {
        ASSERT_EQ(fromHundred[index], fromZero[index] + 100);
    }
}

TEST(SubSynthVoice, PlaysOscillator1AtTheNotePitch) {
    SubSynthVoice::Shared shared;
    SubSynthVoice voice;
    startNote(voice, 441.0f, 1.0f);

    const std::vector<int> output = render(voice, shared, 44100);
    EXPECT_NEAR(countRisingZeroCrossings(output), 441, 1);
}

TEST(SubSynthVoice, LevelFollowsVelocityThroughTheSmoother) {
    SubSynthVoice::Shared shared;
    SubSynthVoice loud;
    SubSynthVoice soft;
    startNote(loud, 441.0f, 1.0f);
    startNote(soft, 441.0f, 0.5f);

    const std::vector<int> loudOutput = render(loud, shared, 4096);
    const std::vector<int> softOutput = render(soft, shared, 4096);

    EXPECT_NEAR(static_cast<double>(peak(softOutput, 2048)) / peak(loudOutput, 2048), 0.5, 0.01);

    // The smoother floors its Q24 multiply, so it settles a few units below
    // the target rather than on it.
    EXPECT_LT(soft.level.current, soft.level.target);
    EXPECT_LE(soft.level.target - soft.level.current, 16);

    // One step of the 0.9 smoother from zero lands a tenth of the way.
    SubSynthVoice fresh;
    startNote(fresh, 441.0f, 1.0f);
    std::vector<int> one(1, 0);
    fresh.process(shared, one.data(), 1);
    EXPECT_NEAR(static_cast<double>(fresh.level.current) / fresh.level.target, 0.1, 1e-3);
}

TEST(SubSynthVoice, Oscillator2IsSilentAndInertWhileDisabled) {
    SubSynthVoice::Shared plainShared;
    SubSynthVoice::Shared modulatedShared;
    modulatedShared.setModulationAmount(1.0f);

    SubSynthVoice plain;
    SubSynthVoice modulated;
    startNote(plain, 441.0f, 1.0f);
    startNote(modulated, 441.0f, 1.0f);

    const std::vector<int> plainOutput = render(plain, plainShared, 1024);
    const std::vector<int> modulatedOutput = render(modulated, modulatedShared, 1024);

    for (const int value : modulated.oscillator2Buffer()) {
        ASSERT_EQ(value, 0);
    }
    EXPECT_EQ(plainOutput, modulatedOutput);
}

TEST(SubSynthVoice, FullMixPlaysOscillator2) {
    SubSynthVoice::Shared shared;
    shared.setOscillator2Waveform(1); // sine
    shared.oscillator2.setPitchRatio(2.0f);
    shared.setMix(1.0f);

    SubSynthVoice voice;
    startNote(voice, 441.0f, 1.0f);
    const std::vector<int> output = render(voice, shared, 44100);

    EXPECT_NEAR(countRisingZeroCrossings(output), 882, 1);
}

TEST(SubSynthVoice, ModulationAmountBendsOscillator1WithOscillator2) {
    SubSynthVoice::Shared plainShared;
    plainShared.setOscillator2Waveform(1);
    plainShared.setMix(0.0f);
    SubSynthVoice::Shared modulatedShared;
    modulatedShared.setOscillator2Waveform(1);
    modulatedShared.setMix(0.0f);
    modulatedShared.setModulationAmount(0.5f);

    SubSynthVoice plain;
    SubSynthVoice modulated;
    startNote(plain, 441.0f, 1.0f);
    startNote(modulated, 441.0f, 1.0f);

    const std::vector<int> plainOutput = render(plain, plainShared, 4096);
    const std::vector<int> modulatedOutput = render(modulated, modulatedShared, 4096);

    // Mix 0 keeps oscillator 2 out of the signal, so any difference is the
    // modulator acting on oscillator 1.
    EXPECT_NE(plainOutput, modulatedOutput);
}

TEST(SubSynthVoice, VolumeModulationScalesTheOutput) {
    std::vector<int> halfDown(kBlock, -(1 << 23));

    SubSynthVoice::Shared plainShared;
    SubSynthVoice::Shared quietShared;
    quietShared.routing.volume = halfDown.data();

    SubSynthVoice plain;
    SubSynthVoice quiet;
    startNote(plain, 441.0f, 1.0f);
    startNote(quiet, 441.0f, 1.0f);

    const std::vector<int> plainOutput = render(plain, plainShared, 4096);
    const std::vector<int> quietOutput = render(quiet, quietShared, 4096);

    EXPECT_NEAR(static_cast<double>(peak(quietOutput, 2048)) / peak(plainOutput, 2048), 0.5, 0.01);
}

TEST(SubSynthVoice, EndsWhenTheAmplitudeEnvelopeIsDone) {
    SubSynthVoice::Shared shared;
    SubSynthVoice voice;
    startNote(voice, 441.0f, 1.0f);

    render(voice, shared, 1024);
    EXPECT_TRUE(voice.isActive());

    voice.voltage.noteOff();
    const unsigned int tail =
        voice.amplitudeEnvelope.releaseSamples() + ADSR::kReleaseTailSamples;
    render(voice, shared, tail);
    EXPECT_TRUE(voice.isActive());

    render(voice, shared, kBlock);
    EXPECT_FALSE(voice.isActive());
}
