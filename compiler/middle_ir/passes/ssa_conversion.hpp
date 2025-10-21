#pragma once

#include "../module.hpp"

#include <string>
#include <vector>

namespace bolt::mir::passes
{
    struct SsaDiagnostic
    {
        std::string code;
        std::string functionName;
        std::string detail;
    };

    /**
     * Convert a MIR function to static single assignment form. Returns true when conversion succeeds.
     * Diagnostics are appended to the provided vector when issues are encountered during renaming.
     */
    bool convertToSsa(Function& function, std::vector<SsaDiagnostic>& diagnostics);

    /**
     * Convert all functions within the module to SSA form. Returns true when every function succeeds.
     */
    bool convertToSsa(Module& module, std::vector<SsaDiagnostic>& diagnostics);
}

