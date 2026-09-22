#include "ControlVoltage.h"

ControlVoltage ControlVoltage::makeDefault() {
    ControlVoltage voltage{};

    // The constructor zeroes every integer and pointer except 0x38, zeroes
    // every float except the pair at 0x58, and clears the low three flag bits.
    voltage.flags = 0;
    voltage.field38 = 1;
    voltage.filterCutoffScale = 1.0f;
    voltage.field5C = 1.0f;

    return voltage;
}
