#pragma once

// Reconstructed Caustic ADSR envelope generator.
//
// Recovered from ADSR::ADSR, ADSR::OnParamModified, ADSR::SetMinSamples,
// ADSR::IsDone and ADSR::GenerateValues in libcaustic.so (ARMv7). See
// src/dsp/ADSR.md for the evidence behind every constant and for the
// original member offsets this class replaces.
//
// The generator works in a 24-bit fixed-point ramp and emits Q15 samples.
// GenerateValues follows the original's structure, one loop per curve per
// segment, because the curves are not handled alike (see the notes).

#include "dsp/Envelope.h"

using uint = unsigned int;
using ushort = unsigned short;

// ADSR derives from Envelope, the two-entry vtable (IsDone, GenerateValues)
// that FixedPointSVFilter calls its envelope through.
class ADSR : public Envelope {
public:
    // Segment shape. The engine stores one of these per segment as a plain int.
    enum class Curve : int {
        Logarithmic = 0, // fast rise, long tail
        Linear = 1,      // default in the original constructor
        Exponential = 2, // slow rise, fast fall
    };

    // The engine hardcodes this rate; seconds are converted with it regardless
    // of the actual output rate.
    static constexpr float kSampleRate = 44100.0f;

    // Full-scale envelope output.
    static constexpr int kUnity = 0x7FFF;

    // A segment may never be longer than five seconds or shorter than its
    // configured minimum.
    static constexpr uint kMaxSegmentSamples = 220500;
    static constexpr uint kDefaultMinSegmentSamples = 0x50; // 80

    // IsDone reports completion this many samples after the release ends.
    static constexpr uint kReleaseTailSamples = 0x800; // 2048

    // Length of the click-suppressing fade applied at the start of an attack.
    static constexpr uint kRetriggerFadeSamples = 0x20; // 32

    ADSR();

    // Parameters, in seconds, and sustain as 0 .. 1. Call OnParamModified after
    // changing them, exactly as the engine does.
    void setAttackSeconds(float seconds) { attackSeconds_ = seconds; }
    void setDecaySeconds(float seconds) { decaySeconds_ = seconds; }
    void setSustainLevel(float level) { sustainLevel_ = level; }
    void setReleaseSeconds(float seconds) { releaseSeconds_ = seconds; }
    float attackSeconds() const { return attackSeconds_; }
    float decaySeconds() const { return decaySeconds_; }
    float sustainLevel() const { return sustainLevel_; }
    float releaseSeconds() const { return releaseSeconds_; }

    void setAttackCurve(Curve curve) { attackCurve_ = curve; }
    void setDecayCurve(Curve curve) { decayCurve_ = curve; }
    void setReleaseCurve(Curve curve) { releaseCurve_ = curve; }

    // Recompute segment lengths from the parameters.
    void OnParamModified();

    // Lower bounds for the attack, decay and release segments, in samples.
    void SetMinSamples(uint attack, uint decay, uint release);

    // Write numSamples envelope values.
    //
    // position is the sample offset since the last gate change. gate is
    // non-zero while the note is held; passing zero runs the release segment.
    void GenerateValues(uint position, uint gate, short *output, uint numSamples) override;

    // True once the release segment and its tail have elapsed.
    bool IsDone(uint position) override;

    // Resolved segment lengths, for tests and for drawing the envelope.
    uint attackSamples() const { return attackSamples_; }
    uint decaySamples() const { return decaySamples_; }
    uint releaseSamples() const { return releaseSamples_; }
    short sustainQ15() const { return sustainQ15_; }
    bool isIdle() const { return idle_; }

    // The byte at 0x61. When set, a note that starts while the envelope is not
    // held rises from the level it last output instead of from zero, and a
    // position that jumps backwards does not restart the note. The ADSR
    // constructor clears it; SubSynth sets it on its amplitude envelopes.
    void setStartFromCurrentLevel(bool enabled) { startFromCurrentLevel_ = enabled; }
    bool startsFromCurrentLevel() const { return startFromCurrentLevel_; }

private:
    void generateRelease(uint position, short *output, uint numSamples);
    void generateHeld(uint position, short *output, uint numSamples);

    // Parameters (offsets 0x04, 0x08, 0x0C, 0x10).
    float attackSeconds_ = 0.0f;
    float decaySeconds_ = 0.0f;
    float sustainLevel_ = 1.0f;
    float releaseSeconds_ = 0.0f;

    // Segment shapes (offsets 0x3C, 0x40, 0x44).
    Curve attackCurve_ = Curve::Linear;
    Curve decayCurve_ = Curve::Linear;
    Curve releaseCurve_ = Curve::Linear;

    // Resolved lengths (offsets 0x14, 0x18, 0x1C) and minima (0x2C, 0x30, 0x34).
    uint attackSamples_ = 0;
    uint decaySamples_ = 0;
    uint releaseSamples_ = 0;
    uint minAttackSamples_ = kDefaultMinSegmentSamples;
    uint minDecaySamples_ = kDefaultMinSegmentSamples;
    uint minReleaseSamples_ = kDefaultMinSegmentSamples;

    // Sustain as Q15 (offset 0x54).
    short sustainQ15_ = kUnity;

    // State latched when a note starts, so that editing a parameter mid-note
    // cannot change the segment currently being generated
    // (offsets 0x20, 0x24, 0x48, 0x4C, 0x58, 0x5C).
    uint heldAttackSamples_ = 0;
    uint heldDecaySamples_ = 0;
    int attackStep_ = 0;
    int decayStep_ = 0;
    short attackStartLevel_ = 0;
    short heldSustainQ15_ = kUnity;

    // State latched when the note is released (offsets 0x28, 0x50, 0x5E).
    uint heldReleaseSamples_ = 0;
    int releaseStep_ = 0;
    short releaseStartLevel_ = 0;

    // Running state (offsets 0x38, 0x56, 0x5A, 0x60, 0x62).
    uint lastPosition_ = 0;
    short lastOutput_ = 0;
    short previousLevel_ = 0;
    bool noteHeld_ = false;
    bool startFromCurrentLevel_ = false;
    bool idle_ = true;
};
