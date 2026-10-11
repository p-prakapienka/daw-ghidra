#include "SubSynth.h"

#include <algorithm>
#include <bit>

#include "Keyboard.h"

namespace {

constexpr float kSweepDownDecay = std::bit_cast<float>(0x3F7FE5C9u); // ~0.99960
constexpr float kSweepUpDecay = std::bit_cast<float>(0x3F7FEDFAu);   // ~0.99972

} // namespace

SubSynth::SubSynth() : pitchSweepDecay(kSweepDownDecay) {
    // The transport's tempo is not traced; 120 is the LFO constructor's.
    setTempo(120.0f);
    setLFOTargets(LFO::Target::None, LFO::Target::None);
    setPolyphony(kDefaultPolyphony);
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
}

void SubSynth::resetVoltage(ControlVoltage &voltage) {
    // SetPolyphony's per-slot loop, which Serialize repeats on load. It keeps
    // the flag bits above 2 and the fields at 0x18 and 0x1C.
    voltage.flags = static_cast<std::uint8_t>(voltage.flags & ~0x07);
    voltage.samplesSinceNoteOn = 0;
    voltage.envelopePosition = 0;
    voltage.releasePosition = 0;
    voltage.phase[0] = 0;
    voltage.phase[1] = 0;
    voltage.frequencyQ12 = 0;
    voltage.glideStartPitch = 0.0f;
    voltage.targetPitch = 0.0f;
    voltage.currentPitch = 0.0f;
    voltage.glideSamples = 0;
    voltage.field34 = 0;
    voltage.field38 = 1;
    voltage.noteId = 0;
    voltage.field40 = 0.0f;
    voltage.field44 = 0.0f;
    voltage.pitchSweep[0] = 0.0f;
    voltage.pitchSweep[1] = 0.0f;
    voltage.pitchSweepDecay = 0.0f;
    voltage.frequencyHertz = 0.0f;
    voltage.filterCutoffScale = 1.0f;
    voltage.velocity = 1.0f;
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

    // All sixteen slots, not just the enabled ones. The engine also writes
    // the (unmodelled) byte at +0x61 of each filter envelope here.
    for (SubSynthVoice &voice : this->voices) {
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
        if (voltage.isGateOpen() && (voltage.flags & 0x04) != 0) {
            voltage.noteOff();
            voltage.releasePosition = 0;
        }
    }
    sustain_ = false;
    releaseSustainedNotes();
}

void SubSynth::playNote(const SequencerKeyEvent &event) {
    // A note that is already held is not played again, unless the sustain
    // pedal is down. All sixteen slots are checked.
    for (const SubSynthVoice &voice : voices) {
        if (voice.voltage.isGateOpen() && voice.voltage.noteId == event.note && !sustain_) {
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
        const auto age = static_cast<std::uint32_t>(voices[index].voltage.samplesSinceNoteOn);
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

    voltage.noteId = event.note;
    voltage.frequencyHertz = hertz;
    voltage.frequencyQ12 = static_cast<std::uint32_t>(pitch);
    voltage.velocity = event.velocity;
    voltage.filterCutoffScale = ControlVoltage::trackingScale(hertz, keyboardTracking);
    voltage.flags = static_cast<std::uint8_t>(
        (voltage.flags & ~0x04) | (event.flags & SequencerKeyEvent::kLiveInputFlag));
    voltage.samplesSinceNoteOn = 0;
    voltage.releasePosition = 0;
    voltage.pitchSweep[0] = pitchSweep;
    voltage.pitchSweep[1] = pitchSweep;
    voltage.pitchSweepDecay = pitchSweepDecay;
    voltage.field44 = 0.0f;
    voltage.field40 = 0.0f;

    // Whether the envelope restarts. With +0xC2F9 clear, which is always in
    // this build, only a slide keeps it running.
    bool retrigger = !slide;
    if (flagC2F9_) {
        retrigger = !(flagC2FA_ && stealing);
    }
    if (retrigger) {
        voltage.envelopePosition = 0;
        // A stolen voice keeps its oscillator phases, so a monophonic line
        // stays phase-continuous from note to note.
        if (!slide && !stealing) {
            voltage.phase[0] = 0;
            voltage.phase[1] = 0;
        }
    }

    voltage.flags = static_cast<std::uint8_t>(voltage.flags | ControlVoltage::kActiveBit
                                              | ControlVoltage::kGateBit);
    voltage.targetPitch = pitch;

    // Polyphonic notes stop here: the glide fields are left alone, and the
    // oscillator reads the target directly while glideSamples is zero.
    if (!monophonic_) {
        return;
    }

    if (slide) {
        voltage.glideStartPitch = voltage.currentPitch;
        voltage.glideSamples = glideSamplesFor(event);
    } else {
        voltage.currentPitch = pitch;
        voltage.glideStartPitch = pitch;
        voltage.glideSamples = 0;
    }
    lastEvent_ = event;
}

void SubSynth::stopNote(const SequencerKeyEvent &event) {
    for (int index = 0; index < polyphony_; ++index) {
        ControlVoltage &voltage = voices[static_cast<uint>(index)].voltage;
        if (voltage.isGateOpen() && voltage.noteId == event.note) {
            voltage.releasePosition = 0;
            voltage.noteOff();
        }
    }
}

void SubSynth::stopAllNotes() {
    for (SubSynthVoice &voice : voices) {
        if (voice.voltage.isGateOpen()) {
            voice.voltage.noteOff();
            voice.voltage.releasePosition = 0;
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

    lfo1.generate(position, numSamples);
    lfo2.generate(position, numSamples);

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
        count += voices[static_cast<uint>(index)].voltage.isGateOpen() ? 1 : 0;
    }
    return count;
}
