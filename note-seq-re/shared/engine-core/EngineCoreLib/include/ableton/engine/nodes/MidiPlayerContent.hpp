#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/MidiPlayerContent.cpp
#include "ableton/support/Stubs.hpp"

namespace ableton::engine {

    namespace midi_player {
        struct Note {
            // TODO: fields
        };

        struct NoteOn {
            // TODO: fields
        };

        struct NoteOff {
            // TODO: fields
        };

        struct PerNoteCCRamp {
            // TODO: fields
        };

        struct InitialPerNoteCCRamp {
            // TODO: fields
        };

        struct CompressedMessage {
            CompressedMessage(std::variant<midi_player::PerNoteCCRamp, midi_player::NoteOff, midi_player::NoteOn, midi_player::InitialPerNoteCCRamp> const&);  // @0xf3bd0 -> decomp/midi_player_CompressedMessage_CompressedMessage__00000000000f3bd0.c; @0xf3b48 -> decomp/midi_player_CompressedMessage_CompressedMessage__00000000000f3b48.c
            CompressedMessage(unsigned long, int, int, float, Beats);  // @0xf3c70 -> decomp/midi_player_CompressedMessage_CompressedMessage__00000000000f3c70.c; @0xf3c58 -> decomp/midi_player_CompressedMessage_CompressedMessage__00000000000f3c58.c
            void get() const;  // @0xf3c88 -> decomp/midi_player_CompressedMessage_get__00000000000f3c88.c
        };

        struct Content {
            Content(boost::container::flat_map<int, std::vector<midi_player::Note>, std::less<int>, void>, std::optional<int> const&);  // @0xf498c -> decomp/midi_player_Content_Content__00000000000f498c.c; @0xf4054 -> decomp/midi_player_Content_Content__00000000000f4054.c
        };

    }  // namespace midi_player

}  // namespace ableton::engine
