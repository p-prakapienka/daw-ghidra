// Reconstructed skeleton (stubs). Original: shared/engine-core/EngineCoreLib/src/nodes/MidiPlayerNode.cpp
#include "ableton/engine/nodes/MidiPlayerNode.hpp"

// Assertions seen in the binary for this file (line: condition):
//   line 119: !perNoteAutomation.empty()
//   line 177: beatTime >= groovedSegment.start.time && beatTime <= groovedSegment.end.time
//   line 320: newContext.content()
//   line 337: oOldContext->content()
//   line 373: pOldNote
//   line 393: oldContext.isPlaying()
//   line 394: newContext.isPlaying()
//   line 586: mActiveNoteNumbers.any()
//   line 588: context.content()
//   line 625: msg.ramp.duration == 0.0_beats
//   line 642: ABL_IMPLIES(moLastTimeInfo, rangeContext.oOldGrooveRef)
//   line 825: mActiveNoteNumbers[size_t(msg.noteNumber)]
//   line 840: mActiveNoteNumbers[size_t(msg.noteNumber)]
//   line 846: msg.ramp.duration == 0.0_beats
//   line 875: noteId != midi::invalidNoteId()

namespace ableton::engine {

// @0xf6a6c -> decomp/MidiPlayerNode_ActiveRamps_uninitializedRampTracker__00000000000f6a6c.c
void MidiPlayerNode::ActiveRamps::uninitializedRampTracker() {
    // TODO
}

// @0xf6a7c -> decomp/MidiPlayerNode_ActiveRamps_add__00000000000f6a7c.c
void MidiPlayerNode::ActiveRamps::add(int p0, int p1) {
    // TODO
}

// @0xf6c14 -> decomp/MidiPlayerNode_ActiveRamps_all__00000000000f6c14.c
void MidiPlayerNode::ActiveRamps::all() {
    // TODO
}

// @0xf6c2c -> decomp/MidiPlayerNode_ActiveRamps_end__00000000000f6c2c.c
void MidiPlayerNode::ActiveRamps::end() {
    // TODO
}

// @0xf6b14 -> decomp/MidiPlayerNode_ActiveRamps_find__00000000000f6b14.c
void MidiPlayerNode::ActiveRamps::find(int p0, int p1) {
    // TODO
}

// @0xf6c24 -> decomp/MidiPlayerNode_ActiveRamps_begin__00000000000f6c24.c
void MidiPlayerNode::ActiveRamps::begin() {
    // TODO
}

// @0xf6b90 -> decomp/MidiPlayerNode_ActiveRamps_findAll__00000000000f6b90.c
void MidiPlayerNode::ActiveRamps::findAll(int p0) {
    // TODO
}

// @0xf6d5c -> decomp/MidiPlayerNode_ActiveRamps_empty__00000000000f6d5c.c
bool MidiPlayerNode::ActiveRamps::empty() const {
    // TODO
    return {};
}

}  // namespace ableton::engine

namespace ableton::engine {

// @0xf7e2c -> decomp/MidiPlayerNode_playEvents__00000000000f7e2c.c
void MidiPlayerNode::playEvents(RangeContext const& p0) {
    // TODO
}

// @0xf7d7c -> decomp/MidiPlayerNode_stopActiveRamp__00000000000f7d7c.c
void MidiPlayerNode::stopActiveRamp(Context const& p0, int p1, int p2, RampTracker<float, Beats>& p3) {
    // TODO
}

// @0xf8148 -> decomp/MidiPlayerNode_processTimeRange__00000000000f8148.c
void MidiPlayerNode::processTimeRange(RangeContext const& p0) {
    // TODO
}

// @0xf86bc -> decomp/MidiPlayerNode_sendCompleteRamp__00000000000f86bc.c
void MidiPlayerNode::sendCompleteRamp(Context const& p0, int p1, int p2, RampTracker<float, Beats>& p3, bool p4, CompleteRamp<float, Beats> const& p5) {
    // TODO
}

// @0xf8468 -> decomp/MidiPlayerNode_stopNoteAndRamps__00000000000f8468.c
void MidiPlayerNode::stopNoteAndRamps(Context const& p0, midi_player::NoteOff const& p1) {
    // TODO
}

// @0xf72ac -> decomp/MidiPlayerNode_seekInAutomations__00000000000f72ac.c
void MidiPlayerNode::seekInAutomations(Context const& p0, Context const& p1, int p2, midi_player::Note const& p3, midi_player::Note const& p4) {
    // TODO
}

// @0xf76c4 -> decomp/MidiPlayerNode_stopNotesAndRamps__00000000000f76c4.c
void MidiPlayerNode::stopNotesAndRamps(Context const& p0, std::bitset<128ul> const& p1) {
    // TODO
}

// @0xf7f98 -> decomp/MidiPlayerNode_playPendingNoteOffs__00000000000f7f98.c
void MidiPlayerNode::playPendingNoteOffs(Context const& p0) {
    // TODO
}

// @0xf7ac8 -> decomp/MidiPlayerNode_sendActiveRampsWithNewTempo__00000000000f7ac8.c
void MidiPlayerNode::sendActiveRampsWithNewTempo(Context const& p0) {
    // TODO
}

// @0xf6e04 -> decomp/MidiPlayerNode_seek__00000000000f6e04.c
void MidiPlayerNode::seek(std::optional<Context> const& p0, Context const& p1) {
    // TODO
}

// @0xf7bb0 -> decomp/MidiPlayerNode_sendRamp__00000000000f7bb0.c
void MidiPlayerNode::sendRamp(Context const& p0, RampTracker<float, Beats>& p1, bool p2, midi_player::PerNoteCCRamp const& p3) {
    // TODO
}

// @0xf87f0 -> decomp/MidiPlayerNode_sendRamp__00000000000f87f0.c
void MidiPlayerNode::sendRamp(Context const& p0, bool p1, midi_player::PerNoteCCRamp const& p2) {
    // TODO
}

// @0xf83e0 -> decomp/MidiPlayerNode_stopNote__00000000000f83e0.c
void MidiPlayerNode::stopNote(Context const& p0, midi_player::NoteOff const& p1) {
    // TODO
}

// @0xf8350 -> decomp/MidiPlayerNode_startNote__00000000000f8350.c
void MidiPlayerNode::startNote(Context const& p0, midi_player::NoteOn const& p1) {
    // TODO
}

// @0xf6db8 -> decomp/MidiPlayerNode_MidiPlayerNode__00000000000f6db8.c; @0xf6d6c -> decomp/MidiPlayerNode_MidiPlayerNode__00000000000f6d6c.c
MidiPlayerNode::MidiPlayerNode() {
    // TODO
}

}  // namespace ableton::engine

