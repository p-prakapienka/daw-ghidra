#pragma once

// Filter: the common base of FixedPointSVFilter and FloatSVFilter. It adds
// nothing to Machine but its own destructor (0x946f8, 0x94728).

#include "Machine.h"

class Filter : public Machine {
public:
    virtual ~Filter() {}
};
