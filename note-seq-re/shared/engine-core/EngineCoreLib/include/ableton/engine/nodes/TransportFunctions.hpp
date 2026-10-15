#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/TransportFunctions.cpp
#include "ableton/support/Stubs.hpp"

#include <cstddef>

namespace ableton::engine {

    namespace transport {
        struct Loop {
            Beats start{};   // invariant: start < end
            Beats end{};
        };

        struct Quantum {
            Beats value{};  // TODO verify; assert "quantum > 0.0"
        };

        // 0x20 bytes. Layout read from songTimeToClipTime @0x10a5cc.
        struct ClipRegion {
            Beats start{};               // 0x00: clip time at the launch beat
            std::optional<Loop> oLoop;   // 0x08 start, 0x10 end, 0x18 engaged
        };

        // 0x60 bytes. A stretch of the audio block with a constant tempo.
        // Offsets are confirmed by beatTimeToFrameOffset, frameOffsetToBeatTime and cropSegment.
        // Names come from assertion strings and from transport::Setter<T, &Segment::field>
        // template arguments; the two ratio names are ours.
        struct Segment {
            Frames frameOffsetStart{};   // 0x00
            Frames frameOffsetEnd{};     // 0x08
            Beats beatTimeStart{};       // 0x10
            Beats beatTimeEnd{};         // 0x18
            bool isPlaying{};            // 0x20: when false, cropSegment keeps the beat times
            std::optional<Loop> oLoop;   // 0x28 start, 0x30 end, 0x38 engaged
            double tempo{};              // 0x40: inferred (only named slot left). Real type is a BPM quotient.
            double framesPerBeat{};      // 0x48: beat -> frame scale
            double beatsPerFrame{};      // 0x50: frame -> beat scale
            float grooveAmount{};        // 0x58
        };

        Segment cropSegment(transport::Segment const&, Frames, Frames);  // @0x10a4cc -> decomp/transport_cropSegment__000000000010a4cc.c
        Beats songTimeToClipTime(Beats, Beats, transport::ClipRegion const&);  // @0x10a5cc -> decomp/transport_songTimeToClipTime__000000000010a5cc.c
        Frames beatTimeToFrameOffset(transport::Segment const&, Beats);  // @0x10a3ec -> decomp/transport_beatTimeToFrameOffset__000000000010a3ec.c
        Beats frameOffsetToBeatTime(transport::Segment const&, Frames);  // @0x10a45c -> decomp/transport_frameOffsetToBeatTime__000000000010a45c.c
    }  // namespace transport

    static_assert(sizeof(transport::ClipRegion) == 0x20);
    static_assert(offsetof(transport::ClipRegion, oLoop) == 0x08);
    static_assert(sizeof(transport::Segment) == 0x60);
    static_assert(offsetof(transport::Segment, isPlaying) == 0x20);
    static_assert(offsetof(transport::Segment, oLoop) == 0x28);
    static_assert(offsetof(transport::Segment, framesPerBeat) == 0x48);
    static_assert(offsetof(transport::Segment, beatsPerFrame) == 0x50);
    static_assert(offsetof(transport::Segment, grooveAmount) == 0x58);

}  // namespace ableton::engine
