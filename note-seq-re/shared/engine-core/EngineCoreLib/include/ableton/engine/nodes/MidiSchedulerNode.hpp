#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/MidiSchedulerNode.cpp
#include "ableton/support/Stubs.hpp"

namespace ableton::engine {

    struct MidiSchedulerNode {
        struct QueuedMidiMessage {
            // TODO: fields
        };

        struct TimedMidiMessage {
            // TODO: fields
        };

        void enqueueMessage(TimedMidiMessage const&, ableton::datatype::Distance<ableton::datatype::Unit<ableton::time::units::SecondKind, std::ratio<1l, 1000000000l>>, unsigned long long>, EventBuffer<std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, Frames, NoopEventBufferOverflowHandler<std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, Frames>>&);  // @0xfd070 -> decomp/MidiSchedulerNode_enqueueMessage__00000000000fd070.c
        void dequeueDueMessage(ProcessContext const&);  // @0xfd244 -> decomp/MidiSchedulerNode_dequeueDueMessage__00000000000fd244.c
        MidiSchedulerNode(unsigned long);  // @0xfce54 -> decomp/MidiSchedulerNode_MidiSchedulerNode__00000000000fce54.c; @0xfcdb4 -> decomp/MidiSchedulerNode_MidiSchedulerNode__00000000000fcdb4.c
        void playbackTime(std::optional<ableton::datatype::Distance<ableton::datatype::Unit<ableton::time::units::SecondKind, std::ratio<1l, 1000000000l>>, unsigned long long>>, Seconds, ableton::datatype::Distance<ableton::datatype::Unit<ableton::time::units::SecondKind, std::ratio<1l, 1000000000l>>, unsigned long long>) const;  // @0xfd398 -> decomp/MidiSchedulerNode_playbackTime__00000000000fd398.c
    };

}  // namespace ableton::engine
