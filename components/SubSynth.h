#pragma once

// Reconstructed SubSynth machine: voice allocation, note on and off, glide,
// sustain, the pending-note queue, polyphony and the per-block driver.
//
// Recovered from SubSynth::PlayNote, PlayChannel, StopNote, StopAllNotes,
// AddPendingNote, ProcessPendingNotes, SlavePlayNote, SlaveStopNote,
// OnSequencerKeyEvent, DoKeyOff, PreviewKeyOn, PreviewKeyOff,
// ReleaseSustainedNotes, OnMIDIFocusLost, SetPolyphony, SetLFO1/2Target,
// OnBPMChanged, GetNumNotesPlaying, GetNumNotesOn and Process in libcaustic.so
// (ARMv7). See components/SubSynth.md.
//
// The UI side of those functions (the on-screen keyboard, the pattern editor,
// recording, the VU meter) is left out. The pattern editor is what drives the
// sequencer, so a host feeds events in with onKeyEvent instead.

#include <array>
#include <cstdint>
#include <mutex>

#include "LFO.h"
#include "SequencerKeyEvent.h"
#include "SubSynthVoice.h"

using uint = unsigned int;

class SubSynth {
public:
    static constexpr uint kVoiceSlots = SubSynthVoice::kVoiceSlots;
    static constexpr int kMaxPolyphony = static_cast<int>(SubSynthVoice::kMaxPolyphony);

    // ResetMachine, which the constructor path runs, calls SetPolyphony(4).
    static constexpr int kDefaultPolyphony = 4;

    // Both the pending-note queue (+0x12FC, count at +0x20FC) and the
    // sustained-note list (+0x2100, count at +0x2F00) hold 64 events.
    static constexpr uint kQueueCapacity = 64;

    // The shortest glide PlayChannel will set up, in beats: 0x3DAAAAAB.
    static constexpr float kMinimumGlideBeats = 1.0f / 12.0f;

    // The engine's fixed rate, which the transport uses for samples per beat.
    static constexpr double kSampleRate = 44100.0;

    SubSynth();
    SubSynth(const SubSynth &) = delete;
    SubSynth &operator=(const SubSynth &) = delete;

    SubSynthVoice::Shared shared;
    std::array<SubSynthVoice, kVoiceSlots> voices;
    LFO lfo1; // +0x36E4
    LFO lfo2; // +0x3724

    // "Filter Kb track", read from its control on every note-on.
    float keyboardTracking = 0.0f;

    // +0x3054 and +0x3058: copied into both of a voice's sweeps and its sweep
    // decay on note-on. The constructor sets no sweep and a decay of
    // 0x3F7FE5C9.
    float pitchSweep = 0.0f;
    float pitchSweepDecay;

    // The pitch sweep control's handler: a positive value sweeps down by that
    // amount with decay 0x3F7FE5C9, anything else sweeps up by twice its
    // magnitude with decay 0x3F7FEDFA.
    void setPitchSweep(float value);

    // OutputPanel::UpdateBPM keeps beats per sample as bpm / 60 / 44100 in a
    // double; SubSynth::OnBPMChanged passes the tempo to both LFOs.
    void setTempo(float bpm);
    double beatsPerSample() const { return beatsPerSample_; }

    // SetLFO1Target and SetLFO2Target store the target and rerun ConnectLFOs.
    void setLFOTargets(LFO::Target target1, LFO::Target target2);
    LFO::Target lfo1Target() const { return lfo1Target_; }
    LFO::Target lfo2Target() const { return lfo2Target_; }

    // SetPolyphony. Values are clamped to 1 .. 8; one voice makes the machine
    // monophonic, which is what enables sliding notes (SupportsGlide). Every
    // voice slot is reset, whatever the count.
    void setPolyphony(int requested);
    int polyphony() const { return polyphony_; }
    bool isMonophonic() const { return monophonic_; }

    // Host entry points, all of which queue rather than play: the engine
    // applies queued events at the top of the next process call, under a
    // mutex, so they are safe to call from another thread.
    //
    // OnSequencerKeyEvent: NoteOn and NoteOff events are queued, anything else
    // belongs to the pattern editor and is ignored here.
    void onKeyEvent(const SequencerKeyEvent &event);
    // SlavePlayNote and SlaveStopNote. The event's type decides what the
    // queue does with it.
    void queueKeyEvent(const SequencerKeyEvent &event);
    // PreviewKeyOff without the scale filter: while the sustain pedal is down
    // the note-off is held back, otherwise it is queued (DoKeyOff).
    void previewKeyOff(const SequencerKeyEvent &event);

    // Control 0x401 in OnSequencerControlEvent: down at 0.5 and above.
    // Lifting it releases every held-back note-off.
    void setSustain(bool down);
    bool isSustainDown() const { return sustain_; }
    void releaseSustainedNotes();
    uint sustainedCount() const { return sustainedCount_; }

    // OnMIDIFocusLost: release every gated live-input note, lift the pedal
    // and release what it held.
    void focusLost();

    // Audio-thread note handling, run by processPendingNotes.
    void playNote(const SequencerKeyEvent &event);
    void stopNote(const SequencerKeyEvent &event);
    void stopAllNotes();
    void processPendingNotes();
    uint pendingCount() const { return pendingCount_; }

    // SubSynth::Process without the pattern editor and the VU meter. The LFOs
    // are locked to songPositionBeats, converted to samples as the engine does.
    // numSamples must not exceed SubSynthVoice::kMaxBlockSamples.
    void process(int *output, uint numSamples, double songPositionBeats = 0.0);

    // The LFO position Process derives: (uint)((uint)(1 / beatsPerSample) * beats).
    uint lfoPosition(double songPositionBeats) const;

    // GetNumNotesPlaying (active voices) and GetNumNotesOn (gated voices),
    // both counted over the first polyphony slots only.
    int notesPlaying() const;
    int notesOn() const;

    // +0xC2BC: the last event a monophonic note-on played, which the next
    // sliding note measures its glide against.
    const SequencerKeyEvent &lastMonophonicEvent() const { return lastEvent_; }

    // The glide length PlayChannel sets for a sliding note, in samples.
    uint glideSamplesFor(const SequencerKeyEvent &event) const;

private:
    void playChannel(const SequencerKeyEvent &event, uint voice, bool stealing);
    static void resetVoltage(ControlVoltage &voltage);

    int polyphony_ = kDefaultPolyphony;
    bool monophonic_ = false;

    // +0xC2F9 and +0xC2FA. SetPolyphony and PlayChannel branch on the first,
    // but nothing in this build ever sets it, so the second (set when a queue
    // batch contained a note-off) never matters either. Kept so the branches
    // can be written as the engine has them.
    bool flagC2F9_ = false;
    bool flagC2FA_ = false;

    bool sustain_ = false; // +0x2F04

    LFO::Target lfo1Target_ = LFO::Target::None;
    LFO::Target lfo2Target_ = LFO::Target::None;

    double beatsPerSample_ = 0.0;

    SequencerKeyEvent lastEvent_;

    std::mutex pendingMutex_; // +0xC2B4
    std::array<SequencerKeyEvent, kQueueCapacity> pending_{};
    uint pendingCount_ = 0;

    std::array<SequencerKeyEvent, kQueueCapacity> sustained_{};
    uint sustainedCount_ = 0;
};
