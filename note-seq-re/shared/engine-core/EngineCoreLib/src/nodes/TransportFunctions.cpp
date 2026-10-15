// Reconstructed from ABLSwiftBridge (Ableton Note). Original: shared/engine-core/EngineCoreLib/src/nodes/TransportFunctions.cpp
// Notes: TransportFunctions.md next to this file.
#include "ableton/engine/nodes/TransportFunctions.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

// Assertions seen in the binary for this file (line: condition):
//   line 23: frameOffset >= segment.frameOffsetStart
//   line 24: frameOffset <= segment.frameOffsetEnd
//   line 47: beatTime >= segment.beatTimeStart
//   line 48: beatTime < segment.beatTimeEnd
//   line 60: frameOffset >= segment.frameOffsetStart
//   line 61: frameOffset < segment.frameOffsetEnd
//   line 73: newFrameStart >= segment.frameOffsetStart
//   line 74: newFrameEnd <= segment.frameOffsetEnd

namespace ableton::engine::transport {

namespace {

constexpr double kMinusInfinity = -std::numeric_limits<double>::infinity();

// Inlined into cropSegment; the name is ours. Unlike frameOffsetToBeatTime it accepts
// frameOffset == frameOffsetEnd and maps it exactly to beatTimeStart/End, and it clamps to
// beatTimeEnd itself rather than to the value just below it (asserts at lines 23-24).
Beats frameOffsetToBeatTimeInclusive(Segment const& segment, Frames frameOffset) {
    assert(frameOffset >= segment.frameOffsetStart);
    assert(frameOffset <= segment.frameOffsetEnd);
    if (frameOffset == segment.frameOffsetEnd) {
        return segment.beatTimeEnd;
    }
    Beats const beatTime =
        segment.beatTimeStart + (frameOffset - segment.frameOffsetStart) * segment.beatsPerFrame;
    return std::min(segment.beatTimeEnd, beatTime);
}

}  // namespace

// @0x10a3ec, 112 bytes
// Maps a beat time inside the segment to a frame offset. The result is clamped to the
// largest frame offset below frameOffsetEnd, so it always lands inside the segment.
Frames beatTimeToFrameOffset(Segment const& segment, Beats beatTime) {
    assert(beatTime >= segment.beatTimeStart);
    assert(beatTime < segment.beatTimeEnd);
    Frames const lastFrame = std::nextafter(segment.frameOffsetEnd, kMinusInfinity);
    Frames const frameOffset =
        segment.frameOffsetStart + (beatTime - segment.beatTimeStart) * segment.framesPerBeat;
    return std::min(lastFrame, frameOffset);
}

// @0x10a45c, 112 bytes
// The inverse: frame offset inside the segment to beat time, clamped below beatTimeEnd.
Beats frameOffsetToBeatTime(Segment const& segment, Frames frameOffset) {
    assert(frameOffset >= segment.frameOffsetStart);
    assert(frameOffset < segment.frameOffsetEnd);
    Beats const lastBeat = std::nextafter(segment.beatTimeEnd, kMinusInfinity);
    Beats const beatTime =
        segment.beatTimeStart + (frameOffset - segment.frameOffsetStart) * segment.beatsPerFrame;
    return std::min(lastBeat, beatTime);
}

// @0x10a4cc, 256 bytes
// Returns a copy of the segment narrowed to [newFrameStart, newFrameEnd). Beat times are
// re-derived only while the transport plays; a stopped segment keeps its beat times.
Segment cropSegment(Segment const& segment, Frames newFrameStart, Frames newFrameEnd) {
    assert(newFrameStart >= segment.frameOffsetStart);
    assert(newFrameEnd <= segment.frameOffsetEnd);
    Segment cropped = segment;
    cropped.frameOffsetStart = newFrameStart;
    cropped.frameOffsetEnd = newFrameEnd;
    if (cropped.isPlaying) {
        cropped.beatTimeStart = frameOffsetToBeatTimeInclusive(segment, newFrameStart);
        cropped.beatTimeEnd = frameOffsetToBeatTimeInclusive(segment, newFrameEnd);
    }
    return cropped;
}

// @0x10a5cc, 76 bytes
// Position inside a clip launched at launchBeat. Wraps into the loop once clip time reaches
// loop.end; before loop.start (an intro before the loop) it is left as is.
Beats songTimeToClipTime(Beats songTime, Beats launchBeat, ClipRegion const& region) {
    Beats clipTime = songTime - launchBeat + region.start;
    if (region.oLoop && clipTime >= region.oLoop->end) {
        Beats const loopStart = region.oLoop->start;
        clipTime = loopStart + std::fmod(clipTime - loopStart, region.oLoop->end - loopStart);
    }
    return clipTime;
}

}  // namespace ableton::engine::transport
