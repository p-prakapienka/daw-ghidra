#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine/EngineLib/src/layers/SyncClipSlots.cpp
#include "ableton/support/Stubs.hpp"
#include "ableton/engine/nodes/MidiPlayerContent.hpp"
#include "nodes/LauncherNode.hpp"

namespace ableton::engine {

    void syncClipSlots(ableton::realtime::Collector&, ParameterIdRegistry const&, ableton::flip_model::FGroovePool const&, ableton::flip_model::ConstArrayRef<ableton::flip_model::FClipSlot, ableton::flip_model::arrayref_detail::NonremovedElementIteratorTrait<ableton::flip_model::FClipSlot>>, std::optional<std::vector<launcher::ClipSlot<midi_player::Content>>>&);  // @0x361e24 -> decomp/syncClipSlots__0000000000361e24.c
}  // namespace ableton::engine
