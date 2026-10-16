#pragma once
// Minimal stand-ins for Ableton's support libraries, so the skeleton compiles.
// Replace these with real implementations as the reconstruction progresses.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <ratio>
#include <variant>
#include <vector>
#include <map>
#include <utility>
#include <functional>
#include <bitset>
#include <unordered_map>

namespace ableton {

namespace time::units { struct BeatKind {}; struct FrameKind {}; struct SecondKind {}; }
namespace datatype {
template <class Kind, class Ratio> struct Unit {};
// Strongly-typed quantity in the original (Distance<Unit<Kind, Ratio>, Rep>).
// Here the common ones are plain doubles via the aliases below; this template only
// exists so less common spellings in signatures still compile.
template <class U, class Rep> struct Distance { Rep value{}; };
template <class... T> struct Quotient { double value{}; };
}  // namespace datatype

// Original: Distance<Unit<BeatKind|FrameKind|SecondKind, std::ratio<1,1>>, double>
using Beats = double;
using Frames = double;
using Seconds = double;
// Original: Distance<Unit<FrameKind, std::ratio<1,1>>, long long> (integer sample count)
using FrameCount = std::int64_t;

namespace realtime {
template <class T> using SharedPtr = std::shared_ptr<T>;   // original: lock-free refcounted pointer
struct Collector {};
}
namespace time { struct Timeline {}; struct Groove {}; }
namespace utility { template <class... T> struct Subrange {}; }

namespace midi {
struct NoteOn {}; struct NoteOff {}; struct ControlChange {}; struct PerNoteControlChange {};
struct PitchBend {}; struct ChannelPressure {}; struct PolyPressure {};
struct RealtimeClock {}; struct RealtimeStart {}; struct RealtimeContinue {}; struct RealtimeStop {}; struct RealtimeReset {};
}

namespace flip_model {
struct FGroovePool {}; struct FClipSlot {}; struct TransactionHub {}; struct ParameterGestureChange {}; struct ParameterValueChange {};
struct PlayingClipChange {}; struct ClipTimelineChange {}; struct SongTimelineChange {};
template <class... T> struct ConstArrayRef {};
template <class... T> struct EngineSignal {};
template <class... T> struct EngineEvent {};
namespace arrayref_detail { template <class... T> struct NonremovedElementIteratorTrait {}; }
}

namespace engine {
// Engine framework placeholders (graph, buffers, recording, parameters).
struct ProcessContext {}; struct ReaderContext {}; struct RecordingContext {}; struct RecordingConfig {};
struct RecordingTransaction {}; struct RealtimeRecordingState {}; struct RealtimeRecordingStates {};
struct SchedulerSlot {}; struct ParameterIdRegistry {}; struct ParameterIdTag {}; struct DeviceIdTag {};
template <class Tag> struct ModelId {};
template <class... T> struct NoopEventBufferOverflowHandler {};

// A value stamped with the time it takes effect (the original keeps the time first, then the value).
template <class Value, class Time>
struct Event {
    Time time{};
    Value value{};
};

// Stand-ins for the engine's port buffers: they record every event the node sends, in order.
// The real CvBuffer holds a control value that persists until changed, and its emplace_back drops
// a value equal to the last one and replaces an event at the same time. The real EventBuffer is a
// fixed-capacity vector. Neither behaviour matters to the nodes reconstructed so far.
template <class Value, class Time>
struct CvBuffer {
    void emplace_back(Time time, Value value) { events.push_back({time, std::move(value)}); }
    std::vector<Event<Value, Time>> events;
};

template <class Value, class Time, class OverflowHandler = NoopEventBufferOverflowHandler<Value, Time>>
struct EventBuffer {
    Event<Value, Time>& emplace_back(Time const& time, Value&& value) {
        return events.emplace_back(Event<Value, Time>{time, std::move(value)});
    }
    std::vector<Event<Value, Time>> events;
};
template <class... T> struct RampTracker {};
template <class... T> struct CompleteRamp {};
struct AudioMeterNode { struct NoValue {}; };
struct MidiRampInterpolatorNode { struct NoValue {}; };
template <class T> struct ParameterRampTerminatorNode { struct RampEnd {}; };
namespace detail { template <class T> struct Smoother { struct ScheduledOperation {}; }; }
}  // namespace engine
}  // namespace ableton

namespace boost::container {
template <class K, class V, class... R> using flat_map = std::map<K, V>;
}
