#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine/EngineLib/src/layers/MidiTrackEventStreamReader.cpp  (path guessed)
#include "ableton/support/Stubs.hpp"

namespace ableton::engine {

    struct MidiTrackEventStreamReader {
        void pruneRemovedParameters(ParameterIdRegistry const&);  // @0x25a4d8 -> decomp/MidiTrackEventStreamReader_pruneRemovedParameters__000000000025a4d8.c
        void exchangeIsGestureActive(ModelId<ParameterIdTag> const&, bool);  // @0x25a1d0 -> decomp/MidiTrackEventStreamReader_exchangeIsGestureActive__000000000025a1d0.c
        void sendParameterValueChange(ReaderContext const&, ModelId<ParameterIdTag> const&, float, ableton::flip_model::EngineSignal<ableton::flip_model::EngineEvent<std::variant<ableton::flip_model::PlayingClipChange, ableton::flip_model::SongTimelineChange, ableton::flip_model::ClipTimelineChange, std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, ableton::flip_model::ParameterGestureChange, ableton::flip_model::ParameterValueChange>, Frames>>&);  // @0x25a39c -> decomp/MidiTrackEventStreamReader_sendParameterValueChange__000000000025a39c.c
        void sendParameterGestureChange(ReaderContext const&, ModelId<ParameterIdTag> const&, bool, ableton::flip_model::EngineSignal<ableton::flip_model::EngineEvent<std::variant<ableton::flip_model::PlayingClipChange, ableton::flip_model::SongTimelineChange, ableton::flip_model::ClipTimelineChange, std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, ableton::flip_model::ParameterGestureChange, ableton::flip_model::ParameterValueChange>, Frames>>&);  // @0x25a258 -> decomp/MidiTrackEventStreamReader_sendParameterGestureChange__000000000025a258.c
        MidiTrackEventStreamReader(ParameterIdRegistry&);  // @0x259efc -> decomp/MidiTrackEventStreamReader_MidiTrackEventStreamReader__0000000000259efc.c; @0x259d38 -> decomp/MidiTrackEventStreamReader_MidiTrackEventStreamReader__0000000000259d38.c
        void isGestureActive(ModelId<ParameterIdTag> const&) const;  // @0x25a380 -> decomp/MidiTrackEventStreamReader_isGestureActive__000000000025a380.c
    };

}  // namespace ableton::engine
