// Reconstructed skeleton (stubs). Original: shared/engine-core/EngineCoreLib/src/nodes/MidiPlayerContent.cpp
#include "ableton/engine/nodes/MidiPlayerContent.hpp"

namespace ableton::engine::midi_player {

// @0xf3bd0 -> decomp/midi_player_CompressedMessage_CompressedMessage__00000000000f3bd0.c; @0xf3b48 -> decomp/midi_player_CompressedMessage_CompressedMessage__00000000000f3b48.c
CompressedMessage::CompressedMessage(std::variant<midi_player::PerNoteCCRamp, midi_player::NoteOff, midi_player::NoteOn, midi_player::InitialPerNoteCCRamp> const& p0) {
    // TODO
}

// @0xf3c70 -> decomp/midi_player_CompressedMessage_CompressedMessage__00000000000f3c70.c; @0xf3c58 -> decomp/midi_player_CompressedMessage_CompressedMessage__00000000000f3c58.c
CompressedMessage::CompressedMessage(unsigned long p0, int p1, int p2, float p3, Beats p4) {
    // TODO
}

// @0xf3c88 -> decomp/midi_player_CompressedMessage_get__00000000000f3c88.c
void CompressedMessage::get() const {
    // TODO
}

}  // namespace ableton::engine::midi_player

namespace ableton::engine::midi_player {

// @0xf498c -> decomp/midi_player_Content_Content__00000000000f498c.c; @0xf4054 -> decomp/midi_player_Content_Content__00000000000f4054.c
Content::Content(boost::container::flat_map<int, std::vector<midi_player::Note>, std::less<int>, void> p0, std::optional<int> const& p1) {
    // TODO
}

}  // namespace ableton::engine::midi_player

