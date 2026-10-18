#include "machines/subsynth/SubSynth.h"

#include <cmath>

#include <algorithm>
#include <bit>

#include "ui/Keyboard.h"

namespace {

constexpr float kSweepDownDecay = std::bit_cast<float>(0x3F7FE5C9u); // ~0.99960
constexpr float kSweepUpDecay = std::bit_cast<float>(0x3F7FEDFAu);   // ~0.99972

} // namespace

SubSynth::SubSynth() : pitchSweepDecay(kSweepDownDecay) {
    // The transport's tempo is not traced; 120 is the LFO constructor's.
    setTempo(120.0f);
    setLFOTargets(LFO::Target::None, LFO::Target::None);
    // The constructor's two envelope loops, each parameter followed by
    // OnParamModified as there. Filter envelopes (0xa038c-0xa03c4): minimums
    // of 10, 20 and 200 samples, attack and decay 0, sustain 1, release 2 s.
    // Amplitude envelopes (0xa0450-0xa048c): attack, decay and release 0,
    // sustain 16777215.0, and the byte at +0x61 set, so a new note rises from
    // the level the voice is at. The control defaults applied through
    // OnChildModified afterwards replace most of these (SubSynthControls).
    // The default modulation source (0xa0494-0xa04d0) is set up by
    // SubSynthVoice::Shared's constructor.

    // Both oscillators can play both custom tables (0xa02c0-0xa02e4).
    shared.oscillator1.m_psCustomTable1 = customWavetables_[0].data();
    shared.oscillator1.m_psCustomTable2 = customWavetables_[1].data();
    shared.oscillator2.m_psCustomTable1 = customWavetables_[0].data();
    shared.oscillator2.m_psCustomTable2 = customWavetables_[1].data();

    for (SubSynthVoice &voice : voices) {
        voice.setFilterEnvelope();
        ADSR &filter = voice.filterEnvelope;
        filter.SetMinSamples(10, 20, 200);
        filter.m_fAttack = 0.0f;
        filter.OnParamModified();
        filter.m_fDecay = 0.0f;
        filter.OnParamModified();
        filter.m_fSustain = 1.0f;
        filter.OnParamModified();
        filter.m_fRelease = 2.0f;
        filter.OnParamModified();

        ADSR &amplitude = voice.amplitudeEnvelope;
        amplitude.m_fAttack = 0.0f;
        amplitude.OnParamModified();
        amplitude.m_fDecay = 0.0f;
        amplitude.OnParamModified();
        amplitude.m_fSustain = 16777215.0f;
        amplitude.OnParamModified();
        amplitude.m_fRelease = 0.0f;
        amplitude.OnParamModified();
        amplitude.m_bStartFromCurrent = true;
    }
    setPolyphony(kDefaultPolyphony);
}

void SubSynth::refreshCustomWavetable(int index) {
    if (index != 0 && index != 1) {
        return;
    }
    const auto slot = static_cast<std::size_t>(index);
    customWaveforms[slot].getWaveform(customWavetables_[slot].data(), Oscillator::kTableSize, 0.3f);
}

void SubSynth::setPitchSweep(float value) {
    if (value > 0.0f) {
        pitchSweep = -value;
        pitchSweepDecay = kSweepDownDecay;
    } else {
        pitchSweep = -value - value;
        pitchSweepDecay = kSweepUpDecay;
    }
}

void SubSynth::setTempo(float bpm) {
    beatsPerSample_ = static_cast<double>(bpm) / 60.0 / kSampleRate;
    lfo1.SetBPM(bpm);
    lfo2.SetBPM(bpm);
}

void SubSynth::setLFOTargets(LFO::Target target1, LFO::Target target2) {
    lfo1Target_ = target1;
    lfo2Target_ = target2;
    shared.routing = SubSynthRouting::connect(lfo1, target1, lfo2, target2);
    shared.connectOscillators();
}

void SubSynth::resetVoltage(ControlVoltage &voltage) {
    // SetPolyphony's per-slot loop, which Serialize repeats on load. It keeps
    // the flag bits above 2 and the fields at 0x18 and 0x1C.
    voltage.m_nFlags = static_cast<std::uint8_t>(voltage.m_nFlags & ~0x07);
    voltage.m_nSamplesSinceNoteOn = 0;
    voltage.m_nEnvelopePosition = 0;
    voltage.m_nReleasePosition = 0;
    voltage.m_anPhase[0] = 0;
    voltage.m_anPhase[1] = 0;
    voltage.m_nFrequency = 0;
    voltage.m_fGlideStart = 0.0f;
    voltage.m_fTargetPitch = 0.0f;
    voltage.m_fCurrentPitch = 0.0f;
    voltage.m_nGlideSamples = 0;
    voltage.m_n34 = 0;
    voltage.m_n38 = 1;
    voltage.m_nNoteId = 0;
    voltage.m_f40 = 0.0f;
    voltage.m_f44 = 0.0f;
    voltage.m_afPitchSweep[0] = 0.0f;
    voltage.m_afPitchSweep[1] = 0.0f;
    voltage.m_fPitchSweepDecay = 0.0f;
    voltage.m_fFrequency = 0.0f;
    voltage.m_fFilterCutoffScale = 1.0f;
    voltage.m_fVelocity = 1.0f;
}

void SubSynth::setPolyphony(int requested) {
    int voices;
    bool monophonic;
    bool clearFlag = true;

    if (requested == 0) {
        voices = 1;
        monophonic = true;
    } else if (requested == 2) {
        monophonic = flagC2F9_;
        voices = flagC2F9_ ? 1 : 2;
    } else if (requested < 0) {
        voices = 1;
        monophonic = true;
        clearFlag = false;
    } else {
        voices = std::min(requested, kMaxPolyphony);
        monophonic = voices == 1;
    }

    if (clearFlag) {
        flagC2F9_ = false;
    }
    polyphony_ = voices;
    monophonic_ = monophonic;

    // All sixteen slots, not just the enabled ones. Each filter envelope's
    // byte at +0x61 takes +0xC2F9 as it now stands (0x9ccdc).
    for (SubSynthVoice &voice : this->voices) {
        voice.filterEnvelope.m_bStartFromCurrent = flagC2F9_;
        resetVoltage(voice.voltage);
        voice.level.smoothing = SubSynthVoice::kLevelSmoothing;
    }
}

void SubSynth::onKeyEvent(const SequencerKeyEvent &event) {
    if (event.type == SequencerKeyEvent::Type::NoteOn || event.type == SequencerKeyEvent::Type::NoteOff) {
        queueKeyEvent(event);
    }
}

void SubSynth::queueKeyEvent(const SequencerKeyEvent &event) {
    // AddPendingNote drops the event once 64 are waiting.
    if (pendingCount_ >= kQueueCapacity) {
        return;
    }
    std::lock_guard<std::mutex> lock(pendingMutex_);
    pending_[pendingCount_++] = event;
}

void SubSynth::previewKeyOff(const SequencerKeyEvent &event) {
    if (sustain_ && sustainedCount_ < kQueueCapacity) {
        for (uint index = 0; index < sustainedCount_; ++index) {
            if (sustained_[index].note == event.note) {
                return;
            }
        }
        sustained_[sustainedCount_++] = event;
        return;
    }
    queueKeyEvent(event);
}

void SubSynth::setSustain(bool down) {
    sustain_ = down;
    if (!down) {
        releaseSustainedNotes();
    }
}

void SubSynth::releaseSustainedNotes() {
    for (uint index = 0; index < sustainedCount_; ++index) {
        queueKeyEvent(sustained_[index]);
    }
    sustainedCount_ = 0;
}

void SubSynth::focusLost() {
    for (SubSynthVoice &voice : voices) {
        ControlVoltage &voltage = voice.voltage;
        if (((voltage.m_nFlags & ControlVoltage::kGateBit) != 0) && (voltage.m_nFlags & 0x04) != 0) {
            voltage.m_nFlags = static_cast<std::uint8_t>(voltage.m_nFlags & ~ControlVoltage::kGateBit);
            voltage.m_nReleasePosition = 0;
        }
    }
    sustain_ = false;
    releaseSustainedNotes();
}

void SubSynth::playNote(const SequencerKeyEvent &event) {
    // A note that is already held is not played again, unless the sustain
    // pedal is down. All sixteen slots are checked.
    for (const SubSynthVoice &voice : voices) {
        if (((voice.voltage.m_nFlags & ControlVoltage::kGateBit) != 0) && voice.voltage.m_nNoteId == event.note && !sustain_) {
            return;
        }
    }

    if (polyphony_ <= 0) {
        return;
    }
    const auto count = static_cast<uint>(polyphony_);

    // The first inactive voice.
    for (uint index = 0; index < count; ++index) {
        if (!voices[index].isActive()) {
            playChannel(event, index, false);
            return;
        }
    }

    // Otherwise steal the voice whose note started longest ago. A strict
    // comparison from zero means a voice that has not rendered yet is never
    // chosen, and the note is dropped if none has.
    int oldest = -1;
    std::uint32_t longest = 0;
    for (uint index = 0; index < count; ++index) {
        const auto age = static_cast<std::uint32_t>(voices[index].voltage.m_nSamplesSinceNoteOn);
        if (age > longest) {
            longest = age;
            oldest = static_cast<int>(index);
        }
    }
    if (oldest < 0) {
        return;
    }
    playChannel(event, static_cast<uint>(oldest), true);
}

uint SubSynth::glideSamplesFor(const SequencerKeyEvent &event) const {
    // A sliding note glides for as long as it overlaps the previous one, but
    // never less than a twelfth of a beat.
    float beats = kMinimumGlideBeats;
    if (event.start > lastEvent_.start) {
        beats = lastEvent_.start + lastEvent_.duration - event.start;
        if (beats < kMinimumGlideBeats) {
            beats = kMinimumGlideBeats;
        }
    }
    const auto samplesPerBeat = static_cast<std::uint32_t>(1.0 / beatsPerSample_);
    return static_cast<uint>(beats * static_cast<float>(samplesPerBeat));
}

void SubSynth::playChannel(const SequencerKeyEvent &event, uint index, bool stealing) {
    ControlVoltage &voltage = voices[index].voltage;
    const bool slide = monophonic_ && (event.flags & SequencerKeyEvent::kSlideFlag) != 0;

    const float hertz = Keyboard::frequencyFromNoteId(event.note);
    const float pitch = hertz * ControlVoltage::kPitchScale;

    voltage.m_nNoteId = event.note;
    voltage.m_fFrequency = hertz;
    voltage.m_nFrequency = static_cast<std::uint32_t>(pitch);
    voltage.m_fVelocity = event.velocity;
    // Key tracking with a square-root law around 500 Hz:
    // 1 + tracking * (sqrt(hz / 500) - 1).
    voltage.m_fFilterCutoffScale = 1.0f + keyboardTracking * (std::sqrt(hertz / 500.0f) - 1.0f);
    voltage.m_nFlags = static_cast<std::uint8_t>(
        (voltage.m_nFlags & ~0x04) | (event.flags & SequencerKeyEvent::kLiveInputFlag));
    voltage.m_nSamplesSinceNoteOn = 0;
    voltage.m_nReleasePosition = 0;
    voltage.m_afPitchSweep[0] = pitchSweep;
    voltage.m_afPitchSweep[1] = pitchSweep;
    voltage.m_fPitchSweepDecay = pitchSweepDecay;
    voltage.m_f44 = 0.0f;
    voltage.m_f40 = 0.0f;

    // Whether the envelope restarts. With +0xC2F9 clear, which is always in
    // this build, only a slide keeps it running.
    bool retrigger = !slide;
    if (flagC2F9_) {
        retrigger = !(flagC2FA_ && stealing);
    }
    if (retrigger) {
        voltage.m_nEnvelopePosition = 0;
        // A stolen voice keeps its oscillator phases, so a monophonic line
        // stays phase-continuous from note to note.
        if (!slide && !stealing) {
            voltage.m_anPhase[0] = 0;
            voltage.m_anPhase[1] = 0;
        }
    }

    voltage.m_nFlags = static_cast<std::uint8_t>(voltage.m_nFlags | ControlVoltage::kActiveBit
                                              | ControlVoltage::kGateBit);
    voltage.m_fTargetPitch = pitch;

    // Polyphonic notes stop here: the glide fields are left alone, and the
    // oscillator reads the target directly while glideSamples is zero.
    if (!monophonic_) {
        return;
    }

    if (slide) {
        voltage.m_fGlideStart = voltage.m_fCurrentPitch;
        voltage.m_nGlideSamples = glideSamplesFor(event);
    } else {
        voltage.m_fCurrentPitch = pitch;
        voltage.m_fGlideStart = pitch;
        voltage.m_nGlideSamples = 0;
    }
    lastEvent_ = event;
}

void SubSynth::stopNote(const SequencerKeyEvent &event) {
    for (int index = 0; index < polyphony_; ++index) {
        ControlVoltage &voltage = voices[static_cast<uint>(index)].voltage;
        if (((voltage.m_nFlags & ControlVoltage::kGateBit) != 0) && voltage.m_nNoteId == event.note) {
            voltage.m_nReleasePosition = 0;
            voltage.m_nFlags = static_cast<std::uint8_t>(voltage.m_nFlags & ~ControlVoltage::kGateBit);
        }
    }
}

void SubSynth::stopAllNotes() {
    for (SubSynthVoice &voice : voices) {
        if (((voice.voltage.m_nFlags & ControlVoltage::kGateBit) != 0)) {
            voice.voltage.m_nFlags = static_cast<std::uint8_t>(voice.voltage.m_nFlags & ~ControlVoltage::kGateBit);
            voice.voltage.m_nReleasePosition = 0;
        }
    }
}

void SubSynth::processPendingNotes() {
    std::lock_guard<std::mutex> lock(pendingMutex_);
    flagC2FA_ = false;
    for (uint index = 0; index < pendingCount_; ++index) {
        SequencerKeyEvent &event = pending_[index];
        if (event.type == SequencerKeyEvent::Type::NoteOn) {
            playNote(event);
        } else if (event.type == SequencerKeyEvent::Type::NoteOff) {
            stopNote(event);
            flagC2FA_ = true;
        }
        // The consumed entry goes back to the empty event.
        event = SequencerKeyEvent{};
    }
    pendingCount_ = 0;
}

uint SubSynth::lfoPosition(double songPositionBeats) const {
    const auto samplesPerBeat = static_cast<std::uint32_t>(1.0 / beatsPerSample_);
    return static_cast<uint>(static_cast<double>(samplesPerBeat) * songPositionBeats);
}

void SubSynth::process(int *output, uint numSamples, double songPositionBeats) {
    const uint position = lfoPosition(songPositionBeats);

    processPendingNotes();

    lfo1.m_nSongPosition = position;
    lfo1.GenerateInternal(numSamples);
    lfo2.m_nSongPosition = position;
    lfo2.GenerateInternal(numSamples);

    for (int index = 0; index < polyphony_; ++index) {
        SubSynthVoice &voice = voices[static_cast<uint>(index)];
        if (voice.isActive()) {
            voice.process(shared, output, numSamples);
        }
    }
}

int SubSynth::notesPlaying() const {
    int count = 0;
    for (int index = 0; index < polyphony_; ++index) {
        count += voices[static_cast<uint>(index)].isActive() ? 1 : 0;
    }
    return count;
}

int SubSynth::notesOn() const {
    int count = 0;
    for (int index = 0; index < polyphony_; ++index) {
        count += ((voices[static_cast<uint>(index)].voltage.m_nFlags & ControlVoltage::kGateBit) != 0) ? 1 : 0;
    }
    return count;
}
