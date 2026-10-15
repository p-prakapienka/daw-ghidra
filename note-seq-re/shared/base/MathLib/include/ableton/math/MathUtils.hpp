#pragma once
// Reconstructed from ABLSwiftBridge (Ableton Note). Original: shared/base/MathLib/include/ableton/math/MathUtils.ipp
// Only the parts the reconstructed engine uses so far.

#include <cassert>
#include <cmath>

namespace ableton::math {

// 2^53: above this a double no longer represents every integer.
inline constexpr double kExactIntRangeMaxForDouble = 9007199254740992.0;

namespace detail {

// Shared by the multiple-rounding functions (asserts at MathUtils.ipp lines 24 and 27, also
// seen in roundToNearestMultiple @0x920530). The name is ours.
inline double nearestQuants(double value, double quantum) {
    assert(quantum > 0.0);
    double const quants = value / quantum;
    assert(std::abs(quants) < kExactIntRangeMaxForDouble);
    return std::round(quants);
}

}  // namespace detail

// Smallest multiple of |quantum| not below value; a zero quantum returns value unchanged.
// Only seen inlined (LauncherNode::launchTimeIfDue @0x3a7b9c); the name is ours. It rounds to
// the nearest multiple first and steps up one quantum if that lies below value, which is not
// always bit-identical to std::ceil(value / quantum) * quantum.
inline double roundUpToMultiple(double value, double quantum) {
    if (quantum == 0.0) {
        return value;
    }
    quantum = std::abs(quantum);
    double const quants = detail::nearestQuants(value, quantum);
    double const lower = quantum * quants;
    return lower < value ? quantum * (quants + 1.0) : lower;
}

}  // namespace ableton::math
