#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/MidiPlayerNode.cpp
#include "ableton/support/Stubs.hpp"
#include "ableton/engine/nodes/MidiPlayerContent.hpp"

namespace ableton::engine {

    struct MidiPlayerNode {
        struct Context {
            // TODO: fields
        };

        struct RangeContext {
            // TODO: fields
        };

        struct ActiveRamps {
            void uninitializedRampTracker();  // @0xf6a6c -> decomp/MidiPlayerNode_ActiveRamps_uninitializedRampTracker__00000000000f6a6c.c
            void add(int, int);  // @0xf6a7c -> decomp/MidiPlayerNode_ActiveRamps_add__00000000000f6a7c.c
            void all();  // @0xf6c14 -> decomp/MidiPlayerNode_ActiveRamps_all__00000000000f6c14.c
            void end();  // @0xf6c2c -> decomp/MidiPlayerNode_ActiveRamps_end__00000000000f6c2c.c
            void find(int, int);  // @0xf6b14 -> decomp/MidiPlayerNode_ActiveRamps_find__00000000000f6b14.c
            void begin();  // @0xf6c24 -> decomp/MidiPlayerNode_ActiveRamps_begin__00000000000f6c24.c
            void findAll(int);  // @0xf6b90 -> decomp/MidiPlayerNode_ActiveRamps_findAll__00000000000f6b90.c
            bool empty() const;  // @0xf6d5c -> decomp/MidiPlayerNode_ActiveRamps_empty__00000000000f6d5c.c
        };

        void playEvents(RangeContext const&);  // @0xf7e2c -> decomp/MidiPlayerNode_playEvents__00000000000f7e2c.c
        void stopActiveRamp(Context const&, int, int, RampTracker<float, Beats>&);  // @0xf7d7c -> decomp/MidiPlayerNode_stopActiveRamp__00000000000f7d7c.c
        void processTimeRange(RangeContext const&);  // @0xf8148 -> decomp/MidiPlayerNode_processTimeRange__00000000000f8148.c
        void sendCompleteRamp(Context const&, int, int, RampTracker<float, Beats>&, bool, CompleteRamp<float, Beats> const&);  // @0xf86bc -> decomp/MidiPlayerNode_sendCompleteRamp__00000000000f86bc.c
        void stopNoteAndRamps(Context const&, midi_player::NoteOff const&);  // @0xf8468 -> decomp/MidiPlayerNode_stopNoteAndRamps__00000000000f8468.c
        void seekInAutomations(Context const&, Context const&, int, midi_player::Note const&, midi_player::Note const&);  // @0xf72ac -> decomp/MidiPlayerNode_seekInAutomations__00000000000f72ac.c
        void stopNotesAndRamps(Context const&, std::bitset<128ul> const&);  // @0xf76c4 -> decomp/MidiPlayerNode_stopNotesAndRamps__00000000000f76c4.c
        void playPendingNoteOffs(Context const&);  // @0xf7f98 -> decomp/MidiPlayerNode_playPendingNoteOffs__00000000000f7f98.c
        void sendActiveRampsWithNewTempo(Context const&);  // @0xf7ac8 -> decomp/MidiPlayerNode_sendActiveRampsWithNewTempo__00000000000f7ac8.c
        void seek(std::optional<Context> const&, Context const&);  // @0xf6e04 -> decomp/MidiPlayerNode_seek__00000000000f6e04.c
        void sendRamp(Context const&, RampTracker<float, Beats>&, bool, midi_player::PerNoteCCRamp const&);  // @0xf7bb0 -> decomp/MidiPlayerNode_sendRamp__00000000000f7bb0.c
        void sendRamp(Context const&, bool, midi_player::PerNoteCCRamp const&);  // @0xf87f0 -> decomp/MidiPlayerNode_sendRamp__00000000000f87f0.c
        void stopNote(Context const&, midi_player::NoteOff const&);  // @0xf83e0 -> decomp/MidiPlayerNode_stopNote__00000000000f83e0.c
        void startNote(Context const&, midi_player::NoteOn const&);  // @0xf8350 -> decomp/MidiPlayerNode_startNote__00000000000f8350.c
        MidiPlayerNode();  // @0xf6db8 -> decomp/MidiPlayerNode_MidiPlayerNode__00000000000f6db8.c; @0xf6d6c -> decomp/MidiPlayerNode_MidiPlayerNode__00000000000f6d6c.c
    };

}  // namespace ableton::engine
