#include "ableton/engine/nodes/TransportFunctions.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace ableton::engine::transport;

namespace {

// 512 frames at 120 BPM and 44.1 kHz: 22050 frames per beat.
Segment playingSegment() {
    Segment segment;
    segment.frameOffsetStart = 0.0;
    segment.frameOffsetEnd = 512.0;
    segment.framesPerBeat = 22050.0;
    segment.beatsPerFrame = 1.0 / 22050.0;
    segment.beatTimeStart = 4.0;
    segment.beatTimeEnd = 4.0 + 512.0 / 22050.0;
    segment.isPlaying = true;
    segment.tempo = 120.0;
    segment.grooveAmount = 0.25f;
    segment.oLoop = Loop{0.0, 8.0};
    return segment;
}

double below(double value) { return std::nextafter(value, -std::numeric_limits<double>::infinity()); }

} // namespace

TEST(TransportBeatTimeToFrameOffset, IsLinearFromTheSegmentStart) {
    Segment const segment = playingSegment();
    EXPECT_DOUBLE_EQ(beatTimeToFrameOffset(segment, 4.0), 0.0);
    EXPECT_NEAR(beatTimeToFrameOffset(segment, 4.0 + 256.0 / 22050.0), 256.0, 1e-9);
}

TEST(TransportBeatTimeToFrameOffset, NeverReachesTheSegmentEnd) {
    Segment const segment = playingSegment();
    EXPECT_LT(beatTimeToFrameOffset(segment, below(segment.beatTimeEnd)), segment.frameOffsetEnd);
}

TEST(TransportFrameOffsetToBeatTime, IsLinearFromTheSegmentStart) {
    Segment const segment = playingSegment();
    EXPECT_DOUBLE_EQ(frameOffsetToBeatTime(segment, 0.0), 4.0);
    EXPECT_NEAR(frameOffsetToBeatTime(segment, 256.0), 4.0 + 256.0 / 22050.0, 1e-12);
}

TEST(TransportFrameOffsetToBeatTime, ClampsJustBelowTheBeatTimeEnd) {
    Segment segment = playingSegment();
    segment.beatsPerFrame *= 2.0; // overshoot on purpose
    EXPECT_EQ(frameOffsetToBeatTime(segment, 511.0), below(segment.beatTimeEnd));
}

TEST(TransportCropSegment, NarrowsTheFramesAndRederivesTheBeats) {
    Segment const segment = playingSegment();
    Segment const cropped = cropSegment(segment, 128.0, 384.0);
    EXPECT_EQ(cropped.frameOffsetStart, 128.0);
    EXPECT_EQ(cropped.frameOffsetEnd, 384.0);
    EXPECT_NEAR(cropped.beatTimeStart, 4.0 + 128.0 / 22050.0, 1e-12);
    EXPECT_NEAR(cropped.beatTimeEnd, 4.0 + 384.0 / 22050.0, 1e-12);
}

TEST(TransportCropSegment, KeepsTheExactEndBeatWhenTheEndIsUnchanged) {
    Segment const segment = playingSegment();
    EXPECT_EQ(cropSegment(segment, 100.0, 512.0).beatTimeEnd, segment.beatTimeEnd);
    EXPECT_EQ(cropSegment(segment, 512.0, 512.0).beatTimeStart, segment.beatTimeEnd);
}

TEST(TransportCropSegment, KeepsTheBeatTimesWhileStopped) {
    Segment segment = playingSegment();
    segment.isPlaying = false;
    Segment const cropped = cropSegment(segment, 128.0, 384.0);
    EXPECT_EQ(cropped.beatTimeStart, segment.beatTimeStart);
    EXPECT_EQ(cropped.beatTimeEnd, segment.beatTimeEnd);
}

TEST(TransportCropSegment, CopiesTheOtherFields) {
    Segment const segment = playingSegment();
    Segment const cropped = cropSegment(segment, 0.0, 256.0);
    EXPECT_TRUE(cropped.isPlaying);
    ASSERT_TRUE(cropped.oLoop.has_value());
    EXPECT_EQ(cropped.oLoop->end, 8.0);
    EXPECT_EQ(cropped.tempo, 120.0);
    EXPECT_EQ(cropped.grooveAmount, 0.25f);
}

TEST(TransportSongTimeToClipTime, CountsFromTheLaunchBeat) {
    ClipRegion const region{2.0, std::nullopt};
    EXPECT_EQ(songTimeToClipTime(10.0, 8.0, region), 4.0);
}

TEST(TransportSongTimeToClipTime, WrapsIntoTheLoop) {
    ClipRegion const region{0.0, Loop{0.0, 4.0}};
    EXPECT_EQ(songTimeToClipTime(9.0, 0.0, region), 1.0);
    EXPECT_EQ(songTimeToClipTime(4.0, 0.0, region), 0.0);
}

TEST(TransportSongTimeToClipTime, WrapsToTheLoopStartNotTheClipStart) {
    ClipRegion const region{0.0, Loop{2.0, 6.0}};
    EXPECT_EQ(songTimeToClipTime(7.0, 0.0, region), 3.0); // 6 -> 2, so 7 -> 3
}

TEST(TransportSongTimeToClipTime, LeavesAnIntroBeforeTheLoopAlone) {
    ClipRegion const region{0.0, Loop{2.0, 6.0}};
    EXPECT_EQ(songTimeToClipTime(1.0, 0.0, region), 1.0);
}

TEST(TransportBeatTimeToFrameOffsetDeathTest, AssertsTheBeatIsInsideTheSegment) {
    Segment const segment = playingSegment();
    EXPECT_DEBUG_DEATH(beatTimeToFrameOffset(segment, segment.beatTimeEnd), "beatTime < segment.beatTimeEnd");
}
