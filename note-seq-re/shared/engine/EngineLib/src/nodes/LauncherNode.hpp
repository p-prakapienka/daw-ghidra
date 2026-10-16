#pragma once
// Reconstructed from ABLSwiftBridge (Ableton Note), for personal study.
// Original: shared/engine/EngineLib/src/nodes/LauncherNode.hpp (path from an assertion string).
// Notes: LauncherNode.md next to this file.
#include "ableton/support/Stubs.hpp"
#include "ableton/engine/nodes/ClipTimeNode.hpp"
#include "ableton/engine/nodes/TransportFunctions.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

// Assertions seen in the binary for this file (line: condition):
//   line 259: newClipDuration >= 0.0_beats   (ClipSlotsHolder::ensureFilledSlot)

namespace ableton::engine {

    namespace launcher {
        // Why the launcher state changed. setState passes it on in launcher::State, and
        // operator<< prints it as a number. Values from the callers; the names are ours.
        enum class ChangeOrigin : int {
            kCommand = 0,     // processCommand, play
            kLaunch = 1,      // launchPendingClip: a queued trigger came due
            kPlayback = 2,    // finishPlayingClip, adjustPlayingClipStartIfNeeded
            kRestart = 3,     // processCommand(RestartPlayingClip)
            kClipSlots = 4,   // processClipSlots: the playing clip's content changed
        };

        std::ostream& operator<<(std::ostream&, ChangeOrigin);  // @0x3a807c

        // A clip's parameter automation: per device, per parameter, a list of breakpoints in clip
        // beats. The alias name is ours; the type is spelled out in the node's output port.
        using ClipAutomation = std::unordered_map<
            ModelId<DeviceIdTag>,
            realtime::SharedPtr<boost::container::flat_map<
                ModelId<ParameterIdTag>, realtime::SharedPtr<std::vector<Event<float, Beats>> const>> const>>;

        // 0x58 bytes. Each member feeds one of the node's outputs (see setState). The member names
        // are ours; the types come from the outputs. Copy, move and destruction are implicit: the
        // binary has the out-of-line copies the compiler emitted, ~Clip @0x3a8efc and
        // operator=(Clip&&) @0x372ff4, which moves member by member.
        template <typename Content>
        struct Clip {
            realtime::SharedPtr<Content const> pContent;           // 0x00
            realtime::SharedPtr<ClipAutomation const> pAutomation; // 0x10
            transport::ClipRegion region;                          // 0x20..0x48
            realtime::SharedPtr<time::Groove const> pGroove;       // 0x48
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
            std::uint64_t unknown18{};                  // 0x18: always copied along; see the notes
        };

        namespace detail {
            // 0x78 bytes: the clip slots published by the model, plus a temporary clip that
            // stands in for one slot while it records.
            template <typename Content>
            struct ClipSlotsHolder {
                // 0x60 bytes. ~TemporaryClip @0x3a8e4c is the compiler's.
                struct TemporaryClip {
                    std::size_t slotIndex{};   // 0x00 (relative to the holder: 0x10)
                    Clip<Content> clip;        // 0x08 (0x18)
                };

                // Defined below; LauncherNode.hpp line 259 asserts on its argument.
                void ensureFilledSlot(unsigned long slotIndex, Beats newClipDuration);  // @0x3a6c60

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

            // @0x3a6c60 -> decomp/launcher_detail_ClipSlotsHolder_midi_player_Content_ensureFilledSlot__00000000003a6c60.c
            // Recording into an empty slot needs a clip to play while it records. If the slot has no
            // clip (and the temporary clip isn't already standing in for it), a temporary clip takes
            // its place: newClipDuration long, looping over its whole length, with no content yet.
            // A zero duration does nothing. Any temporary clip for another slot is replaced.
            template <typename Content>
            void ClipSlotsHolder<Content>::ensureFilledSlot(unsigned long slotIndex, Beats newClipDuration) {
                assert(newClipDuration >= 0.0);
                if (newClipDuration == 0.0 || clipAt(slotIndex)) {
                    return;
                }
                Clip<Content> clip;
                clip.region = transport::ClipRegion{0.0, transport::Loop{0.0, newClipDuration}, newClipDuration};
                moTemporaryClip = TemporaryClip{slotIndex, std::move(clip)};
            }

        }  // namespace detail

        // 0x48 bytes: what the launcher reports after every state change (output event). The member
        // names are ours.
        struct State {
            // 0x18 bytes: the queued trigger, as far as the outside needs to know.
            struct PendingLaunch {
                std::optional<std::size_t> oClipSlot;   // 0x00 index, 0x08 engaged; empty means stop
                bool willRecord{};                      // 0x10: recording, or a record trigger
            };

            std::optional<PendingLaunch> oPendingLaunch;   // 0x00, engaged at 0x18
            PlayingState playingState;                     // 0x20
            bool isRecording{};                            // 0x40
            bool isPlayingTemporaryClip{};                 // 0x41
            ChangeOrigin origin{};                         // 0x44
        };

        // 0x30 bytes: launch a slot (or stop, without a slot). Field names are ours.
        struct Trigger {
            Beats quantum{};                       // 0x00: launch grid; 0 launches at once
            std::optional<std::size_t> oClipSlot;  // 0x08, engaged at 0x10
            Beats newClipDuration{};               // 0x18: length of a clip recorded into an empty slot
            std::optional<Beats> oLaunchTime;      // 0x20, engaged at 0x28
        };

        // Empty commands, passed by value.
        struct RestartPlayingClip {};
        struct StartRecording {};
        struct StopRecording {};

        // 0x18 bytes: record into a slot. Field names are ours.
        struct TriggerRecording {
            Beats quantum{};               // 0x00
            std::size_t clipSlot{};        // 0x08
            Beats newClipDuration{};       // 0x10
        };

    }  // namespace launcher

    template <typename Content>
    struct LauncherNode {
        // Built by operator() for each block: the current transport segment, then the node's
        // outputs. Member names are ours. More members may follow.
        struct AudioContext {
            transport::Segment segment;                                                            // 0x00
            CvBuffer<std::optional<ClipTimeNode::PlayingClipFrame>, Frames>& playingClipFrameOut;  // 0x60
            CvBuffer<realtime::SharedPtr<time::Groove const>, Frames>& grooveOut;                  // 0x68
            CvBuffer<realtime::SharedPtr<Content const>, Frames>& contentOut;                      // 0x70
            CvBuffer<realtime::SharedPtr<launcher::ClipAutomation const>, Frames>& automationOut;  // 0x78
            EventBuffer<launcher::State, Frames>& stateOut;                                        // 0x80
        };

        // 0x38 bytes. The member holding it is called moPendingTrigger (assertion string).
        struct PendingTrigger {
            // Names are ours. StopRecording turns a kRecord trigger back into kLaunch.
            enum class Kind : std::int32_t { kLaunch = 0, kRecord = 1 };

            Beats quantum{};                     // 0x00: launch grid in beats; 0 launches at once
            std::optional<std::size_t> oClipSlot;  // 0x08 index, 0x10 engaged; empty means stop
            Kind kind{};                         // 0x18
            Beats newClipDuration{};             // 0x20: passed on to ensureFilledSlot
            std::optional<Beats> oLaunchTime;    // 0x28, engaged at 0x30: fixed launch time (name is ours)
        };

        // How play() changes recording: kStart turns it on, kNone leaves it. Names are ours; play
        // only tests for 1. Stopping goes through setState directly.
        enum class RecordingChange : int { kNone = 0, kStart = 1 };

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

        // Data members. Offsets from the functions above.
        launcher::detail::ClipSlotsHolder<Content> mClipSlots;   // 0x00 (name is ours)
        std::optional<PendingTrigger> moPendingTrigger;          // 0x78, engaged at 0xb0
        launcher::PlayingState mPlayingState;                     // 0xb8 (name is ours)
        bool mIsRecording{};                                      // 0xd8 (name is ours)
    };

}  // namespace ableton::engine
