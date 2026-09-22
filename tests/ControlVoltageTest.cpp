#include "components/ControlVoltage.h"

#include "components/StateVariableFilter.h"

#include <gtest/gtest.h>

// SubSynth's constructor leaves every integer and pointer at zero except the
// one at 0x38, every float at zero except the pair at 0x58, and the low flag
// bits clear.
TEST(ControlVoltageDefaults, MatchTheConstructorBlock) {
    const ControlVoltage voltage = ControlVoltage::makeDefault();

    EXPECT_EQ(voltage.flags, 0);
    EXPECT_EQ(voltage.field38, 1);
    EXPECT_FLOAT_EQ(voltage.filterCutoffScale, 1.0f);
    EXPECT_FLOAT_EQ(voltage.field5C, 1.0f);

    EXPECT_FLOAT_EQ(voltage.field04, 0.0f);
    EXPECT_FLOAT_EQ(voltage.pitch, 0.0f);
    EXPECT_FLOAT_EQ(voltage.field50, 0.0f);
    EXPECT_EQ(voltage.modulation, nullptr);
    EXPECT_EQ(voltage.field30, nullptr);
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

    EXPECT_EQ(filter.getCutoffCoefficient(), filter.getCutoffTableEntry(0));
}

// The layout mirror is what pins the recovered offsets; the assertions live in
// the header, so reaching them here is enough.
TEST(ControlVoltageLayout, EngineBlockIsNinetySixBytes) {
    EXPECT_EQ(sizeof(controlVoltageLayout::Engine), ControlVoltage::kSize);
    EXPECT_EQ(ControlVoltage::kSize, 0x60u);
}
