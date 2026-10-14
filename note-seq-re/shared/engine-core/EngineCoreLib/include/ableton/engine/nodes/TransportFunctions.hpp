#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/TransportFunctions.cpp
#include "ableton/support/Stubs.hpp"

namespace ableton::engine {

    namespace transport {
        struct Loop {
            Beats start{};   // invariant: start < end
            Beats end{};
        };

        struct Quantum {
            Beats value{};  // TODO verify; assert "quantum > 0.0"
        };

        struct ClipRegion {
            // From songTimeToClipTime @0x10a5cc: clipTime = song - launch + start; wrap into [loop.start, loop.end).
            Beats start{};
            std::optional<Loop> oLoop;
        };

        struct Segment {
            // Field names from pointer-to-member template args (transport::Setter<...>) and assertion strings.
            // Observed layout (offsets in bytes): 0x00 frameOffsetStart, 0x08 frameOffsetEnd, 0x10 beatTimeStart,
            // 0x18 beatTimeEnd, 0x20 isPlaying, 0x28 oLoop{start,end,engaged@0x38}, 0x40.. tempo / groove / ratios (TODO).
            Frames frameOffsetStart{};
            Frames frameOffsetEnd{};
            Beats beatTimeStart{};
            Beats beatTimeEnd{};
            bool isPlaying{};
            std::optional<Loop> oLoop;
            double tempo{};  // TODO: real type is a BPM quotient
            float grooveAmount{};
            double framesPerBeat{};  // TODO verify (used as [9] in beatTimeToFrameOffset)
            double beatsPerFrame{};  // TODO verify (used as [10] in frameOffsetToBeatTime)
        };

        Segment cropSegment(transport::Segment const&, Frames, Frames);  // @0x10a4cc -> decomp/transport_cropSegment__000000000010a4cc.c
        Beats songTimeToClipTime(Beats, Beats, transport::ClipRegion const&);  // @0x10a5cc -> decomp/transport_songTimeToClipTime__000000000010a5cc.c
        Frames beatTimeToFrameOffset(transport::Segment const&, Beats);  // @0x10a3ec -> decomp/transport_beatTimeToFrameOffset__000000000010a3ec.c
        Beats frameOffsetToBeatTime(transport::Segment const&, Frames);  // @0x10a45c -> decomp/transport_frameOffsetToBeatTime__000000000010a45c.c
    }  // namespace transport

}  // namespace ableton::engine
