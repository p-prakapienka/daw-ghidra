#include "components/Keyboard.h"
#include "components/SubSynth.h"

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <vector>

namespace {

constexpr unsigned int kBlock = 128;

std::unique_ptr<SubSynth> makeSynth() { return std::make_unique<SubSynth>(); }

void render(SubSynth &synth, unsigned int blocks = 1) {
    std::vector<int> output(kBlock, 0);
    for (unsigned int block = 0; block < blocks; ++block) {
        synth.process(output.data(), kBlock);
    }
}

void play(SubSynth &synth, int note, float start = 0.0f, float duration = 1.0f,
          std::uint16_t flags = 0) {
    SequencerKeyEvent event = SequencerKeyEvent::noteOn(note, 1.0f, start, duration);
    event.flags = flags;
    synth.onKeyEvent(event);
}

} // namespace

TEST(Keyboard, IsEqualTemperedFromConcertA) {
    EXPECT_FLOAT_EQ(Keyboard::frequencyFromNoteId(69), 440.0f);
    EXPECT_FLOAT_EQ(Keyboard::frequencyFromNoteId(81), 880.0f);
    EXPECT_FLOAT_EQ(Keyboard::frequencyFromNoteId(57), 220.0f);
    EXPECT_NEAR(Keyboard::frequencyFromNoteId(60), 261.6256f, 1e-3f);
}

TEST(SequencerKeyEvent, DefaultsAreTheEmptyQueueEntry) {
    const SequencerKeyEvent event;
    EXPECT_EQ(event.type, SequencerKeyEvent::Type::None);
    EXPECT_EQ(event.field00, -1);
    EXPECT_FLOAT_EQ(event.start, 1000000.0f);
    EXPECT_FLOAT_EQ(event.velocity, 1.0f);
}

TEST(SubSynthPolyphony, DefaultsToFourVoices) {
    auto synth = makeSynth();
    EXPECT_EQ(synth->polyphony(), 4);
    EXPECT_FALSE(synth->isMonophonic());
    for (const SubSynthVoice &voice : synth->voices) {
        EXPECT_EQ(voice.level.smoothing, SubSynthVoice::kLevelSmoothing);
    }
}

TEST(SubSynthPolyphony, ClampsAndMarksOneVoiceMonophonic) {
    auto synth = makeSynth();

    synth->setPolyphony(1);
    EXPECT_EQ(synth->polyphony(), 1);
    EXPECT_TRUE(synth->isMonophonic());

    synth->setPolyphony(0);
    EXPECT_EQ(synth->polyphony(), 1);
    EXPECT_TRUE(synth->isMonophonic());

    synth->setPolyphony(-3);
    EXPECT_EQ(synth->polyphony(), 1);

    synth->setPolyphony(2);
    EXPECT_EQ(synth->polyphony(), 2);
    EXPECT_FALSE(synth->isMonophonic());

    synth->setPolyphony(20);
    EXPECT_EQ(synth->polyphony(), SubSynth::kMaxPolyphony);
}

TEST(SubSynthQueue, EventsWaitForTheNextBlock) {
    auto synth = makeSynth();
    play(*synth, 60);
    EXPECT_EQ(synth->pendingCount(), 1u);
    EXPECT_EQ(synth->notesOn(), 0);

    render(*synth);
    EXPECT_EQ(synth->pendingCount(), 0u);
    EXPECT_EQ(synth->notesOn(), 1);
    EXPECT_EQ(synth->notesPlaying(), 1);
}

TEST(SubSynthQueue, DropsEventsBeyondSixtyFour) {
    auto synth = makeSynth();
    for (int index = 0; index < 70; ++index) {
        synth->onKeyEvent(SequencerKeyEvent::noteOff(index));
    }
    EXPECT_EQ(synth->pendingCount(), SubSynth::kQueueCapacity);
}

TEST(SubSynthQueue, IgnoresOtherEventTypes) {
    auto synth = makeSynth();
    SequencerKeyEvent event = SequencerKeyEvent::noteOn(60);
    event.type = static_cast<SequencerKeyEvent::Type>(2);
    synth->onKeyEvent(event);
    EXPECT_EQ(synth->pendingCount(), 0u);
}

TEST(SubSynthAllocation, FillsFreeVoicesThenStealsTheOldest) {
    auto synth = makeSynth();
    for (int note : {60, 62, 64, 66}) {
        play(*synth, note);
        render(*synth);
    }
    for (unsigned int index = 0; index < 4; ++index) {
        EXPECT_EQ(synth->voices[index].voltage.noteId, 60 + 2 * static_cast<int>(index));
    }

    play(*synth, 68);
    render(*synth);
    EXPECT_EQ(synth->voices[0].voltage.noteId, 68);
    EXPECT_EQ(synth->voices[1].voltage.noteId, 62);
    EXPECT_EQ(synth->notesOn(), 4);
}

TEST(SubSynthAllocation, DropsANoteWhenNoVoiceHasRenderedYet) {
    auto synth = makeSynth();
    for (int note : {60, 62, 64, 66, 68}) {
        play(*synth, note);
    }
    render(*synth);

    EXPECT_EQ(synth->notesOn(), 4);
    for (unsigned int index = 0; index < 4; ++index) {
        EXPECT_NE(synth->voices[index].voltage.noteId, 68);
    }
}

TEST(SubSynthAllocation, AHeldNoteIsNotPlayedTwiceUnlessSustained) {
    auto synth = makeSynth();
    play(*synth, 60);
    render(*synth);
    play(*synth, 60);
    render(*synth);
    EXPECT_EQ(synth->notesOn(), 1);

    synth->setSustain(true);
    play(*synth, 60);
    render(*synth);
    EXPECT_EQ(synth->notesOn(), 2);
}

TEST(SubSynthRelease, NoteOffClosesTheGateAndTheVoiceEnds) {
    auto synth = makeSynth();
    play(*synth, 60);
    render(*synth, 4);

    synth->onKeyEvent(SequencerKeyEvent::noteOff(60));
    render(*synth);
    EXPECT_EQ(synth->notesOn(), 0);
    EXPECT_EQ(synth->notesPlaying(), 1);

    const unsigned int tail = synth->voices[0].amplitudeEnvelope.releaseSamples()
                              + ADSR::kReleaseTailSamples;
    render(*synth, tail / kBlock + 2);
    EXPECT_EQ(synth->notesPlaying(), 0);
}

TEST(SubSynthRelease, StopAllNotesClosesEveryGate) {
    auto synth = makeSynth();
    play(*synth, 60);
    play(*synth, 64);
    render(*synth);
    synth->stopAllNotes();
    EXPECT_EQ(synth->notesOn(), 0);
}

TEST(SubSynthSustain, PedalHoldsNoteOffsUntilLifted) {
    auto synth = makeSynth();
    play(*synth, 60);
    render(*synth);

    synth->setSustain(true);
    synth->previewKeyOff(SequencerKeyEvent::noteOff(60));
    synth->previewKeyOff(SequencerKeyEvent::noteOff(60));
    EXPECT_EQ(synth->sustainedCount(), 1u);
    render(*synth);
    EXPECT_EQ(synth->notesOn(), 1);

    synth->setSustain(false);
    EXPECT_EQ(synth->sustainedCount(), 0u);
    render(*synth);
    EXPECT_EQ(synth->notesOn(), 0);
}

TEST(SubSynthSustain, FocusLossReleasesOnlyLiveNotes) {
    auto synth = makeSynth();
    play(*synth, 60);
    play(*synth, 64, 0.0f, 1.0f, SequencerKeyEvent::kLiveInputFlag);
    render(*synth);

    synth->setSustain(true);
    synth->focusLost();

    EXPECT_FALSE(synth->isSustainDown());
    EXPECT_TRUE(synth->voices[0].voltage.isGateOpen());
    EXPECT_FALSE(synth->voices[1].voltage.isGateOpen());
}

TEST(SubSynthNoteOn, PolyphonicNotesRetriggerAndResetPhase) {
    auto synth = makeSynth();
    synth->keyboardTracking = 1.0f;
    play(*synth, 81);
    render(*synth);

    const ControlVoltage &voltage = synth->voices[0].voltage;
    EXPECT_FLOAT_EQ(voltage.frequencyHertz, 880.0f);
    EXPECT_FLOAT_EQ(voltage.targetPitch, 880.0f * 4096.0f);
    EXPECT_EQ(voltage.frequencyQ12, static_cast<std::uint32_t>(880.0f * 4096.0f));
    EXPECT_FLOAT_EQ(voltage.filterCutoffScale, std::sqrt(880.0f / 500.0f));
    EXPECT_EQ(voltage.glideSamples, 0u);
    EXPECT_EQ(voltage.envelopePosition, static_cast<std::int32_t>(kBlock));
}

TEST(SubSynthNoteOn, AMonophonicLineKeepsItsPhase) {
    auto synth = makeSynth();
    synth->setPolyphony(1);
    play(*synth, 60);
    render(*synth, 3);
    const std::uint32_t phaseBefore = synth->voices[0].voltage.phase[0];
    ASSERT_NE(phaseBefore, 0u);

    // The only voice is busy, so the note steals it: the envelope restarts
    // but the phase is kept.
    play(*synth, 64);
    synth->processPendingNotes();
    EXPECT_EQ(synth->voices[0].voltage.phase[0], phaseBefore);
    EXPECT_EQ(synth->voices[0].voltage.envelopePosition, 0);
    EXPECT_EQ(synth->voices[0].voltage.glideSamples, 0u);
}

TEST(SubSynthGlide, SlideGlidesForTheOverlapWithoutRetriggering) {
    auto synth = makeSynth();
    synth->setPolyphony(1);
    play(*synth, 60, 0.0f, 1.0f);
    render(*synth, 2);
    const float previousPitch = synth->voices[0].voltage.currentPitch;

    play(*synth, 72, 0.5f, 1.0f, SequencerKeyEvent::kSlideFlag);
    synth->processPendingNotes();

    const ControlVoltage &voltage = synth->voices[0].voltage;
    // Half a beat of overlap at 120 BPM, 22050 samples per beat.
    EXPECT_EQ(voltage.glideSamples, 11025u);
    EXPECT_FLOAT_EQ(voltage.glideStartPitch, previousPitch);
    EXPECT_FLOAT_EQ(voltage.targetPitch, Keyboard::frequencyFromNoteId(72) * 4096.0f);
    EXPECT_EQ(voltage.envelopePosition, static_cast<std::int32_t>(2 * kBlock));
    EXPECT_EQ(voltage.samplesSinceNoteOn, 0);
    EXPECT_EQ(synth->lastMonophonicEvent().note, 72);
}

TEST(SubSynthGlide, NeverShorterThanATwelfthOfABeat) {
    auto synth = makeSynth();
    synth->setPolyphony(1);
    play(*synth, 60, 0.0f, 1.0f);
    render(*synth);

    // Starting after the previous note has ended leaves no overlap.
    play(*synth, 72, 2.0f, 1.0f, SequencerKeyEvent::kSlideFlag);
    synth->processPendingNotes();
    EXPECT_EQ(synth->voices[0].voltage.glideSamples,
              static_cast<std::uint32_t>(SubSynth::kMinimumGlideBeats * 22050.0f));
}

TEST(SubSynthGlide, SlideIsIgnoredWhenPolyphonic) {
    auto synth = makeSynth();
    play(*synth, 60, 0.0f, 1.0f);
    render(*synth);
    play(*synth, 72, 0.5f, 1.0f, SequencerKeyEvent::kSlideFlag);
    render(*synth);

    EXPECT_EQ(synth->voices[1].voltage.glideSamples, 0u);
    EXPECT_EQ(synth->voices[1].voltage.envelopePosition, static_cast<std::int32_t>(kBlock));
}

TEST(SubSynthTransport, LFOPositionFollowsSamplesPerBeat) {
    auto synth = makeSynth();
    EXPECT_EQ(synth->lfoPosition(2.0), 44100u);
    synth->setTempo(60.0f);
    EXPECT_EQ(synth->lfoPosition(1.0), 44100u);
}

TEST(SubSynthControls, PitchSweepSignAndDecay) {
    auto synth = makeSynth();
    const float downDecay = synth->pitchSweepDecay;

    synth->setPitchSweep(0.25f);
    EXPECT_FLOAT_EQ(synth->pitchSweep, -0.25f);
    EXPECT_FLOAT_EQ(synth->pitchSweepDecay, downDecay);

    synth->setPitchSweep(-0.25f);
    EXPECT_FLOAT_EQ(synth->pitchSweep, 0.5f);
    EXPECT_GT(synth->pitchSweepDecay, downDecay);
}

TEST(SubSynthProcess, RendersSound) {
    auto synth = makeSynth();
    play(*synth, 69);
    std::vector<int> output(kBlock, 0);
    int peak = 0;
    for (int block = 0; block < 8; ++block) {
        std::fill(output.begin(), output.end(), 0);
        synth->process(output.data(), kBlock);
        for (int value : output) {
            peak = std::max(peak, std::abs(value));
        }
    }
    EXPECT_GT(peak, 0);
}
