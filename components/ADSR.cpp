#include "ADSR.h"

#include <algorithm>
#include <cstring>

namespace {

// One in the 24-bit ramp domain.
constexpr int kOne24 = 0x1000000;

// The ramp starts one 512th below full scale. The engine writes this constant
// rather than kOne24 so that the first shaped value cannot overflow Q15.
constexpr int kRampStart = 0xFFFE00;

// Constants of the logarithmic shape: y = ((s * kLogGain) >> 15 << 15) / (s + kLogBias).
constexpr int kLogGain = 0x5998; // 22936, about 0.6999 in Q15
constexpr int kLogBias = 0x1A3D; // 6717

constexpr int kUnityValue = ADSR::kUnity;

constexpr int clampQ15(int value) {
    return std::clamp(value, -kUnityValue, kUnityValue);
}

// Negative values are floored at zero by the original code, which does this
// branchlessly as v & ~(v >> 15).
constexpr int floorAtZero(int value) {
    return value < 0 ? 0 : value;
}

} // namespace

ADSR::ADSR() {
    OnParamModified();
}

void ADSR::OnParamModified() {
    const auto resolve = [](float seconds, uint minimum) {
        // The engine converts with vcvt.s32.f32 and stores the result in an
        // unsigned field, so a negative time wraps to a very large value and is
        // then clamped to the maximum rather than the minimum. Converting
        // through int reproduces that without relying on undefined behaviour.
        const auto samples = static_cast<uint>(static_cast<int>(seconds * ADSR::kSampleRate));
        if (samples < minimum) {
            return minimum;
        }
        if (samples > ADSR::kMaxSegmentSamples) {
            return ADSR::kMaxSegmentSamples;
        }
        return samples;
    };

    attackSamples_ = resolve(attackSeconds_, minAttackSamples_);
    decaySamples_ = resolve(decaySeconds_, minDecaySamples_);
    releaseSamples_ = resolve(releaseSeconds_, minReleaseSamples_);
    sustainQ15_ = static_cast<short>(sustainLevel_ * 32767.0f);
}

void ADSR::SetMinSamples(uint attack, uint decay, uint release) {
    minAttackSamples_ = attack;
    minDecaySamples_ = decay;
    minReleaseSamples_ = release;
}

bool ADSR::IsDone(uint position) const {
    return releaseSamples_ + kReleaseTailSamples < position;
}

int ADSR::shape(int ramp24, Curve curve) {
    switch (curve) {
    case Curve::Linear: {
        // The ramp occupies bits 9..23, so this is simply ramp / 512.
        return static_cast<short>(static_cast<unsigned>(ramp24 << 7) >> 16);
    }
    case Curve::Exponential: {
        const int level = static_cast<short>(static_cast<unsigned>(ramp24 << 7) >> 16);
        return static_cast<short>(static_cast<unsigned>(level * level * 2) >> 16);
    }
    case Curve::Logarithmic:
    default: {
        // Half-scale ramp, then a hyperbola that reaches roughly full scale.
        const int level = static_cast<short>(static_cast<unsigned>(ramp24 << 6) >> 16);
        int divisor = level + kLogBias;
        if (divisor > kUnity - 1) {
            divisor = kUnity;
        }
        if (divisor == 0) {
            return 0;
        }
        const int scaled = ((level * kLogGain) >> 15) << 15;
        return static_cast<short>((scaled / divisor) * 2);
    }
    }
}

void ADSR::GenerateValues(uint position, uint gate, short *output, uint numSamples) {
    if (numSamples == 0) {
        return;
    }

    if (gate == 0) {
        generateRelease(position, output, numSamples);
    } else {
        generateAttackDecaySustain(position, output, numSamples);
    }
}

void ADSR::generateRelease(uint position, short *output, uint numSamples) {
    if (idle_) {
        std::memset(output, 0, numSamples * sizeof(short));
        return;
    }

    if (noteHeld_) {
        // First block after note-off: latch the segment so later parameter
        // edits cannot change the release in progress.
        heldReleaseSamples_ = releaseSamples_;
        releaseStartLevel_ = lastOutput_;
        releaseStep_ = heldReleaseSamples_ == 0
                           ? kOne24
                           : static_cast<int>(kOne24 / heldReleaseSamples_);
        noteHeld_ = false;
    }

    uint remaining = numSamples;
    short written = lastOutput_;

    if (position < heldReleaseSamples_) {
        int ramp = kRampStart - releaseStep_ * static_cast<int>(position);
        const uint segment = std::min(remaining, heldReleaseSamples_ - position);

        for (uint index = 0; index < segment; ++index) {
            const int level = shape(ramp, releaseCurve_);
            const int value = floorAtZero((level * releaseStartLevel_ * 2) >> 16);
            written = static_cast<short>(value);
            *output++ = written;
            ramp -= releaseStep_;
        }

        remaining -= segment;
    }

    if (remaining != 0) {
        std::memset(output, 0, remaining * sizeof(short));
        written = 0;
        idle_ = true;
    }

    lastOutput_ = written;
}

void ADSR::beginNote(uint position) {
    // A note starts when the position jumps backwards, which is how the engine
    // detects a retrigger.
    heldAttackSamples_ = attackSamples_;
    heldDecaySamples_ = decaySamples_;
    heldSustainQ15_ = sustainQ15_;
    attackStartLevel_ = 0;
    previousLevel_ = lastOutput_;
    attackStep_ = heldAttackSamples_ == 0
                      ? kOne24
                      : static_cast<int>(kOne24 / heldAttackSamples_);
    decayStep_ = heldDecaySamples_ == 0
                     ? kOne24
                     : static_cast<int>(kOne24 / heldDecaySamples_);
    lastPosition_ = position;
    idle_ = false;
}

void ADSR::generateAttackDecaySustain(uint position, short *output, uint numSamples) {
    if (!noteHeld_ || position < lastPosition_) {
        beginNote(position);
        noteHeld_ = true;
    }
    lastPosition_ = position;

    const int span = kUnity - attackStartLevel_;
    const uint decayEnd = heldAttackSamples_ + heldDecaySamples_;

    // The first samples of an attack fade in from whatever the previous note
    // left behind, which is what keeps a retrigger from clicking.
    const uint fadeLength = std::min(heldAttackSamples_, kRetriggerFadeSamples);

    uint remaining = numSamples;
    short written = lastOutput_;

    while (remaining != 0) {
        int value;

        if (position < heldAttackSamples_) {
            const int ramp = attackStep_ * static_cast<int>(position);
            value = attackStartLevel_ + ((shape(ramp, attackCurve_) * span) >> 15);

            if (position < fadeLength && fadeLength != 0) {
                const auto blend = static_cast<int>(
                    (static_cast<float>(position) / static_cast<float>(fadeLength)) * 32767.0f);
                value = previousLevel_ + ((blend * clampQ15(value - previousLevel_)) >> 15);
            }
        } else if (position < decayEnd) {
            const int ramp =
                kRampStart - decayStep_ * static_cast<int>(position - heldAttackSamples_);
            const int level = floorAtZero(shape(ramp, decayCurve_));
            value = heldSustainQ15_ + ((level * (kUnity - heldSustainQ15_)) >> 15);
        } else {
            value = heldSustainQ15_;
        }

        written = static_cast<short>(clampQ15(value));
        *output++ = written;
        ++position;
        --remaining;
    }

    lastOutput_ = written;
}
