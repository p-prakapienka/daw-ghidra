// Reconstructed skeleton (stubs). Original: shared/engine-core/EngineCoreLib/src/nodes/TransportFunctions.cpp
#include "ableton/engine/nodes/TransportFunctions.hpp"

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

// @0x10a4cc -> decomp/transport_cropSegment__000000000010a4cc.c
Segment cropSegment(transport::Segment const& p0, Frames p1, Frames p2) {
    // TODO
    return {};
}

// @0x10a5cc -> decomp/transport_songTimeToClipTime__000000000010a5cc.c
Beats songTimeToClipTime(Beats p0, Beats p1, transport::ClipRegion const& p2) {
    // TODO
    return {};
}

// @0x10a3ec -> decomp/transport_beatTimeToFrameOffset__000000000010a3ec.c
Frames beatTimeToFrameOffset(transport::Segment const& p0, Beats p1) {
    // TODO
    return {};
}

// @0x10a45c -> decomp/transport_frameOffsetToBeatTime__000000000010a45c.c
Beats frameOffsetToBeatTime(transport::Segment const& p0, Frames p1) {
    // TODO
    return {};
}

}  // namespace ableton::engine::transport

