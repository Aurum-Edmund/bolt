#pragma once

#include "module.hpp"

#include <string>

namespace bolt::hir
{
    struct Diagnostic
    {
        std::string code;
        std::string message;
        SourceSpan span;
    };
} // namespace bolt::hir
