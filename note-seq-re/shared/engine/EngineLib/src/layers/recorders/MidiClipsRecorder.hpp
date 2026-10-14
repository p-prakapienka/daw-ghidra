#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine/EngineLib/src/layers/recorders/MidiClipsRecorder.cpp
#include "ableton/support/Stubs.hpp"

namespace ableton::engine {

    struct MidiClipsRecorder {
        struct RecordingSession {
            void undoTitle(RecordingConfig const&) const;  // @0x39cdc8 -> decomp/MidiClipsRecorder_RecordingSession_undoTitle__000000000039cdc8.c
        };

        void adjustLoop(RecordingContext const&, RealtimeRecordingState const&);  // @0x39d994 -> decomp/MidiClipsRecorder_adjustLoop__000000000039d994.c
        void onDispatchEnd(RecordingContext const&, RealtimeRecordingState const&);  // @0x39d98c -> decomp/MidiClipsRecorder_onDispatchEnd__000000000039d98c.c
        void onBeatTimeJump(RecordingContext const&, RealtimeRecordingStates const&);  // @0x39d580 -> decomp/MidiClipsRecorder_onBeatTimeJump__000000000039d580.c
        void onRtStateChange(RecordingContext const&, RealtimeRecordingState const&);  // @0x39d6dc -> decomp/MidiClipsRecorder_onRtStateChange__000000000039d6dc.c
        void onClipFrameChange(RecordingContext const&, RealtimeRecordingStates const&);  // @0x39d52c -> decomp/MidiClipsRecorder_onClipFrameChange__000000000039d52c.c
        void endAndBeginUndoStep(ableton::flip_model::TransactionHub&);  // @0x39dd88 -> decomp/MidiClipsRecorder_endAndBeginUndoStep__000000000039dd88.c
        void onClipStartedRecording(RecordingContext const&, RealtimeRecordingState const&);  // @0x39d608 -> decomp/MidiClipsRecorder_onClipStartedRecording__000000000039d608.c
        void onClipStoppedRecording(RecordingContext const&, RealtimeRecordingState const&);  // @0x39d4c0 -> decomp/MidiClipsRecorder_onClipStoppedRecording__000000000039d4c0.c
        void onPerNoteControlChange(RecordingContext const&, RealtimeRecordingState const&, ableton::midi::PerNoteControlChange const&);  // @0x39e1cc -> decomp/MidiClipsRecorder_onPerNoteControlChange__000000000039e1cc.c
        void dispatchPreTransitionMidi(RecordingContext const&, RealtimeRecordingState const&);  // @0x39d370 -> decomp/MidiClipsRecorder_dispatchPreTransitionMidi__000000000039d370.c
        void ensureFilledRecordingSlot(RecordingContext const&, RealtimeRecordingState const&);  // @0x39ddc8 -> decomp/MidiClipsRecorder_ensureFilledRecordingSlot__000000000039ddc8.c
        void dispatchPostTransitionMidi(RecordingContext const&, RealtimeRecordingState const&);  // @0x39d838 -> decomp/MidiClipsRecorder_dispatchPostTransitionMidi__000000000039d838.c
        void change(std::function<void ()> const&);  // @0x39e1dc -> decomp/MidiClipsRecorder_change__000000000039e1dc.c
        void process(RecordingContext const&, RealtimeRecordingStates const&);  // @0x39d038 -> decomp/MidiClipsRecorder_process__000000000039d038.c
        void onNoteOn(RecordingContext const&, RealtimeRecordingState const&, ableton::midi::NoteOn const&);  // @0x39e1bc -> decomp/MidiClipsRecorder_onNoteOn__000000000039e1bc.c
        void onNoteOff(RecordingContext const&, RealtimeRecordingState const&, ableton::midi::NoteOff const&);  // @0x39e1c4 -> decomp/MidiClipsRecorder_onNoteOff__000000000039e1c4.c
        MidiClipsRecorder(RecordingConfig const&, ParameterIdRegistry&);  // @0x39d034 -> decomp/MidiClipsRecorder_MidiClipsRecorder__000000000039d034.c; @0x39cde0 -> decomp/MidiClipsRecorder_MidiClipsRecorder__000000000039cde0.c
        ~MidiClipsRecorder();  // @0x1d32f4 -> decomp/MidiClipsRecorder_MidiClipsRecorder__00000000001d32f4.c
        bool isRecordingSessionActive() const;  // @0x39e1d4 -> decomp/MidiClipsRecorder_isRecordingSessionActive__000000000039e1d4.c
    };

}  // namespace ableton::engine
