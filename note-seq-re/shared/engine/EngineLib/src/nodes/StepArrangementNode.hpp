#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine/EngineLib/src/nodes/StepArrangementNode.cpp
#include "ableton/support/Stubs.hpp"
#include "ableton/engine/nodes/TransportFunctions.hpp"

namespace ableton::engine {

    namespace step_arrangement {
        struct PlayingState {
            // TODO: fields
        };

        struct TrackCommand {
            // TODO: fields
        };

    }  // namespace step_arrangement

    struct StepArrangementNode {
        struct EnableAll {
            // TODO: fields
        };

        struct Outputs {
            // TODO: fields
        };

        struct PlayingState {
            // TODO: fields
        };

        struct Step {
            // 24-byte element (index*0x18 in launchNextStep); length at +8
            int sceneIndex{};  // TODO verify
            Beats length{};
            double reserved{};  // TODO
        };

        struct Stop {
            // TODO: fields
        };

        struct Trigger {
            // TODO: fields
        };

        struct TriggeredState {
            // TODO: fields
        };

        void launchStep(std::vector<Step> const&, transport::Segment const&, Outputs const&);  // @0x3b43fc -> decomp/StepArrangementNode_launchStep__00000000003b43fc.c
        void updateSteps(ableton::realtime::SharedPtr<std::vector<Step> const> const&, transport::Segment const&, Outputs const&);  // @0x3b47c4 -> decomp/StepArrangementNode_updateSteps__00000000003b47c4.c
        void launchNextStep(std::vector<Step> const&, transport::Segment const&, CvBuffer<std::optional<step_arrangement::PlayingState>, Frames>&);  // @0x3b4354 -> decomp/StepArrangementNode_launchNextStep__00000000003b4354.c
        void processCommand(std::variant<Trigger, EnableAll, Stop> const&, transport::Segment const&, Outputs const&);  // @0x3b46dc -> decomp/StepArrangementNode_processCommand__00000000003b46dc.c
        void setPlayingState(std::optional<PlayingState> const&, CvBuffer<std::optional<step_arrangement::PlayingState>, Frames>&, Frames);  // @0x3b408c -> decomp/StepArrangementNode_setPlayingState__00000000003b408c.c
        void makeNewStepIndex(ableton::realtime::SharedPtr<std::vector<Step> const> const&, ableton::realtime::SharedPtr<std::vector<Step> const> const&, unsigned long);  // @0x3b475c -> decomp/StepArrangementNode_makeNewStepIndex__00000000003b475c.c
        void setTriggeredState(std::optional<TriggeredState> const&, CvBuffer<std::optional<TriggeredState>, Frames>&, Frames);  // @0x3b3f40 -> decomp/StepArrangementNode_setTriggeredState__00000000003b3f40.c
        void handleBeatTimeJump(std::vector<Step> const&, transport::Segment const&, Outputs const&);  // @0x3b4998 -> decomp/StepArrangementNode_handleBeatTimeJump__00000000003b4998.c
        void launchTriggeredStep(Beats, std::vector<Step> const&, transport::Segment const&, Outputs const&);  // @0x3b4238 -> decomp/StepArrangementNode_launchTriggeredStep__00000000003b4238.c
        void stop(Frames, Outputs const&);  // @0x3b41d4 -> decomp/StepArrangementNode_stop__00000000003b41d4.c
        void advance(std::vector<Step> const&, transport::Segment const&, Outputs const&);  // @0x3b4590 -> decomp/StepArrangementNode_advance__00000000003b4590.c
    };

}  // namespace ableton::engine
