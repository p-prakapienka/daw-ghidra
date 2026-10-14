// Reconstructed skeleton (stubs). Original: shared/engine-core/EngineCoreLib/src/nodes/MidiSchedulerNode.cpp
#include "ableton/engine/nodes/MidiSchedulerNode.hpp"

// Assertions seen in the binary for this file (line: condition):
//   line 55: mPendingMessages.empty()

namespace ableton::engine {

// @0xfd070 -> decomp/MidiSchedulerNode_enqueueMessage__00000000000fd070.c
void MidiSchedulerNode::enqueueMessage(TimedMidiMessage const& p0, ableton::datatype::Distance<ableton::datatype::Unit<ableton::time::units::SecondKind, std::ratio<1l, 1000000000l>>, unsigned long long> p1, EventBuffer<std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, Frames, NoopEventBufferOverflowHandler<std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, Frames>>& p2) {
    // TODO
}

// @0xfd244 -> decomp/MidiSchedulerNode_dequeueDueMessage__00000000000fd244.c
void MidiSchedulerNode::dequeueDueMessage(ProcessContext const& p0) {
    // TODO
}

// @0xfce54 -> decomp/MidiSchedulerNode_MidiSchedulerNode__00000000000fce54.c; @0xfcdb4 -> decomp/MidiSchedulerNode_MidiSchedulerNode__00000000000fcdb4.c
MidiSchedulerNode::MidiSchedulerNode(unsigned long p0) {
    // TODO
}

// @0xfd398 -> decomp/MidiSchedulerNode_playbackTime__00000000000fd398.c
void MidiSchedulerNode::playbackTime(std::optional<ableton::datatype::Distance<ableton::datatype::Unit<ableton::time::units::SecondKind, std::ratio<1l, 1000000000l>>, unsigned long long>> p0, Seconds p1, ableton::datatype::Distance<ableton::datatype::Unit<ableton::time::units::SecondKind, std::ratio<1l, 1000000000l>>, unsigned long long> p2) const {
    // TODO
}

}  // namespace ableton::engine

