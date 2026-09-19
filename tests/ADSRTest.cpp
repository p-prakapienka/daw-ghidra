#include "components/ADSR.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

constexpr float kAttackSeconds = 0.05f;
constexpr float kDecaySeconds = 0.10f;
constexpr float kReleaseSeconds = 0.10f;
constexpr float kSustain = 0.5f;

ADSR makeEnvelope(ADSR::Curve curve) {
    ADSR envelope;
    envelope.setAttackSeconds(kAttackSeconds);
    envelope.setDecaySeconds(kDecaySeconds);
    envelope.setSustainLevel(kSustain);
    envelope.setReleaseSeconds(kReleaseSeconds);
    envelope.setAttackCurve(curve);
    envelope.setDecayCurve(curve);
    envelope.setReleaseCurve(curve);
    envelope.OnParamModified();
    return envelope;
}

std::vector<short> hold(ADSR &envelope, unsigned int numSamples) {
    std::vector<short> values(numSamples);
    envelope.GenerateValues(0, 1, values.data(), numSamples);
    return values;
}

std::vector<short> release(ADSR &envelope, unsigned int numSamples) {
    std::vector<short> values(numSamples);
    envelope.GenerateValues(0, 0, values.data(), numSamples);
    return values;
}

} // namespace

// Seconds are converted with the rate the engine hardcodes, not the host rate.
TEST(ADSRParameters, ConvertsSecondsAtTheHardcodedRate) {
    const ADSR envelope = makeEnvelope(ADSR::Curve::Linear);

    EXPECT_EQ(envelope.attackSamples(), 2205u);
    EXPECT_EQ(envelope.decaySamples(), 4410u);
    EXPECT_EQ(envelope.releaseSamples(), 4410u);
    EXPECT_EQ(envelope.sustainQ15(), 16383);
}

TEST(ADSRParameters, ClampsSegmentsToTheEngineRange) {
    ADSR envelope;
    envelope.setAttackSeconds(0.0f);
    envelope.setDecaySeconds(600.0f);
    envelope.setReleaseSeconds(-1.0f);
    envelope.OnParamModified();

    EXPECT_EQ(envelope.attackSamples(), ADSR::kDefaultMinSegmentSamples);
    EXPECT_EQ(envelope.decaySamples(), ADSR::kMaxSegmentSamples);

    // A negative time is not clamped to the minimum: the engine converts to a
    // signed int, stores it in an unsigned field, and the wrapped value trips
    // the upper bound instead.
    EXPECT_EQ(envelope.releaseSamples(), ADSR::kMaxSegmentSamples);
}

TEST(ADSRParameters, HonoursConfiguredMinimums) {
    ADSR envelope;
    envelope.SetMinSamples(1000, 2000, 3000);
    envelope.OnParamModified();

    EXPECT_EQ(envelope.attackSamples(), 1000u);
    EXPECT_EQ(envelope.decaySamples(), 2000u);
    EXPECT_EQ(envelope.releaseSamples(), 3000u);
}

// The measured shape of each segment. These are the values the reconstruction
// produces today, so a change to the fixed-point maths has to be deliberate.
TEST(ADSRShape, LinearRisesAndFallsEvenly) {
    ADSR envelope = makeEnvelope(ADSR::Curve::Linear);
    const auto attack = envelope.attackSamples();
    const auto decay = envelope.decaySamples();
    const auto values = hold(envelope, attack + decay + 2000);

    EXPECT_EQ(values[0], 0);
    EXPECT_EQ(values[attack / 2], 16374);
    EXPECT_EQ(values[attack - 1], 32749);
    EXPECT_EQ(values[attack + decay / 2], 24575);
    EXPECT_EQ(values[attack + decay], envelope.sustainQ15());
    EXPECT_EQ(values.back(), envelope.sustainQ15());
}

TEST(ADSRShape, ExponentialRisesSlowly) {
    ADSR envelope = makeEnvelope(ADSR::Curve::Exponential);
    const auto attack = envelope.attackSamples();
    const auto values = hold(envelope, attack + envelope.decaySamples());

    // A squared ramp is at a quarter of full scale halfway through.
    EXPECT_EQ(values[attack / 2], 8182);
    EXPECT_EQ(values[attack - 1], 32731);
}

TEST(ADSRShape, LogarithmicRisesQuickly) {
    ADSR envelope = makeEnvelope(ADSR::Curve::Logarithmic);
    const auto attack = envelope.attackSamples();
    const auto values = hold(envelope, attack + envelope.decaySamples());

    EXPECT_EQ(values[attack / 2], 25195);

    // The hyperbola stops a little short of full scale by construction.
    EXPECT_EQ(values[attack - 1], 32525);
}

TEST(ADSRRelease, FallsFromTheLevelHeldAtNoteOff) {
    ADSR envelope = makeEnvelope(ADSR::Curve::Linear);
    const auto attack = envelope.attackSamples();
    const auto decay = envelope.decaySamples();
    hold(envelope, attack + decay + 1000);

    const auto releaseLength = envelope.releaseSamples();
    const auto values = release(envelope, releaseLength + 1000);

    EXPECT_EQ(values[0], 16382);
    EXPECT_EQ(values[releaseLength / 2], 8191);
    EXPECT_EQ(values[releaseLength + 10], 0);
    EXPECT_TRUE(envelope.isIdle());
}

TEST(ADSRRelease, ReportsDoneAfterTheTail) {
    const ADSR envelope = makeEnvelope(ADSR::Curve::Linear);
    const auto releaseLength = envelope.releaseSamples();

    EXPECT_FALSE(envelope.IsDone(releaseLength));
    EXPECT_FALSE(envelope.IsDone(releaseLength + ADSR::kReleaseTailSamples));
    EXPECT_TRUE(envelope.IsDone(releaseLength + ADSR::kReleaseTailSamples + 1));
}

TEST(ADSRRelease, IdleEnvelopeWritesSilence) {
    ADSR envelope = makeEnvelope(ADSR::Curve::Linear);
    const auto values = release(envelope, 64);

    for (const short value : values) {
        EXPECT_EQ(value, 0);
    }
}

// Retriggering mid-release has to start from the level already sounding,
// otherwise the jump to zero clicks.
TEST(ADSRRetrigger, FadesInFromTheLevelStillSounding) {
    ADSR envelope = makeEnvelope(ADSR::Curve::Linear);
    hold(envelope, envelope.attackSamples() + envelope.decaySamples() + 1000);
    const auto releasing = release(envelope, 500);

    const auto values = hold(envelope, ADSR::kRetriggerFadeSamples);

    EXPECT_EQ(values[0], releasing.back());
    EXPECT_LT(values[1], values[0]);
    EXPECT_GT(values[0], 0);
}
