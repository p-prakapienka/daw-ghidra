#include "nodes/LauncherNode.hpp"

#include "ableton/engine/nodes/MidiPlayerContent.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

using namespace ableton::engine;
using ableton::Beats;

namespace {

using Content = midi_player::Content;
using Node = LauncherNode<Content>;

transport::Segment segment(Beats start, Beats end) {
    transport::Segment s;
    s.beatTimeStart = start;
    s.beatTimeEnd = end;
    s.isPlaying = true;
    return s;
}

Node::PendingTrigger trigger(Beats quantum) {
    Node::PendingTrigger t;
    t.quantum = quantum;
    t.oClipSlot = 0;
    return t;
}

// One slot holding a clip of the given region, playing since startTime.
void playClip(Node& node, transport::ClipRegion region, Beats startTime) {
    auto slots = std::make_shared<std::vector<launcher::ClipSlot<Content>>>(1);
    (*slots)[0].oClip.emplace();
    (*slots)[0].oClip->region = region;
    node.mClipSlots.mpClipSlots = slots;
    node.mPlayingState.sessionClipStartTime = startTime;
    node.mPlayingState.oPlayingClipSlot = 0;
}

transport::ClipRegion oneShot(Beats length) { return {0.0, std::nullopt, length}; }

} // namespace

TEST(LauncherNodeLaunchTime, NothingIsDueWithoutAPendingTrigger) {
    Node node;
    EXPECT_FALSE(node.launchTimeIfDue(segment(0.0, 1.0)));
}

TEST(LauncherNodeLaunchTime, ZeroQuantumLaunchesAtTheSegmentStart) {
    Node node;
    node.moPendingTrigger = trigger(0.0);
    EXPECT_EQ(node.launchTimeIfDue(segment(2.3, 2.4)), 2.3);
}

TEST(LauncherNodeLaunchTime, WaitsForTheNextMultipleOfTheQuantum) {
    Node node;
    node.moPendingTrigger = trigger(4.0);
    EXPECT_FALSE(node.launchTimeIfDue(segment(5.0, 7.9)));
    EXPECT_EQ(node.launchTimeIfDue(segment(7.9, 8.1)), 8.0);
    EXPECT_EQ(node.launchTimeIfDue(segment(8.0, 8.1)), 8.0);
}

TEST(LauncherNodeLaunchTime, TheEndOfTheSegmentIsExclusive) {
    Node node;
    node.moPendingTrigger = trigger(1.0);
    EXPECT_FALSE(node.launchTimeIfDue(segment(2.5, 3.0)));
}

TEST(LauncherNodeLaunchTime, IgnoresTheSignOfTheQuantum) {
    Node node;
    node.moPendingTrigger = trigger(-4.0);
    EXPECT_EQ(node.launchTimeIfDue(segment(7.9, 8.1)), 8.0);
}

TEST(LauncherNodeClipStop, NothingIsDueWhileStoppedOrForAnEmptySlot) {
    Node node;
    EXPECT_FALSE(node.clipStopTimeIfDue(segment(0.0, 100.0)));
    playClip(node, oneShot(4.0), 0.0);
    (*std::const_pointer_cast<std::vector<launcher::ClipSlot<Content>>>(node.mClipSlots.mpClipSlots))[0].oClip.reset();
    EXPECT_FALSE(node.clipStopTimeIfDue(segment(0.0, 100.0)));
}

TEST(LauncherNodeClipStop, AOneShotClipStopsAfterItsLength) {
    Node node;
    playClip(node, oneShot(4.0), 8.0);
    EXPECT_FALSE(node.clipStopTimeIfDue(segment(10.0, 11.0)));
    EXPECT_EQ(node.clipStopTimeIfDue(segment(11.9, 12.1)), 12.0);
}

TEST(LauncherNodeClipStop, ALoopingClipNeverStopsOnItsOwn) {
    Node node;
    playClip(node, {0.0, transport::Loop{0.0, 4.0}, 4.0}, 0.0);
    EXPECT_FALSE(node.clipStopTimeIfDue(segment(3.9, 100.0)));
}

TEST(LauncherNodeClipStop, AClipStartingAfterTheSegmentStartStopsAtOnce) {
    Node node;
    playClip(node, oneShot(4.0), 8.0);
    EXPECT_EQ(node.clipStopTimeIfDue(segment(2.0, 2.1)), 2.0);
}

TEST(LauncherNodeClipStop, AnOverdueStopHappensAtTheSegmentStart) {
    Node node;
    playClip(node, oneShot(4.0), 0.0);
    EXPECT_EQ(node.clipStopTimeIfDue(segment(6.0, 7.0)), 6.0);
}

TEST(LauncherNodeClipStop, TheTemporaryClipStandsInForItsSlot) {
    Node node;
    playClip(node, oneShot(4.0), 0.0);
    node.mClipSlots.moTemporaryClip.emplace();
    node.mClipSlots.moTemporaryClip->slotIndex = 0;
    node.mClipSlots.moTemporaryClip->clip.region = oneShot(2.0);
    EXPECT_EQ(node.clipStopTimeIfDue(segment(1.9, 2.1)), 2.0);
}

TEST(LauncherNodeClipStop, AnIndexPastTheSlotsIsIgnored) {
    Node node;
    playClip(node, oneShot(4.0), 0.0);
    node.mPlayingState.oPlayingClipSlot = 3;
    EXPECT_FALSE(node.clipStopTimeIfDue(segment(3.9, 4.1)));
}
