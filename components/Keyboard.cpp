#include "Keyboard.h"

#include <array>
#include <cmath>

namespace {

// UIKeyboard's constructor fills the table the first time any keyboard is
// built, guarded by a static flag.
const std::array<float, Keyboard::kNoteCount> &table() {
    static const std::array<float, Keyboard::kNoteCount> frequencies = [] {
        std::array<float, Keyboard::kNoteCount> values{};
        for (int note = 0; note < Keyboard::kNoteCount; ++note) {
            const float exponent =
                (static_cast<float>(note) - static_cast<float>(Keyboard::kReferenceNote)) / 12.0f;
            values[static_cast<std::size_t>(note)] = std::pow(2.0f, exponent) * Keyboard::kConcertA;
        }
        return values;
    }();
    return frequencies;
}

} // namespace

float Keyboard::frequencyFromNoteId(int note) {
    if (note < 0) {
        note = 0;
    } else if (note >= kNoteCount) {
        note = kNoteCount - 1;
    }
    return table()[static_cast<std::size_t>(note)];
}
