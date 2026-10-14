// Reconstructed skeleton (stubs). Original: shared/engine-core/EngineCoreLib/src/nodes/TransportNode.cpp
#include "ableton/engine/nodes/TransportNode.hpp"

// Assertions seen in the binary for this file (line: condition):
//   line 111: lastSegment.frameOffsetStart < frameOffset
//   line 138: loop.start < loop.end
//   line 143: oFrameTime.has_value()
//   line 172: upToFrameOffset >= lastSegment.frameOffsetStart

namespace ableton::engine {

// @0x10aabc -> decomp/TransportNode_addLoopJumps__000000000010aabc.c
void TransportNode::addLoopJumps(ProcessContext const& p0, EventBuffer<transport::Segment, Frames, NoopEventBufferOverflowHandler<transport::Segment, Frames>>& p1, CvBuffer<ableton::time::Timeline, Frames>& p2, Frames p3) {
    // TODO
}

// @0x10ac24 -> decomp/TransportNode_advanceTimeTo__000000000010ac24.c
void TransportNode::advanceTimeTo(ProcessContext const& p0, EventBuffer<transport::Segment, Frames, NoopEventBufferOverflowHandler<transport::Segment, Frames>>& p1, CvBuffer<ableton::time::Timeline, Frames>& p2, Frames p3) {
    // TODO
}

// @0x10acf4 -> decomp/TransportNode_appendSegment__000000000010acf4.c
void TransportNode::appendSegment(EventBuffer<transport::Segment, Frames, NoopEventBufferOverflowHandler<transport::Segment, Frames>>& p0, transport::Segment const& p1) {
    // TODO
}

// @0x10a8d4 -> decomp/TransportNode_addBeatTimeJump__000000000010a8d4.c
void TransportNode::addBeatTimeJump(ProcessContext const& p0, EventBuffer<transport::Segment, Frames, NoopEventBufferOverflowHandler<transport::Segment, Frames>>& p1, CvBuffer<ableton::time::Timeline, Frames>& p2, Frames p3, Beats p4, Beats p5) {
    // TODO
}

// @0x10a7a8 -> decomp/TransportNode_applyStateChanges__000000000010a7a8.c
void TransportNode::applyStateChanges(ProcessContext const& p0, ableton::time::Timeline const& p1, transport::Segment const& p2, transport::Segment const& p3, bool p4, Frames p5) {
    // TODO
}

// @0x10a864 -> decomp/TransportNode_prepareNextSegment__000000000010a864.c
void TransportNode::prepareNextSegment(transport::Segment p0, Frames p1, Beats p2) {
    // TODO
}

// @0x10a770 -> decomp/TransportNode_TransportNode__000000000010a770.c; @0x10a738 -> decomp/TransportNode_TransportNode__000000000010a738.c
TransportNode::TransportNode(std::shared_ptr<transport::Segment> p0) {
    // TODO
}

}  // namespace ableton::engine

