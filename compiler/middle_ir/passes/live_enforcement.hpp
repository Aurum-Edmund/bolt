#pragma once

#include "../module.hpp"

#include <string>
#include <vector>

namespace bolt::mir
{
    struct LiveDiagnostic
    {
        std::string code;
        std::string functionName;
        std::string detail;
    };

    /**
     * Enforce Live qualifiers after MIR lowering.
     * Returns true when the module satisfies Live guarantees.
     * Any diagnostics encountered are appended to the provided vector.
     */
    bool enforceLive(Module& module, std::vector<LiveDiagnostic>& diagnostics);
}
