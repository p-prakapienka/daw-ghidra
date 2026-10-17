#include "dsp/ControlVoltage.h"

#include <gtest/gtest.h>


TEST(ControlVoltageDefaults, MatchTheConstructorBlock) {
    const ControlVoltage voltage = ControlVoltage{};

    EXPECT_EQ(voltage.m_nFlags, 0);
    EXPECT_EQ(voltage.m_n38, 1);
    EXPECT_FLOAT_EQ(voltage.m_fFilterCutoffScale, 1.0f);
    EXPECT_FLOAT_EQ(voltage.m_fVelocity, 1.0f);

    EXPECT_EQ(voltage.m_nSamplesSinceNoteOn, 0);
    EXPECT_EQ(voltage.m_nEnvelopePosition, 0);
    EXPECT_EQ(voltage.m_nReleasePosition, 0);
    EXPECT_FLOAT_EQ(voltage.m_fCurrentPitch, 0.0f);
    EXPECT_FLOAT_EQ(voltage.m_fPitchSweepDecay, 0.0f);
    EXPECT_EQ(voltage.m_nGlideSamples, 0u);
}

TEST(ControlVoltageLayout, IsTheEnginesBlockOnEveryHost) {
    // No pointers: 0x60 bytes, the per-voice stride in SubSynth, on 32- and
    // 64-bit builds alike. The header pins every field's offset.
    EXPECT_EQ(sizeof(ControlVoltage), 0x60u);
}
