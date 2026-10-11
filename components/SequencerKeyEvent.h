#pragma once

#include <cstddef>
#include <cstdint>

// The note event the Caustic sequencer hands to a machine, 0x38 bytes.
//
// Recovered from the readers in SubSynth (PlayChannel, PlayNote, StopNote,
// OnSequencerKeyEvent, PreviewKeyOn/Off, OnMIDIFocusLost) and from the reset
// ProcessPendingNotes applies to a consumed queue entry, which gives the
// defaults. See components/SubSynth.md. Fields with no established role keep
// their offset as their name.
struct SequencerKeyEvent {
    static constexpr std::size_t kSize = 0x38;

    // 0x04: what OnSequencerKeyEvent and ProcessPendingNotes switch on.
    enum class Type : std::int32_t {
        None = -1,
        NoteOn = 0,
        NoteOff = 1,
    };

    // 0x1A flag bits.
    // Bit 0 asks a monophonic SubSynth to slide from the previous note:
    // no envelope retrigger, no phase reset, and a glide.
    static constexpr std::uint16_t kSlideFlag = 0x0001;
    // Bit 2 marks a note from live input. PlayChannel copies it into the
    // control voltage flags, PreviewKeyOn/Off pass such events through the
    // keyboard's scale filter, and OnMIDIFocusLost releases only these.
    static constexpr std::uint16_t kLiveInputFlag = 0x0004;

    std::int32_t field00 = -1;
    Type type = Type::None;

    // 0x08, 0x0C: start and duration, in the unit the sequencer counts in.
    // PlayChannel converts their difference to samples through the transport's
    // samples per beat, so they are beats.
    float start = 1000000.0f;
    float duration = 0.0f;

    std::int32_t field10 = 0;
    std::int32_t field14 = 0;

    // 0x18: the note id, 0 .. 127, read as an unsigned halfword.
    std::uint16_t note = 0;
    std::uint16_t flags = 0;

    std::uint16_t field1C = 0;
    std::uint16_t pad1E = 0;
    std::uint8_t field20 = 0;
    std::uint8_t pad21[3] = {};
    std::int32_t field24 = 0;
    std::int32_t field28 = 0;
    std::int32_t field2C = 0;
    std::int32_t field30 = 0;

    // 0x34: velocity, 0 .. 1. Copied into the control voltage on note-on.
    float velocity = 1.0f;

    static SequencerKeyEvent noteOn(int note, float velocity = 1.0f, float start = 0.0f,
                                    float duration = 0.0f) {
        SequencerKeyEvent event;
        event.type = Type::NoteOn;
        event.note = static_cast<std::uint16_t>(note);
        event.velocity = velocity;
        event.start = start;
        event.duration = duration;
        return event;
    }

    static SequencerKeyEvent noteOff(int note) {
        SequencerKeyEvent event;
        event.type = Type::NoteOff;
        event.note = static_cast<std::uint16_t>(note);
        return event;
    }
};

// Every field is four bytes or packed below four, so the host struct is the
// engine's layout exactly.
static_assert(sizeof(SequencerKeyEvent) == SequencerKeyEvent::kSize, "");
static_assert(offsetof(SequencerKeyEvent, type) == 0x04, "");
static_assert(offsetof(SequencerKeyEvent, start) == 0x08, "");
static_assert(offsetof(SequencerKeyEvent, duration) == 0x0C, "");
static_assert(offsetof(SequencerKeyEvent, note) == 0x18, "");
static_assert(offsetof(SequencerKeyEvent, flags) == 0x1A, "");
static_assert(offsetof(SequencerKeyEvent, field20) == 0x20, "");
static_assert(offsetof(SequencerKeyEvent, velocity) == 0x34, "");
