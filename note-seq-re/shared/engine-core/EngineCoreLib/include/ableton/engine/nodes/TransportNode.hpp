#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/TransportNode.cpp
#include "ableton/support/Stubs.hpp"
#include "ableton/engine/nodes/TransportFunctions.hpp"

namespace ableton::engine {

    struct TransportNode {
        struct StateChange {
            // TODO: fields
        };

        void addLoopJumps(ProcessContext const&, EventBuffer<transport::Segment, Frames, NoopEventBufferOverflowHandler<transport::Segment, Frames>>&, CvBuffer<ableton::time::Timeline, Frames>&, Frames);  // @0x10aabc -> decomp/TransportNode_addLoopJumps__000000000010aabc.c
        void advanceTimeTo(ProcessContext const&, EventBuffer<transport::Segment, Frames, NoopEventBufferOverflowHandler<transport::Segment, Frames>>&, CvBuffer<ableton::time::Timeline, Frames>&, Frames);  // @0x10ac24 -> decomp/TransportNode_advanceTimeTo__000000000010ac24.c
        void appendSegment(EventBuffer<transport::Segment, Frames, NoopEventBufferOverflowHandler<transport::Segment, Frames>>&, transport::Segment const&);  // @0x10acf4 -> decomp/TransportNode_appendSegment__000000000010acf4.c
        void addBeatTimeJump(ProcessContext const&, EventBuffer<transport::Segment, Frames, NoopEventBufferOverflowHandler<transport::Segment, Frames>>&, CvBuffer<ableton::time::Timeline, Frames>&, Frames, Beats, Beats);  // @0x10a8d4 -> decomp/TransportNode_addBeatTimeJump__000000000010a8d4.c
        void applyStateChanges(ProcessContext const&, ableton::time::Timeline const&, transport::Segment const&, transport::Segment const&, bool, Frames);  // @0x10a7a8 -> decomp/TransportNode_applyStateChanges__000000000010a7a8.c
        void prepareNextSegment(transport::Segment, Frames, Beats);  // @0x10a864 -> decomp/TransportNode_prepareNextSegment__000000000010a864.c
        TransportNode(std::shared_ptr<transport::Segment>);  // @0x10a770 -> decomp/TransportNode_TransportNode__000000000010a770.c; @0x10a738 -> decomp/TransportNode_TransportNode__000000000010a738.c
    };

}  // namespace ableton::engine
