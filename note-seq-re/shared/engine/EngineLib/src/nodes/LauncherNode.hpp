#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine/EngineLib/src/nodes/LauncherNode.cpp
#include "ableton/support/Stubs.hpp"
#include "ableton/engine/nodes/TransportFunctions.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ableton::engine {

    namespace launcher {
        struct ChangeOrigin {
            // TODO: fields
        };

        // 0x58 bytes, from Clip::operator= @0x372ff4 and ~Clip @0x3a8efc.
        // The three shared pointers are released in the destructor; what they point to is not known yet.
        template <typename Content>
        struct Clip {
            realtime::SharedPtr<Content const> pContent;   // 0x00 (name and type are ours)
            realtime::SharedPtr<void const> pUnknown10;    // 0x10
            transport::ClipRegion region;                  // 0x20..0x48
            realtime::SharedPtr<void const> pUnknown48;    // 0x48

            ~Clip();  // @0x3a8efc -> decomp/launcher_Clip_midi_player_Content_Clip__00000000003a8efc.c
            Clip& operator=(Clip&&);  // @0x372ff4 -> decomp/launcher_Clip_midi_player_Content_operator___0000000000372ff4.c
        };

        // 0x60 bytes: an optional clip, engaged flag at 0x58.
        template <typename Content>
        struct ClipSlot {
            std::optional<Clip<Content>> oClip;   // name is ours
        };

        // 0x20 bytes. Engine counterpart of flip_model::FPlayingState; field names follow its
        // mSessionClipStartTime / mPlayingClipSlot. No slot means stopped.
        struct PlayingState {
            Beats sessionClipStartTime{};               // 0x00: song beat the playing clip started at
            std::optional<std::size_t> oPlayingClipSlot;  // 0x08 index, 0x10 engaged
            std::uint64_t unknown18{};                  // 0x18: always copied along, meaning unknown
        };

        namespace detail {
            // 0x78 bytes: the clip slots published by the model, plus a temporary clip that
            // stands in for one slot while it records.
            template <typename Content>
            struct ClipSlotsHolder {
                struct TemporaryClip {
                    std::size_t slotIndex{};   // 0x00 (relative to the holder: 0x10)
                    Clip<Content> clip;        // 0x08 (0x18)

                    ~TemporaryClip();  // @0x3a8e4c -> decomp/launcher_detail_ClipSlotsHolder_midi_player_Content_TemporaryClip_TemporaryClip__00000000003a8e4c.c
                };

                void ensureFilledSlot(unsigned long, Beats);  // @0x3a6c60 -> decomp/launcher_detail_ClipSlotsHolder_midi_player_Content_ensureFilledSlot__00000000003a6c60.c

                // Inlined in every caller; the name is ours. The temporary clip wins over the slot it
                // stands in for; otherwise the slot's clip, if the index is valid and the slot is filled.
                Clip<Content> const* clipAt(std::size_t slotIndex) const {
                    if (moTemporaryClip && moTemporaryClip->slotIndex == slotIndex) {
                        return &moTemporaryClip->clip;
                    }
                    if (!mpClipSlots || slotIndex >= mpClipSlots->size()) {
                        return nullptr;
                    }
                    auto const& slot = (*mpClipSlots)[slotIndex];
                    return slot.oClip ? &*slot.oClip : nullptr;
                }

                realtime::SharedPtr<std::vector<ClipSlot<Content>> const> mpClipSlots;   // 0x00
                std::optional<TemporaryClip> moTemporaryClip;                           // 0x10, engaged at 0x70
            };

        }  // namespace detail

        struct State {
            // TODO: fields
        };

        struct Trigger {
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

        // 0x38 bytes. The member holding it is called moPendingTrigger (assertion string).
        struct PendingTrigger {
            Beats quantum{};                     // 0x00: launch grid in beats; 0 launches at once
            std::optional<std::size_t> oClipSlot;  // 0x08 index, 0x10 engaged; empty means stop
            std::int32_t kind{};                 // 0x18: 1 makes the launch record (name is ours)
            std::uint64_t unknown20{};           // 0x20: passed on to ensureFilledSlot
            std::optional<Beats> oLaunchTime;    // 0x28, engaged at 0x30: fixed launch time (name is ours)
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
        std::optional<Beats> clipStopTimeIfDue(transport::Segment const&) const;

        // Data members. Offsets from launchTimeIfDue, clipStopTimeIfDue and launchPendingClip.
        launcher::detail::ClipSlotsHolder<Content> mClipSlots;   // 0x00 (name is ours)
        std::optional<PendingTrigger> moPendingTrigger;          // 0x78, engaged at 0xb0
        launcher::PlayingState mPlayingState;                     // 0xb8 (name is ours)
        bool mIsRecording{};                                      // 0xd8 (name is ours)  // @0x3a7ae4 -> decomp/LauncherNode_midi_player_Content_clipStopTimeIfDue__00000000003a7ae4.c
    };

}  // namespace ableton::engine
