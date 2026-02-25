#pragma once
#include <string>

// Minimal ADSR class declaration as requested
// Uses 'uint' alias for clarity per the issue description

using uint = unsigned int;
using ushort = unsigned short;

class ADSR {
public:
    // *(undefined4 *)(this + 4) = 0;
    float someFloat0x4 = 0.0f; //attack seconds? multiplier of sample rate
    // *(undefined4 *)(this + 8) = 0;
    float someFloat0x8 = 0.0f; //decay seconds? multiplier of sample rate
    // *(undefined4 *)(this + 0xc) = 0x3f800000;
    float someFloat0xc = 1.0f; //sustain level?
    // *(undefined4 *)(this + 0x10) = 0;
    float someFloat0x10 = 0.0f; //release seconds? multiplier of sample rate

    // *(undefined ***)this = &PTR_IsDone_00314198;
    uint someUint0x14;
    uint someUint0x18;
    uint someUint0x1c = 0;
    uint someUint0x20;
    uint someUint0x24;
    // this[0x60] = (ADSR)0x0;
    bool bool0x60 = false;
    short someShort0x54 = 0;
    // *(undefined2 *)(this + 0x56) = 0;
    short someShort0x56 = 0;
    // *(undefined4 *)(this + 0x50) = 0;
    int someInt0x50 = 0;
    // *(undefined2 *)(this + 0x5e) = 0;
    short someShort0x5e = 0;
    // this[0x61] = (ADSR)0x0;
    bool bool0x61 = false;
    // *(undefined4 *)(this + 0x38) = 0;
    uint someUint0x38 = 0;
    // *(undefined4 *)(this + 0x48) = 0;
    int someInt0x48 = 0;
    // *(undefined4 *)(this + 0x4c) = 0;
    int someInt0x4c = 0;
    // *(undefined2 *)(this + 0x58) = 0;
    ushort someShort0x58 = 0;
    // *(undefined2 *)(this + 0x5a) = 0;
    short someShort0x5a = 0;
    // *(undefined2 *)(this + 0x5c) = 0;
    ushort someShort0x5c = 0;
    // this[0x62] = (ADSR)0x1;
    bool isCompleted0x62 = true;
    // *(undefined4 *)(this + 0x44) = 1;
    int envStage0x44 = 1;
    // *(undefined4 *)(this + 0x40) = 1;
    int someInt0x40 = 1;
    // *(undefined4 *)(this + 0x3c) = 1;
    int someInt0x3c = 1;
    uint someUint0x28;
    // *(undefined4 *)(this + 0x2c) = 0x50;
    float someFloat0x2c = 0x50; //attack length samples?
    // *(undefined4 *)(this + 0x30) = 0x50;
    float someFloat0x30 = 0x50; //decay length samples?
    // *(undefined4 *)(this + 0x34) = 0x50;
    float someFloat0x34 = 0x50; //release length samples?

    ADSR();

    ~ADSR();

    void GenerateValues(uint playbackPosParam1, uint param_2, short *outputBuffer_param_3, uint numSamples_param_4);

    bool IsDone(uint param_1);

    void OnParamModified();

    void SetMinSamples(uint param_1, uint param_2, uint param_3);
};