#pragma once

// Envelope: the base of ADSR, AREnvelope and DecayEnvelope. Ported to the
// original's structure. Its vtable holds
// exactly two entries, IsDone and GenerateValues, and no destructor. On its
// own it is the "no envelope" a filter starts with: always done (0x7e4fc),
// and GenerateValues writes nothing (0x7e504).

class Envelope {
public:
    virtual bool IsDone(unsigned int /*nPosition*/) { return true; }
    virtual void GenerateValues(unsigned int /*nPosition*/, unsigned int /*nGate*/,
                                short * /*pOutput*/, unsigned int /*nSamples*/) {}
};
