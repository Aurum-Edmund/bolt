#include "live_enforcement.hpp"

#include <iostream>

namespace bolt::mir
{
    bool enforceLive(Module& module)
    {
        (void)module;
        // TODO: Implement Live qualifier enforcement (SSA + ordering).
        return true;
    }
}
