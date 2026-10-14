// Reconstructed skeleton (stubs). Original: shared/engine/EngineLib/src/layers/MidiTrackEventStreamReader.cpp
#include "layers/MidiTrackEventStreamReader.hpp"

namespace ableton::engine {

// @0x25a4d8 -> decomp/MidiTrackEventStreamReader_pruneRemovedParameters__000000000025a4d8.c
void MidiTrackEventStreamReader::pruneRemovedParameters(ParameterIdRegistry const& p0) {
    // TODO
}

// @0x25a1d0 -> decomp/MidiTrackEventStreamReader_exchangeIsGestureActive__000000000025a1d0.c
void MidiTrackEventStreamReader::exchangeIsGestureActive(ModelId<ParameterIdTag> const& p0, bool p1) {
    // TODO
}

// @0x25a39c -> decomp/MidiTrackEventStreamReader_sendParameterValueChange__000000000025a39c.c
void MidiTrackEventStreamReader::sendParameterValueChange(ReaderContext const& p0, ModelId<ParameterIdTag> const& p1, float p2, ableton::flip_model::EngineSignal<ableton::flip_model::EngineEvent<std::variant<ableton::flip_model::PlayingClipChange, ableton::flip_model::SongTimelineChange, ableton::flip_model::ClipTimelineChange, std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, ableton::flip_model::ParameterGestureChange, ableton::flip_model::ParameterValueChange>, Frames>>& p3) {
    // TODO
}

// @0x25a258 -> decomp/MidiTrackEventStreamReader_sendParameterGestureChange__000000000025a258.c
void MidiTrackEventStreamReader::sendParameterGestureChange(ReaderContext const& p0, ModelId<ParameterIdTag> const& p1, bool p2, ableton::flip_model::EngineSignal<ableton::flip_model::EngineEvent<std::variant<ableton::flip_model::PlayingClipChange, ableton::flip_model::SongTimelineChange, ableton::flip_model::ClipTimelineChange, std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset>, ableton::flip_model::ParameterGestureChange, ableton::flip_model::ParameterValueChange>, Frames>>& p3) {
    // TODO
}

// @0x259efc -> decomp/MidiTrackEventStreamReader_MidiTrackEventStreamReader__0000000000259efc.c; @0x259d38 -> decomp/MidiTrackEventStreamReader_MidiTrackEventStreamReader__0000000000259d38.c
MidiTrackEventStreamReader::MidiTrackEventStreamReader(ParameterIdRegistry& p0) {
    // TODO
}

// @0x25a380 -> decomp/MidiTrackEventStreamReader_isGestureActive__000000000025a380.c
void MidiTrackEventStreamReader::isGestureActive(ModelId<ParameterIdTag> const& p0) const {
    // TODO
}

}  // namespace ableton::engine

