// Reconstructed skeleton (stubs). Original: shared/engine/EngineLib/src/layers/recorders/NotesRecorder.cpp
#include "layers/recorders/NotesRecorder.hpp"

// Assertions seen in the binary for this file (line: condition):
//   line 243: wasInserted

namespace ableton::engine {

// @0x3a02fc -> decomp/NotesRecorder_handleNoteOff__00000000003a02fc.c
void NotesRecorder::handleNoteOff(RecordingContext const& p0, RealtimeRecordingState const& p1, ableton::midi::NoteOff const& p2) {
    // TODO
}

// @0x3a0c64 -> decomp/NotesRecorder_onDispatchEnd__00000000003a0c64.c
void NotesRecorder::onDispatchEnd(RecordingContext const& p0, RealtimeRecordingState const& p1) {
    // TODO
}

// @0x39f588 -> decomp/NotesRecorder_onBeatTimeJump__000000000039f588.c
void NotesRecorder::onBeatTimeJump(RecordingContext const& p0, RealtimeRecordingStates const& p1) {
    // TODO
}

// @0x39f3f8 -> decomp/NotesRecorder_insertQueuedNotes__000000000039f3f8.c
void NotesRecorder::insertQueuedNotes(RecordingContext const& p0) {
    // TODO
}

// @0x3a0cd4 -> decomp/NotesRecorder_noteForClipInsertion__00000000003a0cd4.c
void NotesRecorder::noteForClipInsertion(RecordingContext const& p0, RealtimeRecordingState const& p1, std::variant<std::monostate, UnrecordedNote, RecordingNote, WrappedRecordingNote>&& p2, ableton::midi::NoteOff const& p3, bool p4) {
    // TODO
}

// @0x39f81c -> decomp/NotesRecorder_onClipStartedRecording__000000000039f81c.c
void NotesRecorder::onClipStartedRecording(RecordingContext const& p0, RealtimeRecordingState const& p1) {
    // TODO
}

// @0x39f1ec -> decomp/NotesRecorder_onClipStoppedRecording__000000000039f1ec.c
void NotesRecorder::onClipStoppedRecording(RecordingContext const& p0, RealtimeRecordingState const& p1) {
    // TODO
}

// @0x3a0af0 -> decomp/NotesRecorder_onPerNoteControlChange__00000000003a0af0.c
void NotesRecorder::onPerNoteControlChange(RecordingContext const& p0, RealtimeRecordingState const& p1, ableton::midi::PerNoteControlChange const& p2) {
    // TODO
}

// @0x3a0c68 -> decomp/NotesRecorder_noteId__00000000003a0c68.c
void NotesRecorder::noteId(std::variant<std::monostate, UnrecordedNote, RecordingNote, WrappedRecordingNote> const& p0) {
    // TODO
}

// @0x39fa3c -> decomp/NotesRecorder_onNoteOn__000000000039fa3c.c
void NotesRecorder::onNoteOn(RecordingContext const& p0, RealtimeRecordingState const& p1, ableton::midi::NoteOn const& p2) {
    // TODO
}

// @0x3a0aec -> decomp/NotesRecorder_onNoteOff__00000000003a0aec.c
void NotesRecorder::onNoteOff(RecordingContext const& p0, RealtimeRecordingState const& p1, ableton::midi::NoteOff const& p2) {
    // TODO
}

// @0x39f194 -> decomp/NotesRecorder_NotesRecorder__000000000039f194.c; @0x39f13c -> decomp/NotesRecorder_NotesRecorder__000000000039f13c.c
NotesRecorder::NotesRecorder(RecordingTransaction p0) {
    // TODO
}

// @0x39cee8 -> decomp/NotesRecorder_NotesRecorder__000000000039cee8.c
NotesRecorder::~NotesRecorder() {
    // TODO
}

// @0x3a0984 -> decomp/NotesRecorder_activeNoteNumber__00000000003a0984.c
void NotesRecorder::activeNoteNumber(unsigned long long p0) const {
    // TODO
}

// @0x3a0190 -> decomp/NotesRecorder_extractInitialPerNoteAutomations__00000000003a0190.c
void NotesRecorder::extractInitialPerNoteAutomations(unsigned long long p0) const {
    // TODO
}

}  // namespace ableton::engine

