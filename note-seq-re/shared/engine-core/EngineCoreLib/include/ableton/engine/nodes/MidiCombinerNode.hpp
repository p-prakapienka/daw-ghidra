#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/MidiCombinerNode.cpp
#include "ableton/support/Stubs.hpp"

namespace ableton::engine {

    struct MidiCombinerNode {
        struct Context {
            void sendMidi(std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset> const&) const;  // @0xf274c -> decomp/MidiCombinerNode_Context_sendMidi__00000000000f274c.c
        };

        void processSub(Context const&, ableton::midi::PerNoteControlChange const&);  // @0xf2a74 -> decomp/MidiCombinerNode_processSub__00000000000f2a74.c
        void processSub(Context const&, ableton::midi::NoteOn const&);  // @0xf2810 -> decomp/MidiCombinerNode_processSub__00000000000f2810.c
        void processSub(Context const&, ableton::midi::NoteOff const&);  // @0xf2a54 -> decomp/MidiCombinerNode_processSub__00000000000f2a54.c
        void processMain(Context const&, ableton::midi::PerNoteControlChange const&);  // @0xf2718 -> decomp/MidiCombinerNode_processMain__00000000000f2718.c
        void processMain(Context const&, ableton::midi::NoteOn const&);  // @0xf2144 -> decomp/MidiCombinerNode_processMain__00000000000f2144.c
        void processMain(Context const&, ableton::midi::NoteOff const&);  // @0xf256c -> decomp/MidiCombinerNode_processMain__00000000000f256c.c
        void startSubNote(Context const&, ableton::midi::NoteOn const&);  // @0xf296c -> decomp/MidiCombinerNode_startSubNote__00000000000f296c.c
        void startMainNote(Context const&, ableton::midi::NoteOn const&);  // @0xf2354 -> decomp/MidiCombinerNode_startMainNote__00000000000f2354.c
        void stopActiveNote(Context const&, ableton::midi::NoteOff const&);  // @0xf25d8 -> decomp/MidiCombinerNode_stopActiveNote__00000000000f25d8.c
        void stopActiveSubNotes(Context const&, int);  // @0xf2440 -> decomp/MidiCombinerNode_stopActiveSubNotes__00000000000f2440.c
        void startNote(Context const&, ableton::midi::NoteOn const&);  // @0xf2c50 -> decomp/MidiCombinerNode_startNote__00000000000f2c50.c
    };

}  // namespace ableton::engine
