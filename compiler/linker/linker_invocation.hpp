#pragma once

#include "cli_options.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace bolt
{
namespace linker
{
    struct LinkerInvocation
    {
        std::filesystem::path executable;
        std::vector<std::string> arguments;
    };

    struct LinkerPlanResult
    {
        LinkerInvocation invocation;
        bool hasError{false};
        std::string errorMessage;
    };

    LinkerPlanResult planLinkerInvocation(const CommandLineOptions& options);
}
} // namespace bolt

