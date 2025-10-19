#pragma once

#include "../module.hpp"

namespace bolt::mir
{
    /**
     * Enforce Live qualifiers after MIR lowering.
     * Returns true when the module satisfies Live guarantees.
     */
    bool enforceLive(Module& module);
}
