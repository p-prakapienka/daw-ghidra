#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/OneShotNoteNode.cpp  (path guessed)
#include "ableton/support/Stubs.hpp"

namespace ableton::engine {

    struct OneShotNoteNode {
        struct Note {
            // TODO: fields
        };

        struct ScheduledNoteEnd {
            // TODO: fields
        };

        void startNote(Frames, Note const&, EventBuffer<std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, Frames, NoopEventBufferOverflowHandler<std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, Frames>>&, ableton::datatype::Quotient<ableton::datatype::Unit<ableton::time::units::FrameKind, std::ratio<1l, 1l>>, ableton::datatype::Unit<ableton::time::units::SecondKind, std::ratio<1l, 1l>>, double>);  // @0x105a9c -> decomp/OneShotNoteNode_startNote__0000000000105a9c.c
        OneShotNoteNode();  // @0x105654 -> decomp/OneShotNoteNode_OneShotNoteNode__0000000000105654.c; @0x10564c -> decomp/OneShotNoteNode_OneShotNoteNode__000000000010564c.c
    };

}  // namespace ableton::engine
