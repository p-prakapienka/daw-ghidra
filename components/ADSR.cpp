#include "ADSR.h"

#include <algorithm>
#include <cstring>

namespace {

// The ramps run in a 24-bit domain from one 512th below full scale.
constexpr int kRampStart = 0xFFFE00;

// Constants of the logarithmic shape.
constexpr int kLogGain = 0x5998; // 22936
constexpr int kLogBias = 0x1A3D; // 6717

// smulbb: the signed low halves of both operands, multiplied.
inline int mulLow(int a, int b) {
    return static_cast<short>(a) * static_cast<short>(b);
}

// sbfx #15, #16: bits 15..30 of a product, as a signed 16-bit value.
inline int high15(int product) {
    return static_cast<short>(product >> 15);
}

// The engine's clamp to -0x7FFF .. 0x7FFF.
inline int clamp16(int value) {
    if (value >= 0x8000) {
        return 0x7FFF;
    }
    if (value < -0x7FFF) {
        return -0x7FFF;
    }
    return value;
}

// The negative-floor the engine writes as bic r, r, r, asr #31.
inline int floorAtZero(int value) {
    return value < 0 ? 0 : value;
}

// The logarithmic shape of a 16-bit ramp value s:
// ((s * 0x5998) >> 15 << 15) / min(s + 6717, 0x7FFF), integer division.
inline int logShape(int s) {
    int divisor = static_cast<short>(s) + kLogBias;
    if (divisor >= 0x7FFF) {
        divisor = 0x7FFF;
    }
    return ((mulLow(s, kLogGain) >> 15) << 15) / divisor;
}

// The exponential shape: the square of a 16-bit ramp value in Q15, keeping 16 bits.
inline int expShape(int s) {
    return (mulLow(s, s) >> 15) & 0xFFFF;
}

// vcvt.s32.f32: truncates, clamps at both ends, and turns NaN into 0. A
// plain static_cast is undefined out of range and gives INT_MIN on x86.
int toInt(float value) {
    if (value != value) {
        return 0;
    }
    if (value >= 2147483648.0f) {
        return 0x7FFFFFFF;
    }
    if (value <= -2147483648.0f) {
        return -0x7FFFFFFF - 1;
    }
    return static_cast<int>(value);
}

// 2^24 / length in float, converted with vcvt.s32.f32 (so a zero length gives
// INT_MAX).
int stepFor(uint length) {
    return toInt(16777216.0f / static_cast<float>(length));
}

// The weight of the click-suppressing fade at a position, Q15.
inline int fadeWeight(uint position, uint fadeLength) {
    const float weight = static_cast<float>(static_cast<int>(position))
                         / static_cast<float>(static_cast<int>(fadeLength)) * 32767.0f;
    return static_cast<short>(static_cast<ushort>(static_cast<int>(weight)));
}

// One step of the fade from the previously written sample towards the shaped
// value, as the linear and exponential attacks do it.
inline int fadeFrom(int previous, int shaped, uint position, uint fadeLength) {
    const int difference = clamp16(shaped + static_cast<short>(-previous));
    return clamp16(((fadeWeight(position, fadeLength) * difference) >> 15)
                      + static_cast<short>(previous));
}

} // namespace

ADSR::ADSR() {
    OnParamModified();
}

void ADSR::OnParamModified() {
    const auto resolve = [](float seconds, uint minimum) {
        // The engine converts with vcvt.s32.f32 and stores the result in an
        // unsigned field, so a negative time wraps to a very large value and is
        // then clamped to the maximum rather than the minimum.
        const auto samples = static_cast<uint>(toInt(seconds * ADSR::kSampleRate));
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
    // vcvt.s32.f32, then strh: the low 16 bits of the clamped integer. The
    // amplitude envelopes' constructor sustain of 16777215.0 lands on 0xFFFF.
    sustainQ15_ = static_cast<short>(toInt(sustainLevel_ * 32767.0f));
}

void ADSR::SetMinSamples(uint attack, uint decay, uint release) {
    minAttackSamples_ = attack;
    minDecaySamples_ = decay;
    minReleaseSamples_ = release;
}

bool ADSR::IsDone(uint position) const {
    return releaseSamples_ + kReleaseTailSamples < position;
}


// GenerateValues dispatches on the gate. The engine has no early return for an
// empty block: the latching below still runs, and only the closing read of the
// last written sample is skipped here (the engine reads the halfword before
// the buffer).
void ADSR::GenerateValues(uint position, uint gate, short *output, uint numSamples) {
    if (gate == 0) {
        generateRelease(position, output, numSamples);
    } else {
        generateHeld(position, output, numSamples);
    }
}

void ADSR::generateRelease(uint position, short *output, uint numSamples) {
    if (idle_) {
        std::memset(output, 0, numSamples * sizeof(short));
        return;
    }

    if (noteHeld_) {
        // Note-off: latch the release length and the level to fall from.
        heldReleaseSamples_ = releaseSamples_;
        releaseStartLevel_ = lastOutput_;
        releaseStep_ = stepFor(heldReleaseSamples_);
    }
    noteHeld_ = false;

    const int level = releaseStartLevel_;
    uint remaining = numSamples;
    short *out = output;

    if (position < heldReleaseSamples_) {
        int ramp = kRampStart - releaseStep_ * static_cast<int>(position);
        uint left = heldReleaseSamples_ - position;
        switch (releaseCurve_) {
        case Curve::Linear:
            while (left != 0 && remaining != 0) {
                const int s = (static_cast<uint>(ramp) >> 9) & 0xFFFF;
                *out++ = static_cast<short>(floorAtZero(high15(mulLow(s, level))));
                ramp -= releaseStep_;
                --left;
                --remaining;
            }
            break;
        case Curve::Logarithmic:
            while (left != 0 && remaining != 0) {
                const int s = (static_cast<uint>(ramp) >> 10) & 0xFFFF;
                const int shaped = logShape(s) << 1;
                *out++ = static_cast<short>(floorAtZero(high15(mulLow(shaped, level))));
                ramp -= releaseStep_;
                --left;
                --remaining;
            }
            break;
        case Curve::Exponential:
            while (left != 0 && remaining != 0) {
                const int s = (static_cast<uint>(ramp) >> 9) & 0xFFFF;
                *out++ = static_cast<short>(floorAtZero(high15(mulLow(expShape(s), level))));
                ramp -= releaseStep_;
                --left;
                --remaining;
            }
            break;
        }
    }

    if (remaining == 0) {
        if (out != output) {
            lastOutput_ = out[-1];
        }
        return;
    }
    std::memset(out, 0, remaining * sizeof(short));
    lastOutput_ = 0;
    idle_ = true;
}

void ADSR::generateHeld(uint position, short *output, uint numSamples) {
    uint attackLength;
    uint decayLength;
    int start;
    int span;

    if (!startFromCurrentLevel_ && lastPosition_ > position) {
        // The position jumped backwards: a retrigger, always from zero.
        lastPosition_ = position;
        start = 0;
        span = 0x7FFF;
        attackLength = attackSamples_;
        decayLength = decaySamples_;
        heldAttackSamples_ = attackLength;
        heldDecaySamples_ = decayLength;
        previousLevel_ = lastOutput_;
        attackStep_ = stepFor(attackLength);
        decayStep_ = stepFor(decayLength);
        heldSustainQ15_ = sustainQ15_;
        attackStartLevel_ = 0;
        idle_ = false;
    } else {
        lastPosition_ = position;
        if (!noteHeld_) {
            // A new note. With the 0x61 byte set it rises from the last level.
            attackLength = attackSamples_;
            decayLength = decaySamples_;
            heldAttackSamples_ = attackLength;
            heldDecaySamples_ = decayLength;
            previousLevel_ = lastOutput_;
            if (startFromCurrentLevel_) {
                start = lastOutput_;
                span = static_cast<ushort>(0x7FFF - static_cast<ushort>(lastOutput_));
            } else {
                start = 0;
                span = 0x7FFF;
            }
            attackStep_ = stepFor(attackLength);
            decayStep_ = stepFor(decayLength);
            heldSustainQ15_ = sustainQ15_;
            attackStartLevel_ = static_cast<short>(start);
            idle_ = false;
        } else {
            start = attackStartLevel_;
            span = static_cast<ushort>(0x7FFF - static_cast<ushort>(attackStartLevel_));
            attackLength = heldAttackSamples_;
            decayLength = heldDecaySamples_;
        }
    }
    noteHeld_ = true;

    // Sustain fills and the decay's span use the current sustain (0x54); the
    // decay's floor uses the one latched at note-on (0x5C).
    const int sustain = static_cast<ushort>(sustainQ15_);
    const int heldSustain = heldSustainQ15_;
    start = static_cast<short>(start);

    uint remaining = numSamples;
    short *out = output;

    if (position <= attackLength) {
        const uint fadeLength = attackLength < 32 ? attackLength : 32;
        int ramp = attackStep_ * static_cast<int>(position);
        switch (attackCurve_) {
        case Curve::Logarithmic: {
            // The log attack fades from the level latched at note-on, every
            // block, and doubles the shaped value after offsetting it.
            const int previous = previousLevel_;
            lastOutput_ = previousLevel_;
            while (remaining != 0 && position < fadeLength && position != attackLength) {
                const int shapedLevel = high15(mulLow(logShape((static_cast<uint>(ramp) >> 10) & 0xFFFF), span)) + start;
                int shaped;
                if (shapedLevel >= 0x8000) {
                    shaped = -2;
                } else if (shapedLevel >= -0x7FFF) {
                    shaped = static_cast<short>(shapedLevel << 1);
                } else {
                    shaped = 2;
                }
                const int difference = clamp16(static_cast<short>(-previous) + shaped);
                *out++ = static_cast<short>(clamp16(previous + ((fadeWeight(position, fadeLength) * difference) >> 15)));
                ramp += attackStep_;
                ++position;
                --remaining;
            }
            while (remaining != 0 && position != attackLength) {
                const int shaped = logShape((static_cast<uint>(ramp) >> 10) & 0xFFFF) << 1;
                *out++ = static_cast<short>(clamp16(high15(mulLow(shaped, span)) + start));
                ramp += attackStep_;
                ++position;
                --remaining;
            }
            break;
        }
        case Curve::Linear: {
            // The linear and exponential attacks fade from whatever they wrote
            // last, sample by sample.
            int previous = static_cast<ushort>(lastOutput_);
            const auto shapedAt = [&](int value) {
                uint level = static_cast<uint>(value) >> 9;
                if (level >= 0x7FFF) {
                    level = 0x7FFF;
                }
                return start + ((static_cast<short>(span) * static_cast<int>(level)) >> 15);
            };
            if (remaining != 0 && position < fadeLength) {
                while (remaining != 0 && position < fadeLength && position != attackLength) {
                    previous = static_cast<ushort>(fadeFrom(previous, clamp16(shapedAt(ramp)), position, fadeLength));
                    *out++ = static_cast<short>(previous);
                    ramp += attackStep_;
                    ++position;
                    --remaining;
                }
                lastOutput_ = static_cast<short>(previous);
            }
            while (remaining != 0 && position != attackLength) {
                *out++ = static_cast<short>(clamp16(shapedAt(ramp)));
                ramp += attackStep_;
                ++position;
                --remaining;
            }
            break;
        }
        case Curve::Exponential: {
            int previous = static_cast<ushort>(lastOutput_);
            const auto shapedAt = [&](int value) {
                const int s = (static_cast<uint>(value) >> 9) & 0xFFFF;
                return high15(mulLow(expShape(s), span)) + start;
            };
            if (remaining != 0 && position < fadeLength) {
                while (remaining != 0 && position < fadeLength && position != attackLength) {
                    previous = static_cast<ushort>(fadeFrom(previous, clamp16(shapedAt(ramp)), position, fadeLength));
                    *out++ = static_cast<short>(previous);
                    ramp += attackStep_;
                    ++position;
                    --remaining;
                }
                lastOutput_ = static_cast<short>(previous);
            }
            while (remaining != 0 && position != attackLength) {
                *out++ = static_cast<short>(clamp16(shapedAt(ramp)));
                ramp += attackStep_;
                ++position;
                --remaining;
            }
            break;
        }
        }
    }

    const uint decayEnd = decayLength + attackLength;
    if (position <= decayEnd) {
        const int decaySpan = static_cast<ushort>(0x7FFF - sustain);
        int ramp = kRampStart - decayStep_ * static_cast<int>(position - attackLength);
        switch (decayCurve_) {
        case Curve::Linear:
            while (position != decayEnd && remaining != 0) {
                const int level = floorAtZero(static_cast<short>(ramp >> 9));
                *out++ = static_cast<short>(clamp16(heldSustain + (mulLow(level, decaySpan) >> 15)));
                ramp -= decayStep_;
                ++position;
                --remaining;
            }
            break;
        case Curve::Logarithmic:
            while (position != decayEnd && remaining != 0) {
                const int s = static_cast<ushort>(floorAtZero(static_cast<short>(ramp >> 10)));
                const int shaped = logShape(s) << 1;
                *out++ = static_cast<short>(clamp16(high15(mulLow(shaped, decaySpan)) + heldSustain));
                ramp -= decayStep_;
                ++position;
                --remaining;
            }
            break;
        case Curve::Exponential:
            while (position != decayEnd && remaining != 0) {
                const int s = floorAtZero(static_cast<short>(ramp >> 9));
                const int squared = mulLow(s, s) >> 15;
                *out++ = static_cast<short>(clamp16(heldSustain + ((static_cast<short>(decaySpan) * squared) >> 15)));
                ramp -= decayStep_;
                ++position;
                --remaining;
            }
            break;
        }
    }

    if (remaining == 0) {
        if (out != output) {
            lastOutput_ = out[-1];
        }
        return;
    }
    while (remaining-- != 0) {
        *out++ = static_cast<short>(sustain);
    }
    lastOutput_ = static_cast<short>(sustain);
}
