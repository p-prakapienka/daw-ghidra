#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/ClipTimeNode.cpp
#include "ableton/support/Stubs.hpp"
#include "ableton/engine/nodes/TransportFunctions.hpp"

namespace ableton::engine {

    struct ClipTimeNode {
        struct PlayingClipFrame {
            // TODO: fields
        };

        void sendTimelines(ProcessContext const&, EventBuffer<transport::Segment, Frames, NoopEventBufferOverflowHandler<transport::Segment, Frames>> const&, CvBuffer<ableton::time::Timeline, Frames>&);  // @0xec294 -> decomp/ClipTimeNode_sendTimelines__00000000000ec294.c
    };

}  // namespace ableton::engine
