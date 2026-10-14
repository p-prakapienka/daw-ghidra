// Reconstructed skeleton (stubs). Original: shared/engine-core/EngineCoreLib/src/nodes/MidiCombinerNode.cpp
#include "ableton/engine/nodes/MidiCombinerNode.hpp"

namespace ableton::engine {

// @0xf274c -> decomp/MidiCombinerNode_Context_sendMidi__00000000000f274c.c
void MidiCombinerNode::Context::sendMidi(std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset> const& p0) const {
    // TODO
}

}  // namespace ableton::engine

namespace ableton::engine {

// @0xf2a74 -> decomp/MidiCombinerNode_processSub__00000000000f2a74.c
void MidiCombinerNode::processSub(Context const& p0, ableton::midi::PerNoteControlChange const& p1) {
    // TODO
}

// @0xf2810 -> decomp/MidiCombinerNode_processSub__00000000000f2810.c
void MidiCombinerNode::processSub(Context const& p0, ableton::midi::NoteOn const& p1) {
    // TODO
}

// @0xf2a54 -> decomp/MidiCombinerNode_processSub__00000000000f2a54.c
void MidiCombinerNode::processSub(Context const& p0, ableton::midi::NoteOff const& p1) {
    // TODO
}

// @0xf2718 -> decomp/MidiCombinerNode_processMain__00000000000f2718.c
void MidiCombinerNode::processMain(Context const& p0, ableton::midi::PerNoteControlChange const& p1) {
    // TODO
}

// @0xf2144 -> decomp/MidiCombinerNode_processMain__00000000000f2144.c
void MidiCombinerNode::processMain(Context const& p0, ableton::midi::NoteOn const& p1) {
    // TODO
}

// @0xf256c -> decomp/MidiCombinerNode_processMain__00000000000f256c.c
void MidiCombinerNode::processMain(Context const& p0, ableton::midi::NoteOff const& p1) {
    // TODO
}

// @0xf296c -> decomp/MidiCombinerNode_startSubNote__00000000000f296c.c
void MidiCombinerNode::startSubNote(Context const& p0, ableton::midi::NoteOn const& p1) {
    // TODO
}

// @0xf2354 -> decomp/MidiCombinerNode_startMainNote__00000000000f2354.c
void MidiCombinerNode::startMainNote(Context const& p0, ableton::midi::NoteOn const& p1) {
    // TODO
}

// @0xf25d8 -> decomp/MidiCombinerNode_stopActiveNote__00000000000f25d8.c
void MidiCombinerNode::stopActiveNote(Context const& p0, ableton::midi::NoteOff const& p1) {
    // TODO
}

// @0xf2440 -> decomp/MidiCombinerNode_stopActiveSubNotes__00000000000f2440.c
void MidiCombinerNode::stopActiveSubNotes(Context const& p0, int p1) {
    // TODO
}

// @0xf2c50 -> decomp/MidiCombinerNode_startNote__00000000000f2c50.c
void MidiCombinerNode::startNote(Context const& p0, ableton::midi::NoteOn const& p1) {
    // TODO
}

}  // namespace ableton::engine

