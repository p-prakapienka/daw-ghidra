#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine/EngineLib/src/nodes/LauncherNode.cpp
#include "ableton/support/Stubs.hpp"
#include "ableton/engine/nodes/TransportFunctions.hpp"

namespace ableton::engine {

    namespace launcher {
        namespace detail {
            template <typename Content>
            struct ClipSlotsHolder {
                struct TemporaryClip {
                    ~TemporaryClip();  // @0x3a8e4c -> decomp/launcher_detail_ClipSlotsHolder_midi_player_Content_TemporaryClip_TemporaryClip__00000000003a8e4c.c
                };

                void ensureFilledSlot(unsigned long, Beats);  // @0x3a6c60 -> decomp/launcher_detail_ClipSlotsHolder_midi_player_Content_ensureFilledSlot__00000000003a6c60.c
            };

        }  // namespace detail

        struct ChangeOrigin {
            // TODO: fields
        };

        struct PlayingState {
            // TODO: fields
        };

        struct State {
            // TODO: fields
        };

        struct Trigger {
            // TODO: fields
        };

        template <typename Content>
        struct Clip {
            ~Clip();  // @0x3a8efc -> decomp/launcher_Clip_midi_player_Content_Clip__00000000003a8efc.c
            Clip& operator=(Clip&&);  // @0x372ff4 -> decomp/launcher_Clip_midi_player_Content_operator___0000000000372ff4.c
        };

        template <typename Content>
        struct ClipSlot {
            // TODO: fields
        };

        struct RestartPlayingClip {
            // TODO: fields
        };

        struct StartRecording {
            // TODO: fields
        };

        struct StopRecording {
            // TODO: fields
        };

        struct TriggerRecording {
            // TODO: fields
        };

    }  // namespace launcher

    template <typename Content>
    struct LauncherNode {
        struct AudioContext {
            // TODO: fields
        };

        struct PendingTrigger {
            // TODO: fields
        };

        struct RecordingChange {
            // TODO: fields
        };

        void processCommand(AudioContext const&, launcher::StopRecording);  // @0x3a7724 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a7724.c
        void processCommand(AudioContext const&, launcher::StartRecording);  // @0x3a75e4 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a75e4.c
        void processCommand(AudioContext const&, launcher::RestartPlayingClip);  // @0x3a7598 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a7598.c
        void processCommand(AudioContext const&, launcher::PlayingState const&);  // @0x3a7584 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a7584.c
        void processCommand(AudioContext const&, launcher::TriggerRecording const&);  // @0x3a75f8 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a75f8.c
        void processCommand(AudioContext const&, launcher::Trigger const&);  // @0x3a6a30 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a6a30.c
        void processClipSlots(AudioContext const&, ableton::realtime::SharedPtr<std::vector<launcher::ClipSlot<Content>> const> const&);  // @0x3a66d4 -> decomp/LauncherNode_midi_player_Content_processClipSlots__00000000003a66d4.c
        void finishPlayingClip(AudioContext const&);  // @0x3a7d54 -> decomp/LauncherNode_midi_player_Content_finishPlayingClip__00000000003a7d54.c
        void launchPendingClip(AudioContext const&);  // @0x3a7c44 -> decomp/LauncherNode_midi_player_Content_launchPendingClip__00000000003a7c44.c
        void adjustPlayingClipStartIfNeeded(AudioContext const&);  // @0x3a7d98 -> decomp/LauncherNode_midi_player_Content_adjustPlayingClipStartIfNeeded__00000000003a7d98.c
        void processPendingPlaybackOperations(AudioContext);  // @0x3a7744 -> decomp/LauncherNode_midi_player_Content_processPendingPlaybackOperations__00000000003a7744.c
        void play(AudioContext const&, launcher::ChangeOrigin, launcher::PlayingState const&, RecordingChange);  // @0x3a69fc -> decomp/LauncherNode_midi_player_Content_play__00000000003a69fc.c
        void setState(AudioContext const&, launcher::ChangeOrigin, std::optional<PendingTrigger> const&, launcher::PlayingState const&, bool);  // @0x3a6ec8 -> decomp/LauncherNode_midi_player_Content_setState__00000000003a6ec8.c
        std::optional<Beats> launchTimeIfDue(transport::Segment const&) const;  // @0x3a7b9c -> decomp/LauncherNode_midi_player_Content_launchTimeIfDue__00000000003a7b9c.c
        std::optional<Beats> clipStopTimeIfDue(transport::Segment const&) const;  // @0x3a7ae4 -> decomp/LauncherNode_midi_player_Content_clipStopTimeIfDue__00000000003a7ae4.c
    };

}  // namespace ableton::engine
