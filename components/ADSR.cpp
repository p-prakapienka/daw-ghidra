#include "ADSR.h"

#include <cstring>

ADSR::ADSR() {
    // *(undefined4 *)(this + 4) = 0;
    // *(undefined4 *)(this + 8) = 0;
    // *(undefined4 *)(this + 0x10) = 0;
    // *(undefined ***)this = &PTR_IsDone_00314198;
    // *(undefined4 *)(this + 0xc) = 0x3f800000;
    // this[0x60] = (ADSR)0x0;
    bool0x60 = false;
    // *(undefined2 *)(this + 0x56) = 0;
    someShort0x56 = 0;
    // *(undefined4 *)(this + 0x50) = 0;
    someInt0x50 = 0;
    // *(undefined2 *)(this + 0x5e) = 0;
    // this[0x61] = (ADSR)0x0;
    bool0x61 = false;
    // *(undefined4 *)(this + 0x38) = 0;
    someUint0x38 = 0;
    // *(undefined4 *)(this + 0x48) = 0;
    someInt0x48 = 0;
    // *(undefined4 *)(this + 0x4c) = 0;
    someInt0x4c = 0;
    // *(undefined2 *)(this + 0x58) = 0;
    someShort0x58 = 0;
    // *(undefined2 *)(this + 0x5a) = 0;
    // *(undefined2 *)(this + 0x5c) = 0;
    someShort0x5c = 0;
    isCompleted0x62 = true;
    // *(undefined4 *)(this + 0x44) = 1;
    envStage0x44 = 1;
    // *(undefined4 *)(this + 0x40) = 1;
    someInt0x40 = 1;
    // *(undefined4 *)(this + 0x3c) = 1;
    // *(undefined4 *)(this + 0x2c) = 0x50;
    someFloat0x2c = 0x50;
    // *(undefined4 *)(this + 0x30) = 0x50;
    someFloat0x30 = 0x50;
    // *(undefined4 *)(this + 0x34) = 0x50;
    someFloat0x34 = 0x50;
    OnParamModified();
}

ADSR::~ADSR() = default;

// just to make code a bit more readable
const int BIT24_MAX = 16777216;
const int BIT24_MAX_REDUCED = 16776704;

void ADSR::GenerateValues(uint playbackPosParam1, uint param_2, short *outputBuffer_param_3, uint numSamples_param_4) {
    short sVar1;
    short sVar2;
    ushort uVar3;
    int envLvl24bitiVar4;
    uint uVar5;
    uint uVar6;
    uint uVar7;
    ushort outputUVar8;
    int stepInciVar9;
    uint samplesToProcessuVar10;
    int iVar11;
    ushort *outArrayElemPointerpuVar12;
    ushort unaff_r10;
    int envLvliVar13;
    uint uVar14LengthSamples;
    ushort uVar15;
    bool bVar16;
    bool bVar17;
    int local_60;
    uint local_5c;
    int local_50;
    uint local_4c;
    ushort local_3c;

    if (param_2 == 0) {
        if (isCompleted0x62 /* this[0x62] != (ADSR)0x0 */) {
            //std::memset(param_3,0,param_4 << 1);
            std::memset(outputBuffer_param_3, 0, numSamples_param_4 * 2);
            return;
        }
        if (!bool0x60 /* this[0x60] == (ADSR)0x0 */) {
            //iVar9 = *(int *)(this + 0x50);
            stepInciVar9 = someInt0x50;
            //sVar2 = *(short *)(this + 0x5e);
            sVar2 = someShort0x5e;
            //uVar14 = *(uint *)(this + 0x28);
            uVar14LengthSamples = someUint0x28;
        } else {
            //uVar14 = *(uint *)(this + 0x1c);
            uVar14LengthSamples = someUint0x1c;
            // sVar2 = *(short *)(this + 0x56);
            sVar2 = someShort0x56;
            // *(uint *)(this + 0x28) = uVar14LengthSamples;
            someUint0x28 = uVar14LengthSamples;
            // *(short *)(this + 0x5e) = sVar2;
            someShort0x5e = sVar2;
            // 16777216.0 - 24-bit processing max value
            // iVar9StepInc = (int)(16777216.0 / (float)uVar14LengthSamples);
            stepInciVar9 = static_cast<int>(BIT24_MAX / uVar14LengthSamples);
            //*(int *)(this + 0x50) = iVar9;
            someInt0x50 = stepInciVar9;
        }
        // this[0x60] = (ADSR)0x0;
        bool0x60 = false;
        samplesToProcessuVar10 = numSamples_param_4;
        if (playbackPosParam1 < uVar14LengthSamples) {
            // iVar13 = *(int *)(this + 0x44);
            // actually assigned to be used instead of envStage0x44
            // envLvliVar13 = envStage0x44;

            // compiler reused iVar13 for multiple purposes
            // here it can safely be replaced with a member var
            //if (envLvliVar13 == 1) {
            if (envStage0x44 == 1) {
                // Gemini thinks it's a release phase
                if (uVar14LengthSamples != playbackPosParam1 && numSamples_param_4 != 0) {
                    // 0xfffe00 is close to 24bit maximum, possibly to prevent overflow
                    // envelope stage start level
                    envLvliVar13 = BIT24_MAX_REDUCED /* 0xfffe00 */ - stepInciVar9 * playbackPosParam1;
                    outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    do {
                        // iVar13 << 7 (an internal 24-bit accumulator).
                        // Shifting it right by 16 bits extracts the most significant bits,
                        // resulting in a value that represents the "percentage"
                        // of the envelope (from 0 to 32767)
                        // multiply by 2^7 = 128
                        //iVar4 = envLvliVar13 << 7;
                        //intermediate variable remove, likely inserted by compiler
                        //envLvl24bitiVar4 = envLvliVar13 * 128;
                        samplesToProcessuVar10 = samplesToProcessuVar10 - 1;
                        envLvliVar13 = envLvliVar13 - stepInciVar9;
                        //outputUVar8 = (ushort)((uint)((int)(short)((uint)iVar4 >> 0x10) * (int)sVar2 * 2) >> 0x10);
                        outputUVar8 = envLvliVar13 * 128 / 65536 * (sVar2 * 2) / 65536;

                        //just moving pointers to the next array element
                        outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                        *outArrayElemPointerpuVar12 = outputUVar8 & ~((short)outputUVar8 >> 0xf);
                        outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    } while (samplesToProcessuVar10 - numSamples_param_4 + (uVar14LengthSamples - playbackPosParam1) != 0 && samplesToProcessuVar10 != 0);
                }
            //} else if (envLvliVar13 == 0) {
            } else if (envStage0x44 == 0) {
                if (uVar14LengthSamples != playbackPosParam1 && numSamples_param_4 != 0) {
                    envLvliVar13 = 16776704 /* 0xfffe00 */ - stepInciVar9 * playbackPosParam1;
                    outArrayElemPointerpuVar12 = (ushort *) outputBuffer_param_3;
                    do {
                        samplesToProcessuVar10 = samplesToProcessuVar10 - 1;
                        sVar1 = (short)((uint)(envLvliVar13 << 6) >> 0x10);
                        envLvliVar13 = envLvliVar13 - stepInciVar9;
                        envLvl24bitiVar4 = sVar1 + 0x1a3d;
                        if (0x7ffe < envLvl24bitiVar4) {
                          envLvl24bitiVar4 = 0x7fff;
                        }
                        // sVar1 = __divsi3((sVar1 * 0x5998 >> 0xf) << 0xf,iVar4);
                        // Gemini: In C/C++ source code, this is simply the / operator (e.g., a / b).
                        // Gemini: 0x5998 is 22936 in decimal
                        // Gemini: >> 0xf is a bitwise shift right by 15. Since 2^{15} = 32768,
                        // Gemini: shifting right by 15 is equivalent to dividing by 32768.
                        sVar1 = (sVar1 * 22936 / 32768) / envLvl24bitiVar4;
                        outputUVar8 = (ushort)((uint)((int)(short)(sVar1 << 1) * (int)sVar2 * 2) >> 0x10);
                        outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                        *outArrayElemPointerpuVar12 = outputUVar8 & ~((short)outputUVar8 >> 0xf);
                        outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    } while ((samplesToProcessuVar10 - numSamples_param_4) + (uVar14LengthSamples - playbackPosParam1) != 0 && samplesToProcessuVar10 != 0);
                }
            //} else if (envLvliVar13 == 2 && uVar14LengthSamples != playbackPosParam1 && numSamples_param_4 != 0) {
            } else if (envStage0x44 == 2 && uVar14LengthSamples != playbackPosParam1 && numSamples_param_4 != 0) {
                envLvliVar13 = 0xfffe00 - stepInciVar9 * playbackPosParam1;
                outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                do {
                    envLvl24bitiVar4 = envLvliVar13 << 7;
                    samplesToProcessuVar10 = samplesToProcessuVar10 - 1;
                    envLvliVar13 = envLvliVar13 - stepInciVar9;
                    sVar1 = (short)((uint)envLvl24bitiVar4 >> 0x10);
                    outputUVar8 = (ushort)((uint)((int)(short)((uint)((int)sVar1 * (int)sVar1 * 2) >> 0x10) *
                                            (int)sVar2 * 2) >> 0x10);
                    outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                    *outArrayElemPointerpuVar12 = outputUVar8 & ~((short)outputUVar8 >> 0xf);
                    outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                } while (samplesToProcessuVar10 - numSamples_param_4 + (uVar14LengthSamples - playbackPosParam1) != 0 && samplesToProcessuVar10 != 0);
            }
        }
        // when there are still samples to fill, fill them with zeros
        if (samplesToProcessuVar10 != 0) {
            do {
                samplesToProcessuVar10 = samplesToProcessuVar10 - 1;
                *outputBuffer_param_3 = 0;
                outputBuffer_param_3 = (short *)((ushort *)outputBuffer_param_3 + 1);
            } while (samplesToProcessuVar10 != 0);
            // *(undefined2 *)(this + 0x56) = 0;
            someShort0x56 = 0;
            // this[0x62] = (ADSR)0x1;
            isCompleted0x62 = true;
            return;
        }
    } else {
        // uVar8 = (ushort)(byte)this[0x61];
        outputUVar8 = (ushort) bool0x61;
        if ((outputUVar8 == 0) && (playbackPosParam1 < someUint0x38 /* *(uint *)(this + 0x38) */)) {
            // local_5c = *(uint *)(this + 0x14);
            local_5c = someUint0x14;
            // local_4c = *(uint *)(this + 0x18);
            local_4c = someUint0x18;
            // *(uint *)(this + 0x38) = param_1;
            someUint0x38 = playbackPosParam1;
            // *(uint *)(this + 0x20) = local_5c;
            someUint0x20 = local_5c;
            // *(undefined2 *)(this + 0x5a) = *(undefined2 *)(this + 0x56);
            someShort0x5a = someShort0x56;
            // *(uint *)(this + 0x24) = local_4c;
            someUint0x24 = local_4c;
            unaff_r10 = 0x7fff;

            LAB_0008fce8:

            // local_3c = *(ushort *)(this + 0x54);
            local_3c = someShort0x54;
            // *(ushort *)(this + 0x58) = uVar8;
            someShort0x58 = outputUVar8;
            // this[0x62] = (ADSR)0x0;
            isCompleted0x62 = false;
            // *(ushort *)(this + 0x5c) = local_3c;
            someShort0x5c = local_3c;
            // 16777216.0 - 24-bit processing max value
            local_60 = (int)(16777216.0 / (float)local_5c);
            local_50 = (int)(16777216.0 / (float)local_4c);
            // *(int *)(this + 0x48) = local_60;
            someInt0x48 = local_60;
            // *(int *)(this + 0x4c) = local_50;
            someInt0x4c = local_50;
            uVar15 = local_3c;
        } else {
            // *(uint *)(this + 0x38) = param_1;
            someUint0x38 = playbackPosParam1;
            if (!bool0x60 /* this[0x60] == (ADSR)0x0 */) {
                // local_4c = *(uint *)(this + 0x18);
                local_4c = someUint0x18;
                bVar16 = outputUVar8 != 0;
                // local_5c = *(uint *)(this + 0x14);
                local_5c = someUint0x14;
                // uVar15 = *(ushort *)(this + 0x56);
                uVar15 = someShort0x56;
                if (bVar16) {
                    unaff_r10 = 0x7f00 - uVar15;
                }
                // *(uint *)(this + 0x20) = local_5c;
                someUint0x20 = local_5c;
                if (bVar16) {
                    unaff_r10 = unaff_r10 + 0xff;
                }
                // *(ushort *)(this + 0x5a) = uVar15;
                someShort0x5a = uVar15;
                uVar3 = uVar15;
                if (!bVar16) {
                    uVar15 = 0x7fff;
                    uVar3 = outputUVar8;
                }
                outputUVar8 = uVar3;
                // *(uint *)(this + 0x24) = local_4c;
                someUint0x24 = local_4c;
                if (!bVar16) {
                    unaff_r10 = uVar15;
                }
                goto LAB_0008fce8;
            }
            // uVar8 = *(ushort *)(this + 0x58);
            outputUVar8 = someShort0x58;
            // local_60 = *(int *)(this + 0x48);
            local_60 = someInt0x48;
            unaff_r10 = 0x7fff - outputUVar8;
            // local_50 = *(int *)(this + 0x4c);
            local_50 = someInt0x4c;
            // local_3c = *(ushort *)(this + 0x5c);
            local_3c = someShort0x5c;
            // local_5c = *(uint *)(this + 0x20);
            local_5c = someUint0x20;
            // local_4c = *(uint *)(this + 0x24);
            local_4c = someUint0x24;
            // uVar15 = *(ushort *)(this + 0x54);
            uVar15 = someShort0x54;
        }
        // this[0x60] = (ADSR)0x1;
        bool0x60 = true;
        uVar14LengthSamples = numSamples_param_4;

        if (playbackPosParam1 <= local_5c) {
            // iVar9StepInc = *(int *)(this + 0x3c);
            stepInciVar9 = someInt0x3c;
            if (stepInciVar9 == 1) {
                samplesToProcessuVar10 = local_5c;
                if (0x1f < local_5c) {
                    samplesToProcessuVar10 = 0x20;
                }
                bVar16 = numSamples_param_4 != 0;
                stepInciVar9 = local_5c - playbackPosParam1;
                if (numSamples_param_4 != 0 && playbackPosParam1 < samplesToProcessuVar10) {
                    if (stepInciVar9 == 0) goto LAB_0008fa50;
                    uVar6 = local_60 * playbackPosParam1;
                    // uVar5 = (uint)*(ushort *)(this + 0x56);
                    uVar5 = someShort0x56;
                    outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    do {
                        uVar7 = uVar6 >> 9;
                        if (0x7ffe < uVar7) {
                          uVar7 = 0x7fff;
                        }
                        envLvliVar13 = (int)(short)outputUVar8 + ((int)((int)(short)unaff_r10 * uVar7) >> 0xf);
                        if (envLvliVar13 < 0x8000) {
                            if (envLvliVar13 < -0x7fff) {
                                envLvliVar13 = -0x7fff;
                            }
                        } else {
                            envLvliVar13 = 0x7fff;
                        }
                        envLvliVar13 = envLvliVar13 + (short)-(short)uVar5;
                        outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                        envLvl24bitiVar4 = envLvliVar13;
                        if (0x7fff < envLvliVar13) {
                            envLvl24bitiVar4 = 0x7fff;
                        }
                        if ((envLvliVar13 < 0x8000) && (envLvl24bitiVar4 < -0x7fff)) {
                            envLvl24bitiVar4 = -0x7fff;
                        }
                        uVar5 = ((short)(int)(((float)(longlong)(int)playbackPosParam1 / (float)(longlong)(int)samplesToProcessuVar10) *
                                 32767.0) * envLvl24bitiVar4 >> 0xf) + (int)(short)uVar5;
                        if ((int)uVar5 < 0x8000) {
                            if ((int)uVar5 < -0x7fff) {
                                uVar5 = 0x8001;
                            } else {
                                uVar5 = uVar5 & 0xffff;
                            }
                        } else {
                            uVar5 = 0x7fff;
                        }
                        uVar14LengthSamples = uVar14LengthSamples - 1;
                        bVar17 = uVar14LengthSamples != 0;
                        playbackPosParam1 = playbackPosParam1 + 1;
                        uVar3 = (ushort)uVar5;
                        *outArrayElemPointerpuVar12 = uVar3;
                        bVar16 = playbackPosParam1 < samplesToProcessuVar10 && bVar17;
                        if (playbackPosParam1 >= samplesToProcessuVar10 || !bVar17) {
                            // *(ushort *)(this + 0x56) = uVar3;
                            someShort0x56 = uVar3;
                            stepInciVar9 = local_5c - playbackPosParam1;
                            numSamples_param_4 = uVar14LengthSamples;
                            bVar16 = bVar17;
                            goto LAB_0008ff84;
                        }
                        uVar6 = uVar6 + local_60;
                        outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    } while (uVar14LengthSamples - numSamples_param_4 + stepInciVar9 != 0);
                    // *(ushort *)(this + 0x56) = uVar3;
                    someShort0x56 = uVar3;
                    stepInciVar9 = local_5c - playbackPosParam1;
                    numSamples_param_4 = uVar14LengthSamples;
                }

                LAB_0008ff84:

                if (stepInciVar9 == 0) {
                    bVar16 = false;
                }
                uVar14LengthSamples = numSamples_param_4;
                if (bVar16) {
                    samplesToProcessuVar10 = local_60 * playbackPosParam1;
                    outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    do {
                        uVar6 = samplesToProcessuVar10 >> 9;
                        uVar3 = 0x7fff;
                        if (0x7ffe < uVar6) {
                           uVar6 = 0x7fff;
                        }
                        outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                        envLvliVar13 = (int)(short)outputUVar8 + ((int)((int)(short)unaff_r10 * uVar6) >> 0xf);
                        if ((envLvliVar13 < 0x8000) && (uVar3 = 0x8001, -0x8000 < envLvliVar13)) {
                            uVar3 = (ushort)envLvliVar13;
                        }
                        uVar14LengthSamples = uVar14LengthSamples - 1;
                        *outArrayElemPointerpuVar12 = uVar3;
                        playbackPosParam1 = playbackPosParam1 + 1;
                        samplesToProcessuVar10 = samplesToProcessuVar10 + local_60;
                        outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    } while (stepInciVar9 + uVar14LengthSamples != numSamples_param_4 && uVar14LengthSamples != 0);
                }
            } else if (stepInciVar9 == 0) {
                samplesToProcessuVar10 = local_5c;
                if (0x1f < local_5c) {
                  samplesToProcessuVar10 = 0x20;
                }
                bVar16 = numSamples_param_4 != 0;
                // sVar2 = *(short *)(this + 0x5a);
                sVar2 = someShort0x5a;
                // *(short *)(this + 0x56) = sVar2;
                someShort0x56 = sVar2;
                stepInciVar9 = local_5c - playbackPosParam1;
                if (numSamples_param_4 != 0 && playbackPosParam1 < samplesToProcessuVar10) {
                    if (stepInciVar9 == 0) goto LAB_0008fa50;
                    envLvliVar13 = local_60 * playbackPosParam1;
                    outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    do {
                        sVar1 = (short)((uint)(envLvliVar13 << 6) >> 0x10);
                        envLvl24bitiVar4 = sVar1 + 0x1a3d;
                        if (0x7ffe < envLvl24bitiVar4) {
                          envLvl24bitiVar4 = 0x7fff;
                        }
                        // sVar1 = __divsi3((sVar1 * 0x5998 >> 0xf) << 0xf,iVar4);
                        sVar1 = (sVar1 * 22936 / 32768) / envLvl24bitiVar4;
                        envLvl24bitiVar4 = ((int)sVar1 * (int)(short)unaff_r10 * 2 >> 0x10) + (int)(short)outputUVar8;
                        if (envLvl24bitiVar4 < 0x8000) {
                            if (envLvl24bitiVar4 < -0x7fff) {
                                envLvl24bitiVar4 = 2;
                            } else {
                                envLvl24bitiVar4 = (int)(short)((short)envLvl24bitiVar4 * 2);
                            }
                        } else {
                            envLvl24bitiVar4 = -2;
                        }
                        outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                        envLvl24bitiVar4 = -sVar2 + envLvl24bitiVar4;
                        iVar11 = envLvl24bitiVar4;
                        if (0x7fff < envLvl24bitiVar4) {
                            iVar11 = 0x7fff;
                        }
                        if ((envLvl24bitiVar4 < 0x8000) && (iVar11 < -0x7fff)) {
                            iVar11 = -0x7fff;
                        }
                        envLvl24bitiVar4 = (int)sVar2 +
                        ((short)(int)(((float)(longlong)(int)playbackPosParam1 / (float)(longlong)(int)samplesToProcessuVar10) *
                                 32767.0) * iVar11 >> 0xf);
                        if (envLvl24bitiVar4 < 0x8000) {
                            if (envLvl24bitiVar4 < -0x7fff) {
                                uVar3 = 0x8001;
                            } else {
                                uVar3 = (ushort)envLvl24bitiVar4;
                            }
                        } else {
                            uVar3 = 0x7fff;
                        }
                        uVar14LengthSamples = uVar14LengthSamples - 1;
                        bVar17 = uVar14LengthSamples != 0;
                        playbackPosParam1 = playbackPosParam1 + 1;
                        *outArrayElemPointerpuVar12 = uVar3;
                        bVar16 = playbackPosParam1 < samplesToProcessuVar10 && bVar17;
                        if (playbackPosParam1 >= samplesToProcessuVar10 || !bVar17) {
                            stepInciVar9 = local_5c - playbackPosParam1;
                            numSamples_param_4 = uVar14LengthSamples;
                            bVar16 = bVar17;
                            goto LAB_0008f97c;
                        }
                        envLvliVar13 = envLvliVar13 + local_60;
                        outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    } while ((uVar14LengthSamples - numSamples_param_4) + stepInciVar9 != 0);
                    stepInciVar9 = local_5c - playbackPosParam1;
                    numSamples_param_4 = uVar14LengthSamples;
                }

                LAB_0008f97c:

                if (stepInciVar9 == 0) {
                    bVar16 = false;
                }
                uVar14LengthSamples = numSamples_param_4;
                if (bVar16) {
                    envLvliVar13 = local_60 * playbackPosParam1;
                    outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    do {
                        outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                        sVar2 = (short)((uint)(envLvliVar13 << 6) >> 0x10);
                        envLvl24bitiVar4 = sVar2 + 0x1a3d;
                        if (0x7ffe < envLvl24bitiVar4) {
                            envLvl24bitiVar4 = 0x7fff;
                        }
                        sVar2 = (sVar2 * 22936 / 32768) / envLvl24bitiVar4;
                        uVar3 = 0x7fff;
                        envLvl24bitiVar4 = ((int)(short)(sVar2 << 1) * (int)(short)unaff_r10 * 2 >> 0x10) +
                                (int)(short)outputUVar8;
                        if ((envLvl24bitiVar4 < 0x8000) && (uVar3 = 0x8001, -0x8000 < envLvl24bitiVar4)) {
                            uVar3 = (ushort)envLvl24bitiVar4;
                        }
                        uVar14LengthSamples = uVar14LengthSamples - 1;
                        *outArrayElemPointerpuVar12 = uVar3;
                        playbackPosParam1 = playbackPosParam1 + 1;
                        envLvliVar13 = envLvliVar13 + local_60;
                        outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    } while (stepInciVar9 + uVar14LengthSamples != numSamples_param_4 && uVar14LengthSamples != 0);
                }
            } else if (stepInciVar9 == 2) {
                samplesToProcessuVar10 = local_5c;
                if (0x1f < local_5c) {
                    samplesToProcessuVar10 = 0x20;
                }
                bVar16 = numSamples_param_4 != 0;
                stepInciVar9 = local_5c - playbackPosParam1;
                if (numSamples_param_4 != 0 && playbackPosParam1 < samplesToProcessuVar10) {
                    if (stepInciVar9 == 0) goto LAB_0008fa50;

                    // uVar6 = (uint)*(ushort *)(this + 0x56);
                    uVar6 = someShort0x56;
                    envLvliVar13 = local_60 * playbackPosParam1;
                    outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    do {
                        sVar2 = (short)((uint)(envLvliVar13 << 7) >> 0x10);
                        envLvl24bitiVar4 = ((int)(short)((uint)((int)sVar2 * (int)sVar2 * 2) >> 0x10) *
                                 (int)(short)unaff_r10 * 2 >> 0x10) + (int)(short)outputUVar8;
                        if (envLvl24bitiVar4 < 0x8000) {
                            if (envLvl24bitiVar4 < -0x7fff) {
                                envLvl24bitiVar4 = -0x7fff;
                            }
                        } else {
                            envLvl24bitiVar4 = 0x7fff;
                        }
                        envLvl24bitiVar4 = envLvl24bitiVar4 + (short)-(short)uVar6;
                        outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                        iVar11 = envLvl24bitiVar4;
                        if (0x7fff < envLvl24bitiVar4) {
                            iVar11 = 0x7fff;
                        }
                        if ((envLvl24bitiVar4 < 0x8000) && (iVar11 < -0x7fff)) {
                            iVar11 = -0x7fff;
                        }
                        uVar6 = ((short)(int)(((float)(longlong)(int)playbackPosParam1 / (float)(longlong)(int)samplesToProcessuVar10) *
                             32767.0) * iVar11 >> 0xf) + (int)(short)uVar6;
                        if ((int)uVar6 < 0x8000) {
                            if ((int)uVar6 < -0x7fff) {
                                uVar6 = 0x8001;
                            } else {
                                uVar6 = uVar6 & 0xffff;
                            }
                        } else {
                            uVar6 = 0x7fff;
                        }
                        uVar14LengthSamples = uVar14LengthSamples - 1;
                        bVar17 = uVar14LengthSamples != 0;
                        playbackPosParam1 = playbackPosParam1 + 1;
                        uVar3 = (ushort)uVar6;
                        *outArrayElemPointerpuVar12 = uVar3;
                        bVar16 = playbackPosParam1 < samplesToProcessuVar10 && bVar17;
                        if (playbackPosParam1 >= samplesToProcessuVar10 || !bVar17) {
                            // *(ushort *)(this + 0x56) = uVar3;
                            someShort0x56 = uVar3;
                            stepInciVar9 = local_5c - playbackPosParam1;
                            numSamples_param_4 = uVar14LengthSamples;
                            bVar16 = bVar17;
                            goto LAB_000901e4;
                        }
                        envLvliVar13 = envLvliVar13 + local_60;
                        outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    } while ((uVar14LengthSamples - numSamples_param_4) + stepInciVar9 != 0);
                    // *(ushort *)(this + 0x56) = uVar3;
                    someShort0x56 = uVar3;
                    stepInciVar9 = local_5c - playbackPosParam1;
                    numSamples_param_4 = uVar14LengthSamples;
                }
                LAB_000901e4:
                if (stepInciVar9 == 0) {
                    bVar16 = false;
                }
                uVar14LengthSamples = numSamples_param_4;
                if (bVar16) {
                    envLvliVar13 = local_60 * playbackPosParam1;
                    outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    do {
                        uVar3 = 0x7fff;
                        outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                        sVar2 = (short)((uint)(envLvliVar13 << 7) >> 0x10);
                        envLvl24bitiVar4 = ((int)(short)((uint)((int)sVar2 * (int)sVar2 * 2) >> 0x10) *
                                 (int)(short)unaff_r10 * 2 >> 0x10) + (int)(short)outputUVar8;
                        if ((envLvl24bitiVar4 < 0x8000) && (uVar3 = 0x8001, -0x8000 < envLvl24bitiVar4)) {
                            uVar3 = (ushort)envLvl24bitiVar4;
                        }
                        uVar14LengthSamples = uVar14LengthSamples - 1;
                        *outArrayElemPointerpuVar12 = uVar3;
                        playbackPosParam1 = playbackPosParam1 + 1;
                        envLvliVar13 = envLvliVar13 + local_60;
                        outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    } while (stepInciVar9 + uVar14LengthSamples != numSamples_param_4 && uVar14LengthSamples != 0);
                }
            }
        }
        LAB_0008fa50:
        local_4c = local_4c + local_5c;
        if (playbackPosParam1 <= local_4c) {
            // iVar9StepInc = *(int *)(this + 0x40);
            stepInciVar9 = someInt0x40;
            sVar2 = 0x7fff - uVar15;
            if (stepInciVar9 == 1) {
                if (local_4c != playbackPosParam1 && uVar14LengthSamples != 0) {
                    stepInciVar9 = (local_4c - playbackPosParam1) - uVar14LengthSamples;
                    envLvliVar13 = 0xfffe00 - local_50 * (playbackPosParam1 - local_5c);
                    outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    do {
                        uVar3 = 0x7fff;
                        outputUVar8 = (ushort)((uint)(envLvliVar13 << 7) >> 0x10);
                        outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                        envLvl24bitiVar4 = (int)(short)local_3c +
                                ((int)(short)(outputUVar8 & ~((short)outputUVar8 >> 0xf)) * (int)sVar2 >> 0xf);
                        if ((envLvl24bitiVar4 < 0x8000) && (uVar3 = 0x8001, -0x8000 < envLvl24bitiVar4)) {
                            uVar3 = (ushort)envLvl24bitiVar4;
                        }
                        uVar14LengthSamples = uVar14LengthSamples - 1;
                        *outArrayElemPointerpuVar12 = uVar3;
                        envLvliVar13 = envLvliVar13 - local_50;
                        outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    } while (stepInciVar9 + uVar14LengthSamples != 0 && uVar14LengthSamples != 0);
                }
            } else if (stepInciVar9 == 0) {
                if (local_4c != playbackPosParam1 && uVar14LengthSamples != 0) {
                    stepInciVar9 = (local_4c - playbackPosParam1) - uVar14LengthSamples;
                    envLvliVar13 = 0xfffe00 - local_50 * (playbackPosParam1 - local_5c);
                    outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    do {
                        outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                        outputUVar8 = (ushort)((uint)(envLvliVar13 << 6) >> 0x10);
                        outputUVar8 = outputUVar8 & ~((short)outputUVar8 >> 0xf);
                        envLvl24bitiVar4 = (short)outputUVar8 + 0x1a3d;
                        if (0x7ffe < envLvl24bitiVar4) {
                          envLvl24bitiVar4 = 0x7fff;
                        }
                        // sVar1 = __divsi3(((short)uVar8 * 0x5998 >> 0xf) << 0xf,iVar4);
                        sVar1 = outputUVar8 * 22936 / 32768 / envLvl24bitiVar4;
                        outputUVar8 = 0x7fff;
                        envLvl24bitiVar4 = ((int)(short)(sVar1 << 1) * (int)sVar2 * 2 >> 0x10) + (int)(short)local_3c;
                        if ((envLvl24bitiVar4 < 0x8000) && (outputUVar8 = 0x8001, -0x8000 < envLvl24bitiVar4)) {
                            outputUVar8 = (ushort)envLvl24bitiVar4;
                        }
                        uVar14LengthSamples = uVar14LengthSamples - 1;
                        *outArrayElemPointerpuVar12 = outputUVar8;
                        envLvliVar13 = envLvliVar13 - local_50;
                        outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                    } while (stepInciVar9 + uVar14LengthSamples != 0 && uVar14LengthSamples != 0);
                }
            } else if ((stepInciVar9 == 2) && (local_4c != playbackPosParam1 && uVar14LengthSamples != 0)) {
                envLvliVar13 = (local_4c - playbackPosParam1) - uVar14LengthSamples;
                stepInciVar9 = 0xfffe00 - local_50 * (playbackPosParam1 - local_5c);
                outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                do {
                    uVar3 = 0x7fff;
                    outputUVar8 = (ushort)((uint)(stepInciVar9 << 7) >> 0x10);
                    outputUVar8 = outputUVar8 & ~((short)outputUVar8 >> 0xf);
                    outputBuffer_param_3 = (short *)(outArrayElemPointerpuVar12 + 1);
                    envLvl24bitiVar4 = (int)(short)local_3c +
                          ((int)sVar2 * ((int)(short)outputUVar8 * (int)(short)outputUVar8 >> 0xf) >> 0xf);
                    if ((envLvl24bitiVar4 < 0x8000) && (uVar3 = 0x8001, -0x8000 < envLvl24bitiVar4)) {
                        uVar3 = (ushort)envLvl24bitiVar4;
                    }
                    uVar14LengthSamples = uVar14LengthSamples - 1;
                    *outArrayElemPointerpuVar12 = uVar3;
                    stepInciVar9 = stepInciVar9 - local_50;
                    outArrayElemPointerpuVar12 = (ushort *)outputBuffer_param_3;
                } while (envLvliVar13 + uVar14LengthSamples != 0 && uVar14LengthSamples != 0);
            }
        }
        if (uVar14LengthSamples != 0) {
            do {
                uVar14LengthSamples = uVar14LengthSamples - 1;
                *outputBuffer_param_3 = uVar15;
                outputBuffer_param_3 = (short *)((ushort *)outputBuffer_param_3 + 1);
            } while (uVar14LengthSamples != 0);
            goto LAB_0008fb90;
        }
    }
    uVar15 = ((ushort *)outputBuffer_param_3)[-1];
    LAB_0008fb90:
    *(ushort *)(this + 0x56) = uVar15;
}

bool ADSR::IsDone(uint param_1) {
    return someUint0x1c /* *(int *)(this + 0x1c) */ + 0x800U < param_1;
}

void ADSR::OnParamModified() {
    float pcVar1;
    float pcVar2;
  
    pcVar1 = someFloat0x2c; // *(char **)(this + 0x2c);
    pcVar2 = someFloat0x4 /* *(float *)(this + 4) */ * 44100.0f;
    someUint0x14 /* *(char **)(this + 0x14) */ = pcVar2;
    // *(short *)(this + 0x54) = (short)(int)(someFloat0xc * 32767.0);
    someShort0x54 = static_cast<short>(someFloat0xc * 32767.0f);
    if (pcVar2 < pcVar1 || (pcVar1 = "tSaIS1_EE", "tSaIS1_EE" < pcVar2)) {
        /* *(char **)(this + 0x14) */ someUint0x14 = pcVar1;
    }
    pcVar1 = someFloat0x30;// *(char **)(this + 0x30);
    // pcVar2 = (char *)(int)(*(float *)(this + 8) * 44100.0);
    pcVar2 = someFloat0x8 * 44100.0f;
    /* *(char **)(this + 0x18) */ someUint0x18 = pcVar2;
    if (pcVar2 < pcVar1 || (pcVar1 = "tSaIS1_EE", "tSaIS1_EE" < pcVar2)) {
        /* *(char **)(this + 0x18) */ someUint0x18 = pcVar1;
    }
    pcVar1 = someFloat0x34; // *(char **)(this + 0x34);
    // pcVar2 = (char *)(int)(*(float *)(this + 0x10) * 44100.0);
    pcVar2 = someFloat0x10 * 44100.0f;
    /* *(char **)(this + 0x1c) */ someUint0x1c = pcVar2;
    if (pcVar1 <= pcVar2 && (pcVar1 = "tSaIS1_EE", pcVar2 < "SaIS1_EE")) {
        return;
    }
    /* *(char **)(this + 0x1c) */ someUint0x1c = pcVar1;
}

void ADSR::SetMinSamples(uint param_1, uint param_2, uint param_3)
{
    someFloat0x2c = param_1; //TODO change to uint
    someFloat0x30 = param_2;
    someFloat0x34 = param_3;
}