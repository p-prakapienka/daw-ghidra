#include "components/ControlVoltage.h"

#include "components/StateVariableFilter.h"

#include <gtest/gtest.h>

TEST(ControlVoltageDefaults, MatchTheConstructorBlock) {
    const ControlVoltage voltage = ControlVoltage::makeDefault();

    EXPECT_EQ(voltage.flags, 0);
    EXPECT_EQ(voltage.field38, 1);
    EXPECT_FLOAT_EQ(voltage.filterCutoffScale, 1.0f);
    EXPECT_FLOAT_EQ(voltage.field5C, 1.0f);

    EXPECT_EQ(voltage.samplesSinceNoteOn, 0);
    EXPECT_EQ(voltage.envelopePosition, 0);
    EXPECT_EQ(voltage.releasePosition, 0);
    EXPECT_FLOAT_EQ(voltage.currentPitch, 0.0f);
    EXPECT_FLOAT_EQ(voltage.pitchSweepDecay, 0.0f);
    EXPECT_EQ(voltage.glideSamples, 0u);
}

TEST(ControlVoltageDefaults, LeaveTheFilterCutoffUntouched) {
    const ControlVoltage voltage = ControlVoltage::makeDefault();

    StateVariableFilter withVoltage(true);
    withVoltage.setCutoffWithVoltage(0.5f, voltage);

    StateVariableFilter plain(true);
    plain.setCutoff(0.5f);

    EXPECT_EQ(withVoltage.getCutoffCoefficient(), plain.getCutoffCoefficient());
}

TEST(ControlVoltageScaling, ClosesTheFilterAsTheScaleFalls) {
    ControlVoltage voltage = ControlVoltage::makeDefault();

    StateVariableFilter open(true);
    open.setCutoffWithVoltage(0.8f, voltage);

    voltage.filterCutoffScale = 0.25f;
    StateVariableFilter closed(true);
    closed.setCutoffWithVoltage(0.8f, voltage);

    EXPECT_LT(closed.getCutoffCoefficient(), open.getCutoffCoefficient());
}

TEST(ControlVoltageScaling, AZeroScaleCollapsesToTheCutoffFloor) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.filterCutoffScale = 0.0f;

    StateVariableFilter filter(true);
    filter.setCutoffWithVoltage(1.0f, voltage);

    EXPECT_EQ(filter.getCutoffCoefficient(),
              filter.getCutoffTableEntry(StateVariableFilter::kMinimumCutoffIndex));
}

TEST(ControlVoltageLayout, EngineBlockIsNinetySixBytes) {
    EXPECT_EQ(sizeof(controlVoltageLayout::Engine), ControlVoltage::kSize);
    EXPECT_EQ(ControlVoltage::kSize, 0x60u);
}

TEST(ControlVoltageNoteOn, WritesPitchInQ12HertzToAllThreeFields) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(440.0f, 69, 0.0f, 1.0f);

    const float expected = 440.0f * ControlVoltage::kPitchScale;
    EXPECT_FLOAT_EQ(voltage.currentPitch, expected);
    EXPECT_FLOAT_EQ(voltage.glideStartPitch, expected);
    EXPECT_FLOAT_EQ(voltage.targetPitch, expected);
    EXPECT_EQ(voltage.frequencyQ12, static_cast<std::uint32_t>(expected));
    EXPECT_FLOAT_EQ(voltage.frequencyHertz, 440.0f);
    EXPECT_EQ(voltage.noteId, 69);
    EXPECT_EQ(voltage.glideSamples, 0u);
}

TEST(ControlVoltageNoteOn, SetsTheTwoLowFlagBitsAndClearsRuntimeState) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.samplesSinceNoteOn = 5;
    voltage.field40 = 5.0f;
    voltage.noteOn(220.0f, 57, 0.0f, 1.0f);

    EXPECT_TRUE(voltage.isActive());
    EXPECT_TRUE(voltage.isGateOpen());
    EXPECT_EQ(voltage.samplesSinceNoteOn, 0);
    EXPECT_FLOAT_EQ(voltage.field40, 0.0f);
    EXPECT_EQ(voltage.releasePosition, 0);
}

TEST(ControlVoltageTracking, PivotFrequencyLeavesTheCutoffAlone) {
    EXPECT_FLOAT_EQ(ControlVoltage::trackingScale(500.0f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(ControlVoltage::trackingScale(500.0f, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ControlVoltage::trackingScale(2000.0f, 0.0f), 1.0f);
}

TEST(ControlVoltageTracking, FollowsASquareRootOfTheFrequencyRatio) {
    EXPECT_FLOAT_EQ(ControlVoltage::trackingScale(2000.0f, 1.0f), 2.0f);
    EXPECT_FLOAT_EQ(ControlVoltage::trackingScale(2000.0f, 0.5f), 1.5f);
    EXPECT_LT(ControlVoltage::trackingScale(125.0f, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ControlVoltage::trackingScale(125.0f, 1.0f), 0.5f);
}

TEST(ControlVoltageTracking, ReachesTheFilterThroughNoteOn) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(2000.0f, 96, 1.0f, 1.0f);

    StateVariableFilter tracked(true);
    tracked.setCutoffWithVoltage(0.3f, voltage);

    StateVariableFilter fixed(true);
    fixed.setCutoff(0.6f);

    EXPECT_EQ(tracked.getCutoffCoefficient(), fixed.getCutoffCoefficient());
}

TEST(ControlVoltageGlide, StartsFromTheCurrentPitchAndTargetsTheNewNote) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(220.0f, 57, 0.0f, 1.0f);
    const float before = voltage.currentPitch;

    voltage.beginGlide(440.0f, 2205);

    EXPECT_FLOAT_EQ(voltage.currentPitch, before);
    EXPECT_FLOAT_EQ(voltage.glideStartPitch, before);
    EXPECT_FLOAT_EQ(voltage.targetPitch, 440.0f * ControlVoltage::kPitchScale);
    EXPECT_EQ(voltage.glideSamples, 2205u);
}

TEST(ControlVoltageCounters, ReleaseOnlyCountsWhileTheGateIsClosed) {
    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(440.0f, 69, 0.0f, 1.0f);

    voltage.advance(64);
    EXPECT_EQ(voltage.samplesSinceNoteOn, 64);
    EXPECT_EQ(voltage.envelopePosition, 64);
    EXPECT_EQ(voltage.releasePosition, 0);
    EXPECT_EQ(voltage.envelopePositionForGate(), 64);

    voltage.noteOff();
    voltage.advance(64);
    EXPECT_FALSE(voltage.isGateOpen());
    EXPECT_EQ(voltage.envelopePosition, 128);
    EXPECT_EQ(voltage.releasePosition, 64);
    EXPECT_EQ(voltage.envelopePositionForGate(), 64);
}
