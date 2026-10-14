// Reconstructed skeleton (stubs). Original: shared/engine-core/EngineCoreLib/src/nodes/ClipTimeNode.cpp
#include "ableton/engine/nodes/ClipTimeNode.hpp"

// Assertions seen in the binary for this file (line: condition):
//   line 166: globalSegment.beatTimeStart <= loopEndInGlobalTime

namespace ableton::engine {

// @0xec294 -> decomp/ClipTimeNode_sendTimelines__00000000000ec294.c
void ClipTimeNode::sendTimelines(ProcessContext const& p0, EventBuffer<transport::Segment, Frames, NoopEventBufferOverflowHandler<transport::Segment, Frames>> const& p1, CvBuffer<ableton::time::Timeline, Frames>& p2) {
    // TODO
}

}  // namespace ableton::engine

