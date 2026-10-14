// Reconstructed skeleton (stubs). Original: shared/engine-core/EngineCoreLib/src/nodes/NoteRepeatNode.cpp
#include "ableton/engine/nodes/NoteRepeatNode.hpp"

namespace ableton::engine {

// @0x101514 -> decomp/NoteRepeatNode_Context_sendMidi__0000000000101514.c
void NoteRepeatNode::Context::sendMidi(std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset> const& p0) const {
    // TODO
}

}  // namespace ableton::engine

namespace ableton::engine {

// @0x100438 -> decomp/NoteRepeatNode_ActiveNotes_add__0000000000100438.c
void NoteRepeatNode::ActiveNotes::add(int p0, std::variant<NoteRepeatNode::PendingStart, NoteRepeatNode::Playing, NoteRepeatNode::PendingStop, NoteRepeatNode::PendingOneShot, NoteRepeatNode::PendingStopForGlide> const& p1) {
    // TODO
}

// @0x1004f4 -> decomp/NoteRepeatNode_ActiveNotes_find__00000000001004f4.c
void NoteRepeatNode::ActiveNotes::find(int p0, unsigned long long p1) {
    // TODO
}

// @0x100770 -> decomp/NoteRepeatNode_ActiveNotes_find__0000000000100770.c
void NoteRepeatNode::ActiveNotes::find(unsigned long long p0) {
    // TODO
}

// @0x100864 -> decomp/NoteRepeatNode_ActiveNotes_empty__0000000000100864.c
bool NoteRepeatNode::ActiveNotes::empty() const {
    // TODO
    return {};
}

}  // namespace ableton::engine

namespace ableton::engine {

// @0x101070 -> decomp/NoteRepeatNode_processNoteOn__0000000000101070.c
void NoteRepeatNode::processNoteOn(Context const& p0, ableton::midi::NoteOn const& p1) {
    // TODO
}

// @0x10292c -> decomp/NoteRepeatNode_processDisable__000000000010292c.c
void NoteRepeatNode::processDisable(Context const& p0) {
    // TODO
}

// @0x1015d8 -> decomp/NoteRepeatNode_processNoteOff__00000000001015d8.c
void NoteRepeatNode::processNoteOff(Context const& p0, ableton::midi::NoteOff const& p1) {
    // TODO
}

// @0x102154 -> decomp/NoteRepeatNode_processGridTick__0000000000102154.c
void NoteRepeatNode::processGridTick(Context const& p0) {
    // TODO
}

// @0x102670 -> decomp/NoteRepeatNode_processGlideReleases__0000000000102670.c
void NoteRepeatNode::processGlideReleases(Context const& p0) {
    // TODO
}

// @0x102068 -> decomp/NoteRepeatNode_trackPerNoteControlChange__0000000000102068.c
void NoteRepeatNode::trackPerNoteControlChange(ableton::midi::PerNoteControlChange const& p0) {
    // TODO
}

// @0x101e1c -> decomp/NoteRepeatNode_processPerNoteControlChange__0000000000101e1c.c
void NoteRepeatNode::processPerNoteControlChange(Context const& p0, ableton::midi::PerNoteControlChange const& p1) {
    // TODO
}

// @0x101d38 -> decomp/NoteRepeatNode_removePerNoteControlChanges__0000000000101d38.c
void NoteRepeatNode::removePerNoteControlChanges(unsigned long long p0) {
    // TODO
}

// @0x102ed8 -> decomp/NoteRepeatNode_sendInitialPerNoteControlChanges__0000000000102ed8.c
void NoteRepeatNode::sendInitialPerNoteControlChanges(Context const& p0, unsigned long long p1, unsigned long long p2) {
    // TODO
}

// @0x100898 -> decomp/NoteRepeatNode_NoteRepeatNode__0000000000100898.c; @0x100874 -> decomp/NoteRepeatNode_NoteRepeatNode__0000000000100874.c
NoteRepeatNode::NoteRepeatNode(Frames p0) {
    // TODO
}

// @0x102fcc -> decomp/NoteRepeatNode_outputVelocity__0000000000102fcc.c
int NoteRepeatNode::outputVelocity(unsigned long long p0, float p1) const {
    // TODO
    return {};
}

// @0x102e40 -> decomp/NoteRepeatNode_rangeOfPerNoteControlChanges__0000000000102e40.c
void NoteRepeatNode::rangeOfPerNoteControlChanges(unsigned long long p0) const {
    // TODO
}

}  // namespace ableton::engine

