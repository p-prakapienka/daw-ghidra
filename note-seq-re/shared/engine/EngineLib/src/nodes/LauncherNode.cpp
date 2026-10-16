// Reconstructed from ABLSwiftBridge (Ableton Note). Original: shared/engine/EngineLib/src/nodes/LauncherNode.cpp
// Notes: LauncherNode.md next to this file.
#include "nodes/LauncherNode.hpp"

#include "ableton/math/MathUtils.hpp"

#include <cassert>
#include <cmath>

// Assertions seen in the binary for this file (line: condition):
//   line 39: segment.isPlaying
//   line 374: moPendingTrigger.has_value()

namespace ableton::engine::launcher {

// @0x3a807c -> decomp/launcher_operator___00000000003a807c.c
// A tail call to ostream::operator<<(int).
std::ostream& operator<<(std::ostream& os, ChangeOrigin origin) {
    return os << static_cast<int>(origin);
}

}  // namespace ableton::engine::launcher

namespace ableton::engine {

// @0x3a7724 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a7724.c
// Stops recording. A queued record trigger stays queued but launches without recording.
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& context, launcher::StopRecording) {
    if (moPendingTrigger) {
        moPendingTrigger->kind = PendingTrigger::Kind::kLaunch;
    }
    setState(context, launcher::ChangeOrigin::kCommand, moPendingTrigger, mPlayingState, false);
}

// @0x3a75e4 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a75e4.c
// Starts recording into whatever plays now.
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& context, launcher::StartRecording) {
    play(context, launcher::ChangeOrigin::kCommand, mPlayingState, RecordingChange::kStart);
}

// @0x3a7598 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a7598.c
// Restarts the playing clip from the start of this segment. Does nothing while stopped.
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& context, launcher::RestartPlayingClip) {
    if (!mPlayingState.oPlayingClipSlot) {
        return;
    }
    launcher::PlayingState const restarted{
        context.segment.beatTimeStart, mPlayingState.oPlayingClipSlot, mPlayingState.unknown18};
    play(context, launcher::ChangeOrigin::kRestart, restarted, RecordingChange::kNone);
}

// @0x3a7584 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a7584.c
// Takes over a playing state from outside as it is; the pending trigger and recording stay.
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& context, launcher::PlayingState const& playingState) {
    play(context, launcher::ChangeOrigin::kCommand, playingState, RecordingChange::kNone);
}

// @0x3a75f8 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a75f8.c
// Records into a slot:
// - the slot is already playing: start recording into it;
// - zero quantum: fill the slot if it's empty and play it, recording, from the segment start;
// - otherwise: queue a record trigger, replacing any pending one.
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& context, launcher::TriggerRecording const& trigger) {
    if (mPlayingState.oPlayingClipSlot == trigger.clipSlot) {
        play(context, launcher::ChangeOrigin::kCommand, mPlayingState, RecordingChange::kStart);
        return;
    }
    if (trigger.quantum == 0.0) {
        moPendingTrigger.reset();
        mClipSlots.ensureFilledSlot(trigger.clipSlot, trigger.newClipDuration);
        launcher::PlayingState const recording{
            context.segment.beatTimeStart, trigger.clipSlot, mPlayingState.unknown18};
        play(context, launcher::ChangeOrigin::kCommand, recording, RecordingChange::kStart);
        return;
    }
    PendingTrigger const pending{
        trigger.quantum, trigger.clipSlot, PendingTrigger::Kind::kRecord, trigger.newClipDuration, std::nullopt};
    setState(context, launcher::ChangeOrigin::kCommand, pending, mPlayingState, mIsRecording);
}

// @0x3a6a30 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a6a30.c
// Launches a slot, or stops without one. With a quantum, the trigger is queued (replacing any
// pending one) while the transport plays or the slot holds a clip; launchPendingClip fires it
// later. Otherwise it takes effect now, the same way launchPendingClip does. Either way it keeps
// the kind of the trigger it replaces, so a queued record trigger stays a record trigger.
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& context, launcher::Trigger const& trigger) {
    auto const kind = moPendingTrigger ? moPendingTrigger->kind : PendingTrigger::Kind::kLaunch;
    launcher::Clip<Content> const* pClip = trigger.oClipSlot ? mClipSlots.clipAt(*trigger.oClipSlot) : nullptr;
    if (trigger.quantum != 0.0 && (context.segment.isPlaying || pClip)) {
        PendingTrigger const pending{
            trigger.quantum, trigger.oClipSlot, kind, trigger.newClipDuration, trigger.oLaunchTime};
        setState(context, launcher::ChangeOrigin::kCommand, pending, mPlayingState, mIsRecording);
        return;
    }
    moPendingTrigger.reset();
    Beats const launchTime = trigger.oLaunchTime ? *trigger.oLaunchTime : context.segment.beatTimeStart;
    if (trigger.oClipSlot && (mIsRecording || kind == PendingTrigger::Kind::kRecord)) {
        mClipSlots.ensureFilledSlot(*trigger.oClipSlot, trigger.newClipDuration);
    }
    launcher::PlayingState const launched{launchTime, trigger.oClipSlot, mPlayingState.unknown18};
    play(context, launcher::ChangeOrigin::kCommand, launched,
         kind == PendingTrigger::Kind::kRecord ? RecordingChange::kStart : RecordingChange::kNone);
}

// @0x3a66d4 -> decomp/LauncherNode_midi_player_Content_processClipSlots__00000000003a66d4.c
template <typename Content>
void LauncherNode<Content>::processClipSlots(AudioContext const& p0, ableton::realtime::SharedPtr<std::vector<launcher::ClipSlot<Content>> const> const& p1) {
    // TODO
}

// @0x3a7d54 -> decomp/LauncherNode_midi_player_Content_finishPlayingClip__00000000003a7d54.c
// The playing clip ran out: stop at the start of this segment.
template <typename Content>
void LauncherNode<Content>::finishPlayingClip(AudioContext const& context) {
    launcher::PlayingState const stopped{context.segment.beatTimeStart, std::nullopt, mPlayingState.unknown18};
    play(context, launcher::ChangeOrigin::kPlayback, stopped, RecordingChange::kNone);
}

// @0x3a7c44 -> decomp/LauncherNode_midi_player_Content_launchPendingClip__00000000003a7c44.c
// The pending trigger came due: launch its slot (or stop, without one) at its fixed launch time,
// else at the start of this segment. A record trigger, or launching while recording, first
// fills an empty slot with a temporary clip; a record trigger also starts recording.
template <typename Content>
void LauncherNode<Content>::launchPendingClip(AudioContext const& context) {
    assert(moPendingTrigger.has_value());
    PendingTrigger const trigger = *moPendingTrigger;
    moPendingTrigger.reset();
    Beats const launchTime = trigger.oLaunchTime ? *trigger.oLaunchTime : context.segment.beatTimeStart;
    bool const isRecordTrigger = trigger.kind == PendingTrigger::Kind::kRecord;
    if (trigger.oClipSlot && (mIsRecording || isRecordTrigger)) {
        mClipSlots.ensureFilledSlot(*trigger.oClipSlot, trigger.newClipDuration);
    }
    launcher::PlayingState const launched{launchTime, trigger.oClipSlot, mPlayingState.unknown18};
    play(context, launcher::ChangeOrigin::kLaunch, launched,
         isRecordTrigger ? RecordingChange::kStart : RecordingChange::kNone);
}

// @0x3a7d98 -> decomp/LauncherNode_midi_player_Content_adjustPlayingClipStartIfNeeded__00000000003a7d98.c
// When the song time jumps back to before the playing clip's start, a looping clip keeps
// playing: its start moves back by whole loop lengths to at or before the segment start, so the
// clip stays at the same position in its loop. One-shot clips are left to clipStopTimeIfDue.
template <typename Content>
void LauncherNode<Content>::adjustPlayingClipStartIfNeeded(AudioContext const& context) {
    if (!mPlayingState.oPlayingClipSlot) {
        return;
    }
    launcher::Clip<Content> const* pClip = mClipSlots.clipAt(*mPlayingState.oPlayingClipSlot);
    if (!pClip || !pClip->region.oLoop) {
        return;
    }
    Beats clipStart = mPlayingState.sessionClipStartTime;
    Beats const segmentStart = context.segment.beatTimeStart;
    if (!(segmentStart < clipStart)) {
        return;
    }
    Beats const loopLength = pClip->region.oLoop->end - pClip->region.oLoop->start;
    Beats const shift = loopLength * std::ceil((clipStart - segmentStart) / loopLength);
    clipStart -= shift;
    launcher::PlayingState const adjusted{clipStart, mPlayingState.oPlayingClipSlot, mPlayingState.unknown18};
    play(context, launcher::ChangeOrigin::kPlayback, adjusted, RecordingChange::kNone);
}

// @0x3a7744 -> decomp/LauncherNode_midi_player_Content_processPendingPlaybackOperations__00000000003a7744.c
template <typename Content>
void LauncherNode<Content>::processPendingPlaybackOperations(AudioContext p0) {
    // TODO
}

// @0x3a69fc -> decomp/LauncherNode_midi_player_Content_play__00000000003a69fc.c
// setState with the pending trigger unchanged. kStart turns recording on; otherwise it stays.
template <typename Content>
void LauncherNode<Content>::play(AudioContext const& context, launcher::ChangeOrigin origin,
                                 launcher::PlayingState const& playingState, RecordingChange recordingChange) {
    setState(context, origin, moPendingTrigger, playingState,
             recordingChange == RecordingChange::kStart || mIsRecording);
}

// @0x3a6ec8 -> decomp/LauncherNode_midi_player_Content_setState__00000000003a6ec8.c
// The one place the launcher's state changes. It stores the new pending trigger, playing state
// and recording flag, then sends, all at the segment's first frame:
//  1. the playing clip's frame (start time and region), or nothing while stopped;
//  2. its groove, content and automation (null while stopped);
//  3. a launcher::State event describing the new state and why it changed.
// A playing slot without a clip counts as stopped. Both arguments may refer to the node's own
// members (play passes moPendingTrigger, StartRecording passes mPlayingState).
template <typename Content>
void LauncherNode<Content>::setState(AudioContext const& context, launcher::ChangeOrigin origin,
                                     std::optional<PendingTrigger> const& oPendingTrigger,
                                     launcher::PlayingState const& playingState, bool isRecording) {
    moPendingTrigger = oPendingTrigger;
    mPlayingState = playingState;
    mIsRecording = isRecording;

    launcher::Clip<Content> const* pClip = nullptr;
    if (mPlayingState.oPlayingClipSlot) {
        pClip = mClipSlots.clipAt(*mPlayingState.oPlayingClipSlot);
        if (!pClip) {
            mPlayingState.oPlayingClipSlot.reset();
        }
    }

    Frames const time = context.segment.frameOffsetStart;
    std::optional<ClipTimeNode::PlayingClipFrame> oPlayingClipFrame;
    if (pClip) {
        oPlayingClipFrame = ClipTimeNode::PlayingClipFrame{mPlayingState.sessionClipStartTime, pClip->region};
    }
    context.playingClipFrameOut.emplace_back(time, oPlayingClipFrame);
    context.grooveOut.emplace_back(time, pClip ? pClip->pGroove : nullptr);
    context.contentOut.emplace_back(time, pClip ? pClip->pContent : nullptr);
    context.automationOut.emplace_back(time, pClip ? pClip->pAutomation : nullptr);

    launcher::State state;
    if (moPendingTrigger) {
        state.oPendingLaunch = launcher::State::PendingLaunch{
            moPendingTrigger->oClipSlot, mIsRecording || moPendingTrigger->kind == PendingTrigger::Kind::kRecord};
    }
    state.playingState = mPlayingState;
    state.isRecording = mIsRecording;
    state.isPlayingTemporaryClip = mPlayingState.oPlayingClipSlot && mClipSlots.moTemporaryClip &&
                                   mClipSlots.moTemporaryClip->slotIndex == *mPlayingState.oPlayingClipSlot;
    state.origin = origin;
    context.stateOut.emplace_back(time, std::move(state));
}

// @0x3a7b9c -> decomp/LauncherNode_midi_player_Content_launchTimeIfDue__00000000003a7b9c.c
// When the pending trigger fires inside this segment: the first multiple of its quantum at or
// after the segment start, if that is still before the segment end. A zero quantum fires at once.
template <typename Content>
std::optional<Beats> LauncherNode<Content>::launchTimeIfDue(transport::Segment const& segment) const {
    if (!moPendingTrigger) {
        return std::nullopt;
    }
    Beats const launchTime = math::roundUpToMultiple(segment.beatTimeStart, moPendingTrigger->quantum);
    if (launchTime < segment.beatTimeEnd) {
        return launchTime;
    }
    return std::nullopt;
}

// @0x3a7ae4 -> decomp/LauncherNode_midi_player_Content_clipStopTimeIfDue__00000000003a7ae4.c
// When a one-shot (non-looping) clip runs out inside this segment. Looping clips never stop on
// their own. A clip whose start lies after the segment start (the song time jumped back) stops
// at once.
template <typename Content>
std::optional<Beats> LauncherNode<Content>::clipStopTimeIfDue(transport::Segment const& segment) const {
    if (!mPlayingState.oPlayingClipSlot) {
        return std::nullopt;
    }
    launcher::Clip<Content> const* clip = mClipSlots.clipAt(*mPlayingState.oPlayingClipSlot);
    if (!clip || clip->region.oLoop) {
        return std::nullopt;
    }
    Beats const segmentStart = segment.beatTimeStart;
    Beats const clipStart = mPlayingState.sessionClipStartTime;
    if (clipStart > segmentStart) {
        return segmentStart;
    }
    Beats const stopTime = clipStart + clip->region.end - clip->region.start;
    if (stopTime >= segment.beatTimeEnd) {
        return std::nullopt;
    }
    return stopTime < segmentStart ? segmentStart : stopTime;
}

}  // namespace ableton::engine

// Explicit instantiations (the binary only contains these):
#include "ableton/engine/nodes/MidiPlayerContent.hpp"
template struct ableton::engine::launcher::Clip<ableton::engine::midi_player::Content>;
template struct ableton::engine::launcher::detail::ClipSlotsHolder<ableton::engine::midi_player::Content>;
template struct ableton::engine::LauncherNode<ableton::engine::midi_player::Content>;

// Layout checks against the original (arm64, 64-bit size_t).
namespace {
using Content = ableton::engine::midi_player::Content;
using Node = ableton::engine::LauncherNode<Content>;
using Holder = ableton::engine::launcher::detail::ClipSlotsHolder<Content>;
static_assert(sizeof(ableton::engine::launcher::Clip<Content>) == 0x58);
static_assert(offsetof(ableton::engine::launcher::Clip<Content>, region) == 0x20);
static_assert(sizeof(ableton::engine::launcher::ClipSlot<Content>) == 0x60);
static_assert(sizeof(ableton::engine::launcher::PlayingState) == 0x20);
static_assert(sizeof(Holder) == 0x78);
static_assert(offsetof(Holder, moTemporaryClip) == 0x10);
static_assert(sizeof(Node::PendingTrigger) == 0x38);
static_assert(offsetof(Node::PendingTrigger, kind) == 0x18);
static_assert(offsetof(Node::PendingTrigger, newClipDuration) == 0x20);
static_assert(offsetof(Node::PendingTrigger, oLaunchTime) == 0x28);
static_assert(sizeof(Node::AudioContext) == 0x60 + 5 * sizeof(void*));   // segment, then the outputs from 0x60
static_assert(sizeof(ableton::engine::ClipTimeNode::PlayingClipFrame) == 0x30);
static_assert(sizeof(ableton::engine::launcher::State) == 0x48);
static_assert(offsetof(ableton::engine::launcher::State, playingState) == 0x20);
static_assert(offsetof(ableton::engine::launcher::State, isRecording) == 0x40);
static_assert(offsetof(ableton::engine::launcher::State, isPlayingTemporaryClip) == 0x41);
static_assert(offsetof(ableton::engine::launcher::State, origin) == 0x44);
static_assert(sizeof(ableton::engine::launcher::Trigger) == 0x30);
static_assert(offsetof(ableton::engine::launcher::Trigger, newClipDuration) == 0x18);
static_assert(offsetof(ableton::engine::launcher::Trigger, oLaunchTime) == 0x20);
static_assert(sizeof(ableton::engine::launcher::TriggerRecording) == 0x18);
static_assert(offsetof(Node, moPendingTrigger) == 0x78);
static_assert(offsetof(Node, mPlayingState) == 0xb8);
static_assert(offsetof(Node, mIsRecording) == 0xd8);
}  // namespace
