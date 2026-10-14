// Reconstructed skeleton (stubs). Original: shared/engine/EngineLib/src/nodes/StepArrangementNode.cpp
#include "nodes/StepArrangementNode.hpp"

// Assertions seen in the binary for this file (line: condition):
//   line 24: !oState.has_value() || oState->stepIndex < mpSteps->size()
//   line 244: iNewStep > steps.begin()

namespace ableton::engine {

// @0x3b43fc -> decomp/StepArrangementNode_launchStep__00000000003b43fc.c
void StepArrangementNode::launchStep(std::vector<Step> const& p0, transport::Segment const& p1, Outputs const& p2) {
    // TODO
}

// @0x3b47c4 -> decomp/StepArrangementNode_updateSteps__00000000003b47c4.c
void StepArrangementNode::updateSteps(ableton::realtime::SharedPtr<std::vector<Step> const> const& p0, transport::Segment const& p1, Outputs const& p2) {
    // TODO
}

// @0x3b4354 -> decomp/StepArrangementNode_launchNextStep__00000000003b4354.c
void StepArrangementNode::launchNextStep(std::vector<Step> const& p0, transport::Segment const& p1, CvBuffer<std::optional<step_arrangement::PlayingState>, Frames>& p2) {
    // TODO
}

// @0x3b46dc -> decomp/StepArrangementNode_processCommand__00000000003b46dc.c
void StepArrangementNode::processCommand(std::variant<Trigger, EnableAll, Stop> const& p0, transport::Segment const& p1, Outputs const& p2) {
    // TODO
}

// @0x3b408c -> decomp/StepArrangementNode_setPlayingState__00000000003b408c.c
void StepArrangementNode::setPlayingState(std::optional<PlayingState> const& p0, CvBuffer<std::optional<step_arrangement::PlayingState>, Frames>& p1, Frames p2) {
    // TODO
}

// @0x3b475c -> decomp/StepArrangementNode_makeNewStepIndex__00000000003b475c.c
void StepArrangementNode::makeNewStepIndex(ableton::realtime::SharedPtr<std::vector<Step> const> const& p0, ableton::realtime::SharedPtr<std::vector<Step> const> const& p1, unsigned long p2) {
    // TODO
}

// @0x3b3f40 -> decomp/StepArrangementNode_setTriggeredState__00000000003b3f40.c
void StepArrangementNode::setTriggeredState(std::optional<TriggeredState> const& p0, CvBuffer<std::optional<TriggeredState>, Frames>& p1, Frames p2) {
    // TODO
}

// @0x3b4998 -> decomp/StepArrangementNode_handleBeatTimeJump__00000000003b4998.c
void StepArrangementNode::handleBeatTimeJump(std::vector<Step> const& p0, transport::Segment const& p1, Outputs const& p2) {
    // TODO
}

// @0x3b4238 -> decomp/StepArrangementNode_launchTriggeredStep__00000000003b4238.c
void StepArrangementNode::launchTriggeredStep(Beats p0, std::vector<Step> const& p1, transport::Segment const& p2, Outputs const& p3) {
    // TODO
}

// @0x3b41d4 -> decomp/StepArrangementNode_stop__00000000003b41d4.c
void StepArrangementNode::stop(Frames p0, Outputs const& p1) {
    // TODO
}

// @0x3b4590 -> decomp/StepArrangementNode_advance__00000000003b4590.c
void StepArrangementNode::advance(std::vector<Step> const& p0, transport::Segment const& p1, Outputs const& p2) {
    // TODO
}

}  // namespace ableton::engine

