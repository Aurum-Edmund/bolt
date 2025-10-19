#include "verifier.hpp"

namespace bolt::mir
{
    bool verify(const Module& module)
    {
        for (const auto& function : module.functions)
        {
            if (function.blocks.empty())
            {
                return false;
            }
        }
        return true;
    }
} // namespace bolt::mir
