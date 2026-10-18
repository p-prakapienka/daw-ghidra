#pragma once

// Filter: the common base of FixedPointSVFilter and FloatSVFilter. Ported to
// the original's structure. It adds
// nothing to Machine but its own destructor (0x946f8, 0x94728).

#include "core/Machine.h"

class Filter : public Machine {
public:
    virtual ~Filter() {}
};
