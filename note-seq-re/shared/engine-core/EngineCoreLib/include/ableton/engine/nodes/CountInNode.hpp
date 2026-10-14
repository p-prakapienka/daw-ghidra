#pragma once
// Reconstructed skeleton (stubs) of Ableton Note's engine - for personal study.
// Original translation unit: shared/engine-core/EngineCoreLib/src/nodes/CountInNode.cpp
#include "ableton/support/Stubs.hpp"

namespace ableton::engine {

    struct CountInNode {
        struct Context {
            // TODO: fields
        };

        struct StartCountIn {
            // TODO: fields
        };

        struct StopCountIn {
            // TODO: fields
        };

        void stopCountIn(Frames, ableton::datatype::Quotient<ableton::datatype::Unit<ableton::time::units::FrameKind, std::ratio<1l, 1l>>, ableton::datatype::Unit<ableton::time::units::SecondKind, std::ratio<1l, 1l>>, double>);  // @0xed860 -> decomp/CountInNode_stopCountIn__00000000000ed860.c
        void startCountIn(Beats, Beats, Frames, ableton::datatype::Quotient<ableton::datatype::Unit<ableton::time::units::FrameKind, std::ratio<1l, 1l>>, ableton::datatype::Unit<ableton::time::units::SecondKind, std::ratio<1l, 1l>>, double>);  // @0xed8d8 -> decomp/CountInNode_startCountIn__00000000000ed8d8.c
        void setCountInTempo(ableton::datatype::Quotient<ableton::datatype::Unit<ableton::time::units::BeatKind, std::ratio<1l, 1l>>, ableton::datatype::Unit<ableton::time::units::SecondKind, std::ratio<60l, 1l>>, double>, Frames, ableton::datatype::Quotient<ableton::datatype::Unit<ableton::time::units::FrameKind, std::ratio<1l, 1l>>, ableton::datatype::Unit<ableton::time::units::SecondKind, std::ratio<1l, 1l>>, double>);  // @0xeda10 -> decomp/CountInNode_setCountInTempo__00000000000eda10.c
        void processCountInEnd(Context const&);  // @0xed67c -> decomp/CountInNode_processCountInEnd__00000000000ed67c.c
    };

}  // namespace ableton::engine
