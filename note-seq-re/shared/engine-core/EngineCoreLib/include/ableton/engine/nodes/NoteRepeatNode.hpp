#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/NoteRepeatNode.cpp
#include "ableton/support/Stubs.hpp"

namespace ableton::engine {

    struct NoteRepeatNode {
        struct Context {
            void sendMidi(std::variant<ableton::midi::NoteOn, ableton::midi::NoteOff, ableton::midi::ControlChange, ableton::midi::PerNoteControlChange, ableton::midi::RealtimeStart, ableton::midi::RealtimeContinue, ableton::midi::RealtimeStop, ableton::midi::RealtimeClock, ableton::midi::RealtimeReset> const&) const;  // @0x101514 -> decomp/NoteRepeatNode_Context_sendMidi__0000000000101514.c
        };

        struct PendingOneShot {
            // TODO: fields
        };

        struct PendingStart {
            // TODO: fields
        };

        struct PendingStop {
            // TODO: fields
        };

        struct PendingStopForGlide {
            // TODO: fields
        };

        struct PerNoteCCMapKey {
            // TODO: fields
        };

        struct Playing {
            // TODO: fields
        };

        struct ActiveNotes {
            void add(int, std::variant<NoteRepeatNode::PendingStart, NoteRepeatNode::Playing, NoteRepeatNode::PendingStop, NoteRepeatNode::PendingOneShot, NoteRepeatNode::PendingStopForGlide> const&);  // @0x100438 -> decomp/NoteRepeatNode_ActiveNotes_add__0000000000100438.c
            void find(int, unsigned long long);  // @0x1004f4 -> decomp/NoteRepeatNode_ActiveNotes_find__00000000001004f4.c
            void find(unsigned long long);  // @0x100770 -> decomp/NoteRepeatNode_ActiveNotes_find__0000000000100770.c
            bool empty() const;  // @0x100864 -> decomp/NoteRepeatNode_ActiveNotes_empty__0000000000100864.c
        };

        void processNoteOn(Context const&, ableton::midi::NoteOn const&);  // @0x101070 -> decomp/NoteRepeatNode_processNoteOn__0000000000101070.c
        void processDisable(Context const&);  // @0x10292c -> decomp/NoteRepeatNode_processDisable__000000000010292c.c
        void processNoteOff(Context const&, ableton::midi::NoteOff const&);  // @0x1015d8 -> decomp/NoteRepeatNode_processNoteOff__00000000001015d8.c
        void processGridTick(Context const&);  // @0x102154 -> decomp/NoteRepeatNode_processGridTick__0000000000102154.c
        void processGlideReleases(Context const&);  // @0x102670 -> decomp/NoteRepeatNode_processGlideReleases__0000000000102670.c
        void trackPerNoteControlChange(ableton::midi::PerNoteControlChange const&);  // @0x102068 -> decomp/NoteRepeatNode_trackPerNoteControlChange__0000000000102068.c
        void processPerNoteControlChange(Context const&, ableton::midi::PerNoteControlChange const&);  // @0x101e1c -> decomp/NoteRepeatNode_processPerNoteControlChange__0000000000101e1c.c
        void removePerNoteControlChanges(unsigned long long);  // @0x101d38 -> decomp/NoteRepeatNode_removePerNoteControlChanges__0000000000101d38.c
        void sendInitialPerNoteControlChanges(Context const&, unsigned long long, unsigned long long);  // @0x102ed8 -> decomp/NoteRepeatNode_sendInitialPerNoteControlChanges__0000000000102ed8.c
        NoteRepeatNode(Frames);  // @0x100898 -> decomp/NoteRepeatNode_NoteRepeatNode__0000000000100898.c; @0x100874 -> decomp/NoteRepeatNode_NoteRepeatNode__0000000000100874.c
        int outputVelocity(unsigned long long, float) const;  // @0x102fcc -> decomp/NoteRepeatNode_outputVelocity__0000000000102fcc.c
        void rangeOfPerNoteControlChanges(unsigned long long) const;  // @0x102e40 -> decomp/NoteRepeatNode_rangeOfPerNoteControlChanges__0000000000102e40.c
    };

}  // namespace ableton::engine
