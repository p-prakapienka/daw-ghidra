#include "machines/subsynth/SubSynthControls.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <string>

#include "machines/subsynth/SubSynth.h"
#include "machines/subsynth/SubSynthArchive.h"

namespace {

struct Rig {
    std::unique_ptr<SubSynth> synth = std::make_unique<SubSynth>();
    SubSynthControls controls{*synth};
};

float cents(float value) { return std::pow(2.0f, value / 1200.0f * 12.0f / 12.0f); }

} // namespace

TEST(SubSynthControlTable, NamesMatchGetControlIDFromName) {
    // The 30 comparisons of GetControlIDFromName, in its order.
    const std::pair<const char *, int> expected[] = {
        {"osc1_waveform", 1},     {"osc_mix", 2},          {"osc_modulation", 31},
        {"osc_bend", 3},          {"osc2_waveform", 4},    {"osc2_phase", 5},
        {"osc2_octave", 6},       {"osc2_semis", 7},       {"osc2_cents", 8},
        {"filter_type", 19},      {"filter_cutoff", 17},   {"filter_resonance", 18},
        {"filter_kbtrack", 20},   {"filter_attack", 21},   {"filter_decay", 22},
        {"filter_sustain", 23},   {"filter_release", 24},  {"lfo1_target", 11},
        {"lfo1_waveform", 10},    {"lfo1_rate", 12},       {"lfo1_depth", 13},
        {"lfo2_target", 14},      {"lfo2_rate", 15},       {"lfo2_depth", 16},
        {"volume_attack", 25},    {"volume_decay", 26},    {"volume_sustain", 27},
        {"volume_release", 28},   {"volume_out", 29},      {"pitch_bend", 32},
    };
    for (const auto &[name, id] : expected) {
        EXPECT_EQ(SubSynthControls::idFromName(name), id) << name;
    }
    EXPECT_EQ(SubSynthControls::idFromName("osc1_cents"), -1);
    EXPECT_EQ(SubSynthControls::idFromName(""), -1);
}

TEST(SubSynthControlTable, SavedIdsSkipNineAndThirty) {
    std::set<int> saved;
    for (const auto &control : SubSynthControls::table()) {
        if (control.saved) {
            saved.insert(control.id);
        }
    }
    EXPECT_EQ(saved.size(), 30u);
    EXPECT_EQ(saved.count(9), 0u);
    EXPECT_EQ(saved.count(30), 0u);
    EXPECT_EQ(SubSynthControls::find(9), nullptr);
    EXPECT_EQ(SubSynthControls::find(30), nullptr);
    ASSERT_NE(SubSynthControls::find(32), nullptr);
    EXPECT_FALSE(SubSynthControls::find(32)->saved);
}

TEST(SubSynthControlTable, KnobsClampAndIntegerKnobsRound) {
    Rig rig;
    rig.controls.set(6, 2.6f);
    EXPECT_EQ(rig.controls.position(6), 3.0f);
    rig.controls.set(6, 9.0f);
    EXPECT_EQ(rig.controls.position(6), 4.0f);
    rig.controls.set(12, -1.0f);
    EXPECT_EQ(rig.controls.position(12), 0.0f);
    // UIKnob::SetValue keeps the maximum for anything not at or below it.
    rig.controls.set(2, std::numeric_limits<float>::quiet_NaN());
    EXPECT_EQ(rig.controls.position(2), 1.0f);
}

TEST(SubSynthControlTable, SquareKnobsReachTheEngineSquared) {
    Rig rig;
    rig.controls.set(25, 1.5f);
    EXPECT_EQ(rig.controls.position(25), 1.5f);
    EXPECT_EQ(rig.controls.value(25), 2.25f);
    for (const auto &voice : rig.synth->voices) {
        EXPECT_EQ(voice.amplitudeEnvelope.m_fAttack, 2.25f);
    }
    // Sustain is linear.
    rig.controls.set(23, 0.5f);
    EXPECT_EQ(rig.synth->voices[15].filterEnvelope.m_fSustain, 0.5f);
}

TEST(SubSynthControlTable, SelectorsTruncateAndClamp) {
    Rig rig;
    rig.controls.set(19, 6.9f);
    EXPECT_EQ(rig.controls.position(19), 6.0f);
    rig.controls.set(19, 9.0f);
    EXPECT_EQ(rig.controls.position(19), 6.0f);
    rig.controls.set(19, -2.0f);
    EXPECT_EQ(rig.controls.position(19), 0.0f);
}

TEST(SubSynthControlTable, UnmodelledIdsChangeNothing) {
    Rig rig;
    EXPECT_FALSE(rig.controls.set(9, 1.0f));
    EXPECT_FALSE(rig.controls.set(40, 1.0f));
    EXPECT_FALSE(rig.controls.set(-1, 1.0f));
    EXPECT_FALSE(rig.controls.set(1000, 1.0f));
}

TEST(SubSynthControlTable, OscillatorTwoPitchReadsAllThreeKnobs) {
    Rig rig;
    rig.controls.set(6, 1.0f);
    rig.controls.set(7, 7.0f);
    rig.controls.set(8, 10.0f);
    float ratio = std::pow(2.0f, 1.0f * 12.0f / 12.0f);
    ratio *= std::pow(2.0f, 7.0f / 12.0f);
    ratio *= cents(10.0f);
    EXPECT_EQ(rig.synth->shared.oscillator2.pitchRatio(), ratio);
    EXPECT_EQ(rig.synth->shared.oscillator1.pitchRatio(), 1.0f);
}

TEST(SubSynthControlTable, CentsModeDetunesOscillatorOneDown) {
    Rig rig;
    rig.controls.set(8, 20.0f);
    rig.controls.setCentsMode(1);
    EXPECT_EQ(rig.synth->shared.oscillator1.pitchRatio(), cents(-20.0f));

    // The cents knob refreshes oscillator 1 in mode 1; the octave does not.
    rig.controls.set(8, 30.0f);
    EXPECT_EQ(rig.synth->shared.oscillator1.pitchRatio(), cents(-30.0f));
    rig.synth->shared.oscillator1.setPitchRatio(1.5f);
    rig.controls.set(6, 1.0f);
    EXPECT_EQ(rig.synth->shared.oscillator1.pitchRatio(), 1.5f);

    rig.controls.setCentsMode(0);
    EXPECT_EQ(rig.synth->shared.oscillator1.pitchRatio(), 1.0f);
}

TEST(SubSynthControlTable, WaveformValuesSelectTypes) {
    Rig rig;
    rig.controls.set(1, 6.0f);
    EXPECT_EQ(rig.synth->shared.oscillator1.type(), Oscillator::Type::Noise);
    // Outside 0..8 the engine keeps the playing table.
    rig.controls.set(1, 12.0f);
    EXPECT_EQ(rig.synth->shared.oscillator1.type(), Oscillator::Type::Noise);

    rig.controls.set(4, 3.0f);
    EXPECT_TRUE(rig.synth->shared.oscillator2Enabled);
    EXPECT_EQ(rig.synth->shared.oscillator2.type(), Oscillator::Type::Sawtooth);
    rig.controls.set(4, 0.0f);
    EXPECT_FALSE(rig.synth->shared.oscillator2Enabled);
}

TEST(SubSynthControlTable, FilterTypeSplitsModeAndInvert) {
    Rig rig;
    rig.controls.set(19, 5.0f); // Inv. HP
    for (const auto &voice : rig.synth->voices) {
        EXPECT_EQ(voice.filter.m_nMode, 2);
        EXPECT_TRUE(voice.filter.m_bInvertEnvelope);
    }
    rig.controls.set(19, 2.0f); // HighPass
    EXPECT_EQ(rig.synth->voices[0].filter.m_nMode, 2);
    EXPECT_FALSE(rig.synth->voices[0].filter.m_bInvertEnvelope);
    rig.controls.set(19, 0.0f);
    EXPECT_EQ(rig.synth->voices[0].filter.m_nMode, 0);
}

TEST(SubSynthControlTable, CutoffAndResonanceResolveAtOnce) {
    Rig rig;
    rig.controls.set(17, 0.5f);
    rig.controls.set(18, 0.4f);
    for (const auto &voice : rig.synth->voices) {
        EXPECT_EQ(voice.filter.m_fCutoff, 0.5f);
        EXPECT_EQ(voice.filter.m_nCutoff, voice.filter.m_anCutoffTable[127]);
        EXPECT_EQ(voice.filter.m_nDamping, voice.filter.m_anDampingTable[102]);
        EXPECT_EQ(voice.filter.m_nInputGain,
                  static_cast<int>((1.0f + -0.75f * 0.4f) * 16777215.0f));
    }
}

TEST(SubSynthControlTable, LfoControlsWriteTheirLfo) {
    Rig rig;
    rig.controls.set(12, 3.0f);
    rig.controls.set(13, 0.5f);
    rig.controls.set(10, 2.0f);
    rig.controls.set(14, 5.0f);
    EXPECT_EQ(rig.synth->lfo1.rate(), 3.0f);
    EXPECT_EQ(rig.synth->lfo1.depthQ24(), static_cast<std::int32_t>(0.5f * 16777215.0f));
    EXPECT_EQ(rig.synth->lfo1.waveform(), LFO::Waveform::Sawtooth);
    EXPECT_EQ(rig.synth->lfo2Target(), LFO::Target::Cutoff);
    EXPECT_EQ(rig.synth->lfo1Target(), LFO::Target::None);
}

TEST(SubSynthControlTable, OutputVolumeIsSquared) {
    Rig rig;
    rig.controls.set(29, 1.5f);
    EXPECT_EQ(rig.synth->shared.voiceGain, 2.25f);
}

TEST(SubSynthControlTable, PhaseAndAmountsScale) {
    Rig rig;
    rig.controls.set(5, -0.25f);
    EXPECT_EQ(rig.synth->shared.oscillator2.phaseOffset(), -1024);
    rig.controls.set(31, 0.5f);
    EXPECT_EQ(rig.synth->shared.modulationAmountQ24, static_cast<std::int32_t>(0.5f * 16777215.0f));
    rig.controls.set(2, 0.25f);
    EXPECT_EQ(rig.synth->shared.mixQ24, static_cast<std::int32_t>(0.25f * 16777215.0f));
}

TEST(SubSynthControlEvents, BendIsSymmetricInSemitones) {
    Rig rig;
    rig.controls.pitchBendSemitones = 2;
    rig.controls.set(SubSynthControls::kPitchBendId, 1.0f);
    EXPECT_EQ(rig.synth->shared.bend, std::pow(2.0f, 2.0f / 12.0f));
    rig.controls.set(SubSynthControls::kPitchBendId, 0.0f);
    EXPECT_EQ(rig.synth->shared.bend, std::pow(2.0f, -2.0f / 12.0f));
    rig.controls.set(SubSynthControls::kPitchBendId, 0.5f);
    EXPECT_EQ(rig.synth->shared.bend, 1.0f);
}

TEST(SubSynthControlEvents, MidiTargetsHandledInPlace) {
    Rig rig;
    EXPECT_TRUE(rig.controls.applyMidiTarget(SubSynthControls::kSustainTarget, 0.7f));
    EXPECT_TRUE(rig.synth->isSustainDown());
    rig.controls.applyMidiTarget(SubSynthControls::kSustainTarget, 0.2f);
    EXPECT_FALSE(rig.synth->isSustainDown());

    rig.controls.applyMidiTarget(SubSynthControls::kModulationModeTarget, 0.2f);
    EXPECT_EQ(rig.controls.modulationMode(), 0);
    rig.controls.applyMidiTarget(SubSynthControls::kModulationModeTarget, 0.5f);
    EXPECT_EQ(rig.controls.modulationMode(), 1);
    rig.controls.applyMidiTarget(SubSynthControls::kModulationModeTarget, 0.9f);
    EXPECT_EQ(rig.controls.modulationMode(), 2);
    EXPECT_EQ(rig.synth->shared.oscillator1.modulationMode(), Oscillator::ModulationMode::Amplitude);

    rig.controls.applyMidiTarget(SubSynthControls::kCentsModeTarget, 0.6f);
    EXPECT_EQ(rig.controls.centsMode(), 1);
}

TEST(SubSynthControlEvents, MidiValuesSpanTheControlRange) {
    Rig rig;
    rig.controls.applyMidiTarget(29, 0.5f);
    EXPECT_EQ(rig.controls.position(29), 1.0f);
    // A selector reports its count as the maximum, so 1.0 clamps to the last item.
    rig.controls.applyMidiTarget(19, 1.0f);
    EXPECT_EQ(rig.controls.position(19), 6.0f);
    EXPECT_FALSE(rig.controls.applyMidiTarget(0, 0.5f));
}

TEST(SubSynthControlLoad, AppliesControlsThenModes) {
    Rig rig;
    SubSynthArchive archive;
    archive.controls = {{8, 30.0f}, {25, 1.2f}, {29, 0.5f}};
    archive.polyphony = 2;
    archive.centsMode = 1;
    archive.modulationMode = 1;
    rig.controls.load(archive);

    // The cents mode runs after the collection, so it sees the loaded cents.
    EXPECT_EQ(rig.synth->shared.oscillator1.pitchRatio(), cents(-30.0f));
    EXPECT_EQ(rig.synth->shared.oscillator1.modulationMode(), Oscillator::ModulationMode::Phase);
    EXPECT_EQ(rig.synth->polyphony(), 2);
    // Current version: no fix-up, the position is squared.
    EXPECT_EQ(rig.synth->voices[0].amplitudeEnvelope.m_fAttack, 1.2f * 1.2f);
    EXPECT_EQ(rig.synth->shared.voiceGain, 0.25f);
}

TEST(SubSynthControlLoad, OldTimesComeBackAsSeconds) {
    Rig rig;
    SubSynthArchive archive;
    archive.version = 0x00020100;
    archive.controls = {{25, 1.2f}, {28, 3.0f}, {27, 0.5f}};
    rig.controls.load(archive);

    const ADSR &envelope = rig.synth->voices[0].amplitudeEnvelope;
    EXPECT_NEAR(envelope.m_fAttack, 1.2f, 1e-6f);
    EXPECT_EQ(rig.controls.position(25), std::sqrt(std::sqrt(1.2f * 1.2f)));
    // The collection clamps the stored 3.0 to the knob's 1.75 before the
    // fix-up runs, so the time comes back as 1.75 s.
    EXPECT_NEAR(envelope.m_fRelease, 1.75f, 1e-6f);
    // Sustain is not touched.
    EXPECT_EQ(envelope.m_fSustain, 0.5f);
}

TEST(SubSynthControlLoad, OldWaveformsAreRenumbered) {
    Rig rig;
    SubSynthArchive archive;
    archive.version = 0x00020200;
    archive.controls = {{1, 4.0f}, {4, 5.0f}};
    rig.controls.load(archive);
    EXPECT_EQ(rig.controls.position(1), 6.0f);
    EXPECT_EQ(rig.synth->shared.oscillator1.type(), Oscillator::Type::Noise);
    EXPECT_EQ(rig.controls.position(4), 7.0f);
    EXPECT_EQ(rig.synth->shared.oscillator2.type(), Oscillator::Type::Noise);
}

TEST(SubSynthControlLoad, IdsWithoutAModelledControlAreIgnored) {
    Rig rig;
    SubSynthArchive archive;
    archive.controls = {{9, 1.0f}, {200, 1.0f}, {29, 2.0f}};
    rig.controls.load(archive);
    EXPECT_EQ(rig.synth->shared.voiceGain, 4.0f);
}

TEST(SubSynthControlLoad, DefaultsReachTheEngine) {
    Rig rig;
    rig.controls.resetToDefaults();
    EXPECT_EQ(rig.synth->lfo1.rate(), 6.0f);
    EXPECT_EQ(rig.synth->voices[3].filterEnvelope.m_fRelease, 1.75f * 1.75f);
    EXPECT_EQ(rig.synth->voices[3].amplitudeEnvelope.m_fSustain, 1.0f);
    EXPECT_EQ(rig.synth->shared.voiceGain, 1.0f);
    EXPECT_EQ(rig.synth->shared.bend, 1.0f);
    EXPECT_EQ(rig.synth->shared.mixQ24, static_cast<std::int32_t>(0.5f * 16777215.0f));
}

TEST(SubSynthControlLoad, CustomWaveformsFillTheTables) {
    Rig rig;
    SubSynthArchive archive;
    SubSynthArchive::Waveform first{};
    SubSynthArchive::Waveform second{};
    for (std::size_t point = 0; point < first.size(); ++point) {
        first[point] = static_cast<std::int16_t>(point % 100);
        second[point] = static_cast<std::int16_t>(-static_cast<int>(point % 50));
    }
    archive.customWaveform1 = first;
    archive.customWaveform2 = second;
    rig.controls.load(archive);
    EXPECT_EQ(rig.synth->customWaveforms[0].height(99), 99.0f);
    EXPECT_EQ(rig.synth->customWaveforms[1].height(49), -49.0f);
    EXPECT_NE(rig.synth->customWavetable(0)[0], 0);

    // Played as type 7, the oscillator reads the custom table.
    rig.controls.set(1, 7.0f);
    EXPECT_EQ(rig.synth->shared.oscillator1.type(), Oscillator::Type::Custom1);
}

TEST(SubSynthControlLoad, BeforeThreeTheSecondWaveformIsHalfCleared) {
    Rig rig;
    SubSynthArchive archive;
    archive.version = 0x00020200;
    SubSynthArchive::Waveform first{};
    first.fill(25);
    archive.customWaveform1 = first;
    rig.controls.load(archive);
    // 0x294 bytes cleared: the first 330 points, not all 660.
    EXPECT_EQ(rig.synth->customWaveforms[1].height(329), 0.0f);
    EXPECT_EQ(rig.synth->customWaveforms[1].height(330), 25.0f);
    EXPECT_EQ(rig.synth->customWaveforms[1].height(659), 25.0f);
}
