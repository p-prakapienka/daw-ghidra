#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine/EngineLib/src/layers/recorders/NotesRecorder.cpp
#include "ableton/support/Stubs.hpp"

namespace ableton::engine {

    struct NotesRecorder {
        struct QueuedNote {
            // TODO: fields
        };

        struct RecordingNote {
            // TODO: fields
        };

        struct UnrecordedNote {
            // TODO: fields
        };

        struct WrappedRecordingNote {
            // TODO: fields
        };

        void handleNoteOff(RecordingContext const&, RealtimeRecordingState const&, ableton::midi::NoteOff const&);  // @0x3a02fc -> decomp/NotesRecorder_handleNoteOff__00000000003a02fc.c
        void onDispatchEnd(RecordingContext const&, RealtimeRecordingState const&);  // @0x3a0c64 -> decomp/NotesRecorder_onDispatchEnd__00000000003a0c64.c
        void onBeatTimeJump(RecordingContext const&, RealtimeRecordingStates const&);  // @0x39f588 -> decomp/NotesRecorder_onBeatTimeJump__000000000039f588.c
        void insertQueuedNotes(RecordingContext const&);  // @0x39f3f8 -> decomp/NotesRecorder_insertQueuedNotes__000000000039f3f8.c
        void noteForClipInsertion(RecordingContext const&, RealtimeRecordingState const&, std::variant<std::monostate, UnrecordedNote, RecordingNote, WrappedRecordingNote>&&, ableton::midi::NoteOff const&, bool);  // @0x3a0cd4 -> decomp/NotesRecorder_noteForClipInsertion__00000000003a0cd4.c
        void onClipStartedRecording(RecordingContext const&, RealtimeRecordingState const&);  // @0x39f81c -> decomp/NotesRecorder_onClipStartedRecording__000000000039f81c.c
        void onClipStoppedRecording(RecordingContext const&, RealtimeRecordingState const&);  // @0x39f1ec -> decomp/NotesRecorder_onClipStoppedRecording__000000000039f1ec.c
        void onPerNoteControlChange(RecordingContext const&, RealtimeRecordingState const&, ableton::midi::PerNoteControlChange const&);  // @0x3a0af0 -> decomp/NotesRecorder_onPerNoteControlChange__00000000003a0af0.c
        void noteId(std::variant<std::monostate, UnrecordedNote, RecordingNote, WrappedRecordingNote> const&);  // @0x3a0c68 -> decomp/NotesRecorder_noteId__00000000003a0c68.c
        void onNoteOn(RecordingContext const&, RealtimeRecordingState const&, ableton::midi::NoteOn const&);  // @0x39fa3c -> decomp/NotesRecorder_onNoteOn__000000000039fa3c.c
        void onNoteOff(RecordingContext const&, RealtimeRecordingState const&, ableton::midi::NoteOff const&);  // @0x3a0aec -> decomp/NotesRecorder_onNoteOff__00000000003a0aec.c
        NotesRecorder(RecordingTransaction);  // @0x39f194 -> decomp/NotesRecorder_NotesRecorder__000000000039f194.c; @0x39f13c -> decomp/NotesRecorder_NotesRecorder__000000000039f13c.c
        ~NotesRecorder();  // @0x39cee8 -> decomp/NotesRecorder_NotesRecorder__000000000039cee8.c
        void activeNoteNumber(unsigned long long) const;  // @0x3a0984 -> decomp/NotesRecorder_activeNoteNumber__00000000003a0984.c
        void extractInitialPerNoteAutomations(unsigned long long) const;  // @0x3a0190 -> decomp/NotesRecorder_extractInitialPerNoteAutomations__00000000003a0190.c
    };

}  // namespace ableton::engine
