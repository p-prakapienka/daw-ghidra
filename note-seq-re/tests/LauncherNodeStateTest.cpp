#include "nodes/LauncherNode.hpp"

#include "ableton/engine/nodes/MidiPlayerContent.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

using namespace ableton::engine;
using ableton::Beats;
using ableton::Frames;

namespace {

using Content = midi_player::Content;
using Node = LauncherNode<Content>;
using Kind = Node::PendingTrigger::Kind;
using launcher::ChangeOrigin;

// The node's outputs plus a context for one segment.
struct Outputs {
    CvBuffer<std::optional<ClipTimeNode::PlayingClipFrame>, Frames> clipFrames;
    CvBuffer<ableton::realtime::SharedPtr<ableton::time::Groove const>, Frames> grooves;
    CvBuffer<ableton::realtime::SharedPtr<Content const>, Frames> contents;
    CvBuffer<ableton::realtime::SharedPtr<launcher::ClipAutomation const>, Frames> automations;
    EventBuffer<launcher::State, Frames> states;

    Node::AudioContext context(Beats beatStart, bool isPlaying = true, Frames frameStart = 64.0) {
        transport::Segment s;
        s.frameOffsetStart = frameStart;
        s.frameOffsetEnd = frameStart + 128.0;
        s.beatTimeStart = beatStart;
        s.beatTimeEnd = isPlaying ? beatStart + 0.05 : beatStart;
        s.isPlaying = isPlaying;
        return {s, clipFrames, grooves, contents, automations, states};
    }

    launcher::State const& lastState() const { return states.events.back().value; }
};

transport::ClipRegion looping(Beats length) { return {0.0, transport::Loop{0.0, length}, length}; }
transport::ClipRegion oneShot(Beats length) { return {0.0, std::nullopt, length}; }

// A distinct pointer that the launcher only passes on, never dereferences. (The real content
// and automation types can't be built yet.)
template <class T>
ableton::realtime::SharedPtr<T const> token() {
    auto owner = std::make_shared<int>(0);
    return {owner, reinterpret_cast<T const*>(owner.get())};
}

// A clip whose shared pointers are distinct and recognisable.
launcher::Clip<Content> makeClip(transport::ClipRegion region) {
    launcher::Clip<Content> clip;
    clip.pContent = token<Content>();
    clip.pAutomation = token<launcher::ClipAutomation>();
    clip.pGroove = std::make_shared<ableton::time::Groove const>();
    clip.region = region;
    return clip;
}

// Slots 0..n-1; the regions given fill the first slots, the rest stay empty.
void setSlots(Node& node, std::vector<std::optional<transport::ClipRegion>> regions) {
    auto slots = std::make_shared<std::vector<launcher::ClipSlot<Content>>>(regions.size());
    for (std::size_t i = 0; i < regions.size(); ++i) {
        if (regions[i]) {
            (*slots)[i].oClip = makeClip(*regions[i]);
        }
    }
    node.mClipSlots.mpClipSlots = slots;
}

launcher::Clip<Content> const& slotClip(Node const& node, std::size_t index) {
    return *(*node.mClipSlots.mpClipSlots)[index].oClip;
}

launcher::Trigger trigger(Beats quantum, std::optional<std::size_t> slot) {
    launcher::Trigger t;
    t.quantum = quantum;
    t.oClipSlot = slot;
    t.newClipDuration = 8.0;
    return t;
}

}  // namespace

// setState and play

TEST(LauncherNodeSetState, SendsTheClipItStartsPlayingAtTheFirstFrame) {
    Node node;
    setSlots(node, {looping(4.0)});
    Outputs out;
    node.play(out.context(2.0, true, 100.0), ChangeOrigin::kCommand, {2.0, 0, 0}, Node::RecordingChange::kNone);

    auto const& clip = slotClip(node, 0);
    ASSERT_EQ(out.clipFrames.events.size(), 1u);
    EXPECT_EQ(out.clipFrames.events[0].time, 100.0);
    ASSERT_TRUE(out.clipFrames.events[0].value);
    EXPECT_EQ(out.clipFrames.events[0].value->sessionClipStartTime, 2.0);
    EXPECT_EQ(out.clipFrames.events[0].value->region.end, 4.0);
    EXPECT_EQ(out.grooves.events.at(0).value, clip.pGroove);
    EXPECT_EQ(out.contents.events.at(0).value, clip.pContent);
    EXPECT_EQ(out.automations.events.at(0).value, clip.pAutomation);
    ASSERT_EQ(out.states.events.size(), 1u);
    EXPECT_EQ(out.states.events[0].time, 100.0);
    EXPECT_EQ(out.lastState().playingState.oPlayingClipSlot, 0u);
    EXPECT_EQ(out.lastState().origin, ChangeOrigin::kCommand);
}

TEST(LauncherNodeSetState, AnEmptySlotPlaysAsStopped) {
    Node node;
    setSlots(node, {std::nullopt});
    Outputs out;
    node.play(out.context(0.0), ChangeOrigin::kCommand, {0.0, 0, 0}, Node::RecordingChange::kNone);

    EXPECT_FALSE(node.mPlayingState.oPlayingClipSlot);
    EXPECT_FALSE(out.clipFrames.events.at(0).value);
    EXPECT_EQ(out.grooves.events.at(0).value, nullptr);
    EXPECT_EQ(out.contents.events.at(0).value, nullptr);
    EXPECT_EQ(out.automations.events.at(0).value, nullptr);
    EXPECT_FALSE(out.lastState().playingState.oPlayingClipSlot);
}

TEST(LauncherNodeSetState, ReportsThePendingTriggerAndWhetherItWillRecord) {
    Node node;
    Outputs out;
    Node::PendingTrigger pending;
    pending.quantum = 4.0;
    pending.oClipSlot = 2;
    node.setState(out.context(0.0), ChangeOrigin::kCommand, pending, {}, false);
    ASSERT_TRUE(out.lastState().oPendingLaunch);
    EXPECT_EQ(out.lastState().oPendingLaunch->oClipSlot, 2u);
    EXPECT_FALSE(out.lastState().oPendingLaunch->willRecord);

    pending.kind = Kind::kRecord;
    node.setState(out.context(0.0), ChangeOrigin::kCommand, pending, {}, false);
    EXPECT_TRUE(out.lastState().oPendingLaunch->willRecord);

    pending.kind = Kind::kLaunch;
    node.setState(out.context(0.0), ChangeOrigin::kCommand, pending, {}, true);
    EXPECT_TRUE(out.lastState().oPendingLaunch->willRecord);

    node.setState(out.context(0.0), ChangeOrigin::kCommand, std::nullopt, {}, false);
    EXPECT_FALSE(out.lastState().oPendingLaunch);
    EXPECT_FALSE(node.moPendingTrigger);
}

TEST(LauncherNodeSetState, FlagsTheTemporaryClipWhileItPlays) {
    Node node;
    setSlots(node, {std::nullopt, std::nullopt});
    node.mClipSlots.ensureFilledSlot(1, 4.0);
    Outputs out;
    node.play(out.context(0.0), ChangeOrigin::kCommand, {0.0, 1, 0}, Node::RecordingChange::kNone);
    EXPECT_TRUE(out.lastState().isPlayingTemporaryClip);
    node.play(out.context(0.0), ChangeOrigin::kCommand, {0.0, 0, 0}, Node::RecordingChange::kNone);
    EXPECT_FALSE(out.lastState().isPlayingTemporaryClip);
}

TEST(LauncherNodeSetState, PlayStartsRecordingButNeverStopsIt) {
    Node node;
    Outputs out;
    node.play(out.context(0.0), ChangeOrigin::kCommand, {}, Node::RecordingChange::kStart);
    EXPECT_TRUE(node.mIsRecording);
    node.play(out.context(0.0), ChangeOrigin::kCommand, {}, Node::RecordingChange::kNone);
    EXPECT_TRUE(node.mIsRecording);
    EXPECT_TRUE(out.lastState().isRecording);
}

TEST(LauncherNodeSetState, KeepsTheModeFieldOfThePlayingState) {
    Node node;
    setSlots(node, {looping(4.0)});
    Outputs out;
    node.processCommand(out.context(0.0), launcher::PlayingState{1.0, 0, 7});
    EXPECT_EQ(node.mPlayingState.unknown18, 7u);
    node.finishPlayingClip(out.context(3.0));
    EXPECT_EQ(node.mPlayingState.unknown18, 7u);
    EXPECT_EQ(out.lastState().playingState.unknown18, 7u);
}

// Trigger

TEST(LauncherNodeTrigger, IsQueuedWhileThePlayerRuns) {
    Node node;
    setSlots(node, {looping(4.0)});
    Outputs out;
    auto t = trigger(4.0, 0);
    t.oLaunchTime = 8.0;
    node.processCommand(out.context(1.0), t);

    ASSERT_TRUE(node.moPendingTrigger);
    EXPECT_EQ(node.moPendingTrigger->quantum, 4.0);
    EXPECT_EQ(node.moPendingTrigger->oClipSlot, 0u);
    EXPECT_EQ(node.moPendingTrigger->newClipDuration, 8.0);
    EXPECT_EQ(node.moPendingTrigger->oLaunchTime, 8.0);
    EXPECT_FALSE(node.mPlayingState.oPlayingClipSlot);
    ASSERT_TRUE(out.lastState().oPendingLaunch);
}

TEST(LauncherNodeTrigger, AZeroQuantumLaunchesAtTheSegmentStart) {
    Node node;
    setSlots(node, {looping(4.0)});
    Outputs out;
    node.processCommand(out.context(1.5), trigger(0.0, 0));
    EXPECT_FALSE(node.moPendingTrigger);
    EXPECT_EQ(node.mPlayingState.oPlayingClipSlot, 0u);
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 1.5);
}

TEST(LauncherNodeTrigger, WhileStoppedAClipIsQueuedButAnEmptySlotTakesEffectAtOnce) {
    Node node;
    setSlots(node, {looping(4.0), std::nullopt});
    Outputs out;
    node.processCommand(out.context(0.0, false), trigger(4.0, 0));
    EXPECT_TRUE(node.moPendingTrigger);

    node.processCommand(out.context(0.0, false), trigger(4.0, 1));
    EXPECT_FALSE(node.moPendingTrigger);
    EXPECT_FALSE(node.mPlayingState.oPlayingClipSlot);
}

TEST(LauncherNodeTrigger, AnImmediateLaunchUsesTheFixedLaunchTime) {
    Node node;
    setSlots(node, {looping(4.0)});
    Outputs out;
    auto t = trigger(0.0, 0);
    t.oLaunchTime = 0.75;
    node.processCommand(out.context(1.0), t);
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 0.75);
}

TEST(LauncherNodeTrigger, ReplacingARecordTriggerKeepsItsKind) {
    Node node;
    setSlots(node, {std::nullopt, looping(4.0)});
    Outputs out;
    node.processCommand(out.context(0.0), launcher::TriggerRecording{4.0, 0, 8.0});
    node.processCommand(out.context(0.5), trigger(4.0, 1));
    ASSERT_TRUE(node.moPendingTrigger);
    EXPECT_EQ(node.moPendingTrigger->kind, Kind::kRecord);
    EXPECT_EQ(node.moPendingTrigger->oClipSlot, 1u);
}

TEST(LauncherNodeTrigger, LaunchingWhileRecordingFillsAnEmptySlot) {
    Node node;
    setSlots(node, {std::nullopt});
    node.mIsRecording = true;
    Outputs out;
    node.processCommand(out.context(2.0), trigger(0.0, 0));
    ASSERT_TRUE(node.mClipSlots.moTemporaryClip);
    EXPECT_EQ(node.mClipSlots.moTemporaryClip->slotIndex, 0u);
    EXPECT_EQ(node.mPlayingState.oPlayingClipSlot, 0u);
    EXPECT_TRUE(out.lastState().isPlayingTemporaryClip);
}

TEST(LauncherNodeTrigger, WithoutASlotItStops) {
    Node node;
    setSlots(node, {looping(4.0)});
    node.mPlayingState = {0.0, 0, 0};
    Outputs out;
    node.processCommand(out.context(3.0), trigger(0.0, std::nullopt));
    EXPECT_FALSE(node.mPlayingState.oPlayingClipSlot);
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 3.0);
}

// Recording commands

TEST(LauncherNodeRecording, TriggerRecordingOnThePlayingSlotStartsRecordingAtOnce) {
    Node node;
    setSlots(node, {looping(4.0)});
    node.mPlayingState = {0.0, 0, 0};
    Outputs out;
    node.processCommand(out.context(1.0), launcher::TriggerRecording{4.0, 0, 8.0});
    EXPECT_TRUE(node.mIsRecording);
    EXPECT_FALSE(node.moPendingTrigger);
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 0.0);
}

TEST(LauncherNodeRecording, TriggerRecordingWithoutQuantumFillsAndPlaysTheSlot) {
    Node node;
    setSlots(node, {std::nullopt, std::nullopt});
    Outputs out;
    node.processCommand(out.context(1.0), launcher::TriggerRecording{0.0, 1, 8.0});
    EXPECT_TRUE(node.mIsRecording);
    EXPECT_EQ(node.mPlayingState.oPlayingClipSlot, 1u);
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 1.0);
    ASSERT_TRUE(node.mClipSlots.moTemporaryClip);
    EXPECT_EQ(node.mClipSlots.moTemporaryClip->clip.region.end, 8.0);
}

TEST(LauncherNodeRecording, TriggerRecordingWithQuantumQueuesARecordTrigger) {
    Node node;
    Outputs out;
    node.processCommand(out.context(1.0), launcher::TriggerRecording{4.0, 3, 16.0});
    ASSERT_TRUE(node.moPendingTrigger);
    EXPECT_EQ(node.moPendingTrigger->kind, Kind::kRecord);
    EXPECT_EQ(node.moPendingTrigger->oClipSlot, 3u);
    EXPECT_EQ(node.moPendingTrigger->newClipDuration, 16.0);
    EXPECT_FALSE(node.moPendingTrigger->oLaunchTime);
    EXPECT_FALSE(node.mIsRecording);
    EXPECT_TRUE(out.lastState().oPendingLaunch->willRecord);
}

TEST(LauncherNodeRecording, StopRecordingKeepsTheQueuedTriggerAsAPlainLaunch) {
    Node node;
    node.mIsRecording = true;
    Outputs out;
    node.processCommand(out.context(1.0), launcher::TriggerRecording{4.0, 3, 16.0});
    node.processCommand(out.context(1.0), launcher::StopRecording{});
    EXPECT_FALSE(node.mIsRecording);
    ASSERT_TRUE(node.moPendingTrigger);
    EXPECT_EQ(node.moPendingTrigger->kind, Kind::kLaunch);
    EXPECT_FALSE(out.lastState().oPendingLaunch->willRecord);
}

TEST(LauncherNodeRecording, StartRecordingKeepsWhatPlays) {
    Node node;
    setSlots(node, {looping(4.0)});
    node.mPlayingState = {2.0, 0, 0};
    Outputs out;
    node.processCommand(out.context(3.0), launcher::StartRecording{});
    EXPECT_TRUE(node.mIsRecording);
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 2.0);
    EXPECT_EQ(node.mPlayingState.oPlayingClipSlot, 0u);
}

// Restart, launch, finish, adjust

TEST(LauncherNodeRestart, RestartsThePlayingClipAtTheSegmentStart) {
    Node node;
    setSlots(node, {looping(4.0)});
    node.mPlayingState = {0.0, 0, 0};
    Outputs out;
    node.processCommand(out.context(5.0), launcher::RestartPlayingClip{});
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 5.0);
    EXPECT_EQ(out.lastState().origin, ChangeOrigin::kRestart);
}

TEST(LauncherNodeRestart, DoesNothingWhileStopped) {
    Node node;
    Outputs out;
    node.processCommand(out.context(5.0), launcher::RestartPlayingClip{});
    EXPECT_TRUE(out.states.events.empty());
}

TEST(LauncherNodeLaunchPending, LaunchesAtTheFixedTimeOrTheSegmentStart) {
    Node node;
    setSlots(node, {looping(4.0)});
    Outputs out;
    node.processCommand(out.context(1.0), trigger(4.0, 0));
    node.launchPendingClip(out.context(4.0));
    EXPECT_FALSE(node.moPendingTrigger);
    EXPECT_EQ(node.mPlayingState.oPlayingClipSlot, 0u);
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 4.0);
    EXPECT_EQ(out.lastState().origin, ChangeOrigin::kLaunch);

    auto t = trigger(4.0, 0);
    t.oLaunchTime = 7.5;
    node.processCommand(out.context(5.0), t);
    node.launchPendingClip(out.context(8.0));
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 7.5);
}

TEST(LauncherNodeLaunchPending, ARecordTriggerFillsTheSlotAndStartsRecording) {
    Node node;
    setSlots(node, {std::nullopt});
    Outputs out;
    node.processCommand(out.context(1.0), launcher::TriggerRecording{4.0, 0, 8.0});
    node.launchPendingClip(out.context(4.0));
    EXPECT_TRUE(node.mIsRecording);
    ASSERT_TRUE(node.mClipSlots.moTemporaryClip);
    EXPECT_EQ(node.mPlayingState.oPlayingClipSlot, 0u);
    EXPECT_TRUE(out.lastState().isPlayingTemporaryClip);
}

TEST(LauncherNodeFinish, StopsAtTheSegmentStart) {
    Node node;
    setSlots(node, {oneShot(4.0)});
    node.mPlayingState = {0.0, 0, 0};
    Outputs out;
    node.finishPlayingClip(out.context(4.0));
    EXPECT_FALSE(node.mPlayingState.oPlayingClipSlot);
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 4.0);
    EXPECT_EQ(out.lastState().origin, ChangeOrigin::kPlayback);
    EXPECT_FALSE(out.clipFrames.events.at(0).value);
}

TEST(LauncherNodeAdjust, MovesALoopingClipBackByWholeLoops) {
    Node node;
    setSlots(node, {looping(4.0)});
    node.mPlayingState = {16.0, 0, 0};
    Outputs out;
    node.adjustPlayingClipStartIfNeeded(out.context(5.0));
    // 16 - 4 * ceil((16 - 5) / 4) = 4: at or before the segment start, same phase in the loop.
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 4.0);
    EXPECT_EQ(out.lastState().origin, ChangeOrigin::kPlayback);

    node.mPlayingState = {16.0, 0, 0};
    node.adjustPlayingClipStartIfNeeded(out.context(12.0));
    EXPECT_EQ(node.mPlayingState.sessionClipStartTime, 12.0);
}

TEST(LauncherNodeAdjust, LeavesEverythingElseAlone) {
    Node node;
    setSlots(node, {oneShot(4.0), looping(4.0)});
    Outputs out;
    node.mPlayingState = {16.0, 0, 0};
    node.adjustPlayingClipStartIfNeeded(out.context(5.0));   // one-shot
    node.mPlayingState = {4.0, 1, 0};
    node.adjustPlayingClipStartIfNeeded(out.context(5.0));   // starts before the segment
    node.mPlayingState = {4.0, std::nullopt, 0};
    node.adjustPlayingClipStartIfNeeded(out.context(0.0));   // stopped
    EXPECT_TRUE(out.states.events.empty());
}

// ensureFilledSlot

TEST(LauncherNodeEnsureFilledSlot, CreatesALoopingTemporaryClipForAnEmptySlot) {
    Node node;
    setSlots(node, {std::nullopt});
    node.mClipSlots.ensureFilledSlot(0, 8.0);
    ASSERT_TRUE(node.mClipSlots.moTemporaryClip);
    auto const& temp = *node.mClipSlots.moTemporaryClip;
    EXPECT_EQ(temp.slotIndex, 0u);
    EXPECT_EQ(temp.clip.region.start, 0.0);
    ASSERT_TRUE(temp.clip.region.oLoop);
    EXPECT_EQ(temp.clip.region.oLoop->start, 0.0);
    EXPECT_EQ(temp.clip.region.oLoop->end, 8.0);
    EXPECT_EQ(temp.clip.region.end, 8.0);
    EXPECT_EQ(temp.clip.pContent, nullptr);
}

TEST(LauncherNodeEnsureFilledSlot, LeavesFilledSlotsAndZeroDurationsAlone) {
    Node node;
    setSlots(node, {looping(4.0), std::nullopt});
    node.mClipSlots.ensureFilledSlot(0, 8.0);
    node.mClipSlots.ensureFilledSlot(1, 0.0);
    EXPECT_FALSE(node.mClipSlots.moTemporaryClip);
}

TEST(LauncherNodeEnsureFilledSlot, ReplacesATemporaryClipForAnotherSlot) {
    Node node;
    node.mClipSlots.ensureFilledSlot(0, 8.0);
    node.mClipSlots.ensureFilledSlot(0, 4.0);   // already filled by the temporary clip
    EXPECT_EQ(node.mClipSlots.moTemporaryClip->clip.region.end, 8.0);
    node.mClipSlots.ensureFilledSlot(5, 4.0);   // no slots at all: still filled
    EXPECT_EQ(node.mClipSlots.moTemporaryClip->slotIndex, 5u);
    EXPECT_EQ(node.mClipSlots.moTemporaryClip->clip.region.end, 4.0);
}
