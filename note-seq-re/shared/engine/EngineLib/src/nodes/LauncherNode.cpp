// Reconstructed skeleton (stubs). Original: shared/engine/EngineLib/src/nodes/LauncherNode.cpp
#include "nodes/LauncherNode.hpp"

// Assertions seen in the binary for this file (line: condition):
//   line 39: segment.isPlaying
//   line 374: moPendingTrigger.has_value()

namespace ableton::engine::launcher {

// @0x3a8efc -> decomp/launcher_Clip_midi_player_Content_Clip__00000000003a8efc.c
template <typename Content>
Clip<Content>::~Clip() {
    // TODO
}

// @0x372ff4 -> decomp/launcher_Clip_midi_player_Content_operator___0000000000372ff4.c
template <typename Content>
Clip<Content>& Clip<Content>::operator=(Clip&& p0) {
    // TODO
    return *this;
}

}  // namespace ableton::engine::launcher

namespace ableton::engine::launcher::detail {

// @0x3a8e4c -> decomp/launcher_detail_ClipSlotsHolder_midi_player_Content_TemporaryClip_TemporaryClip__00000000003a8e4c.c
template <typename Content>
ClipSlotsHolder<Content>::TemporaryClip::~TemporaryClip() {
    // TODO
}

}  // namespace ableton::engine::launcher::detail

namespace ableton::engine::launcher::detail {

// @0x3a6c60 -> decomp/launcher_detail_ClipSlotsHolder_midi_player_Content_ensureFilledSlot__00000000003a6c60.c
template <typename Content>
void ClipSlotsHolder<Content>::ensureFilledSlot(unsigned long p0, Beats p1) {
    // TODO
}

}  // namespace ableton::engine::launcher::detail

namespace ableton::engine {

// @0x3a7724 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a7724.c
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& p0, launcher::StopRecording p1) {
    // TODO
}

// @0x3a75e4 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a75e4.c
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& p0, launcher::StartRecording p1) {
    // TODO
}

// @0x3a7598 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a7598.c
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& p0, launcher::RestartPlayingClip p1) {
    // TODO
}

// @0x3a7584 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a7584.c
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& p0, launcher::PlayingState const& p1) {
    // TODO
}

// @0x3a75f8 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a75f8.c
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& p0, launcher::TriggerRecording const& p1) {
    // TODO
}

// @0x3a6a30 -> decomp/LauncherNode_midi_player_Content_processCommand__00000000003a6a30.c
template <typename Content>
void LauncherNode<Content>::processCommand(AudioContext const& p0, launcher::Trigger const& p1) {
    // TODO
}

// @0x3a66d4 -> decomp/LauncherNode_midi_player_Content_processClipSlots__00000000003a66d4.c
template <typename Content>
void LauncherNode<Content>::processClipSlots(AudioContext const& p0, ableton::realtime::SharedPtr<std::vector<launcher::ClipSlot<Content>> const> const& p1) {
    // TODO
}

// @0x3a7d54 -> decomp/LauncherNode_midi_player_Content_finishPlayingClip__00000000003a7d54.c
template <typename Content>
void LauncherNode<Content>::finishPlayingClip(AudioContext const& p0) {
    // TODO
}

// @0x3a7c44 -> decomp/LauncherNode_midi_player_Content_launchPendingClip__00000000003a7c44.c
template <typename Content>
void LauncherNode<Content>::launchPendingClip(AudioContext const& p0) {
    // TODO
}

// @0x3a7d98 -> decomp/LauncherNode_midi_player_Content_adjustPlayingClipStartIfNeeded__00000000003a7d98.c
template <typename Content>
void LauncherNode<Content>::adjustPlayingClipStartIfNeeded(AudioContext const& p0) {
    // TODO
}

// @0x3a7744 -> decomp/LauncherNode_midi_player_Content_processPendingPlaybackOperations__00000000003a7744.c
template <typename Content>
void LauncherNode<Content>::processPendingPlaybackOperations(AudioContext p0) {
    // TODO
}

// @0x3a69fc -> decomp/LauncherNode_midi_player_Content_play__00000000003a69fc.c
template <typename Content>
void LauncherNode<Content>::play(AudioContext const& p0, launcher::ChangeOrigin p1, launcher::PlayingState const& p2, RecordingChange p3) {
    // TODO
}

// @0x3a6ec8 -> decomp/LauncherNode_midi_player_Content_setState__00000000003a6ec8.c
template <typename Content>
void LauncherNode<Content>::setState(AudioContext const& p0, launcher::ChangeOrigin p1, std::optional<PendingTrigger> const& p2, launcher::PlayingState const& p3, bool p4) {
    // TODO
}

// @0x3a7b9c -> decomp/LauncherNode_midi_player_Content_launchTimeIfDue__00000000003a7b9c.c
template <typename Content>
std::optional<Beats> LauncherNode<Content>::launchTimeIfDue(transport::Segment const& p0) const {
    // TODO
    return {};
}

// @0x3a7ae4 -> decomp/LauncherNode_midi_player_Content_clipStopTimeIfDue__00000000003a7ae4.c
template <typename Content>
std::optional<Beats> LauncherNode<Content>::clipStopTimeIfDue(transport::Segment const& p0) const {
    // TODO
    return {};
}

}  // namespace ableton::engine

// Explicit instantiations (the binary only contains these):
#include "ableton/engine/nodes/MidiPlayerContent.hpp"
template struct ableton::engine::launcher::Clip<ableton::engine::midi_player::Content>;
template struct ableton::engine::launcher::detail::ClipSlotsHolder<ableton::engine::midi_player::Content>;
template struct ableton::engine::LauncherNode<ableton::engine::midi_player::Content>;
