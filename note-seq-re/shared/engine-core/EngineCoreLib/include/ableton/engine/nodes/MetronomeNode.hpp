#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/MetronomeNode.cpp
#include "ableton/support/Stubs.hpp"

namespace ableton::engine {

    struct MetronomeNode {
        void stopNote(Frames, EventBuffer<std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, Frames, NoopEventBufferOverflowHandler<std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, Frames>>&);  // @0xf0c10 -> decomp/MetronomeNode_stopNote__00000000000f0c10.c
        void startNote(Frames, bool, EventBuffer<std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, Frames, NoopEventBufferOverflowHandler<std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, Frames>>&);  // @0xf0ad0 -> decomp/MetronomeNode_startNote__00000000000f0ad0.c
    };

}  // namespace ableton::engine
