#pragma once

// Machine: the base of Caustic's audio processors. Ported to the original's
// structure. Every method is a weak
// inline in libcaustic.so (ARMv7), and the declaration order below is the
// vtable's: ~Machine (complete and deleting), OnParamModified, Process,
// ProcessStereo, GetInputIndex, GetInput, ResetMachine. SetInput is not
// virtual.
//
// Layout on the 32-bit build: the vtable pointer, then the input at +0x04.

class Machine {
public:
    Machine() {}
    virtual ~Machine() {}

    // 0x7abdc: does nothing.
    virtual void OnParamModified() {}

    // 0x7abe0, 0x7ac00: pass the buffer on to the input, if there is one.
    virtual void Process(int *pBuffer, unsigned int nSamples) {
        if (m_pInput != nullptr) {
            m_pInput->Process(pBuffer, nSamples);
        }
    }
    virtual void ProcessStereo(int *pBuffer, unsigned int nSamples) {
        if (m_pInput != nullptr) {
            m_pInput->ProcessStereo(pBuffer, nSamples);
        }
    }

    // 0x7ac20: always 0.
    virtual int GetInputIndex(Machine * /*pInput*/) { return 0; }
    // 0x7ac28.
    virtual Machine *GetInput(int /*nIndex*/) { return m_pInput; }
    // 0x7ac30: does nothing.
    virtual void ResetMachine() {}

    // 0x7ac34.
    void SetInput(Machine *pInput, int /*nIndex*/) { m_pInput = pInput; }

    Machine *m_pInput = nullptr; // +0x04
};
