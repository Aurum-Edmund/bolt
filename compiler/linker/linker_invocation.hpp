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

    struct LinkerValidationResult
    {
        bool hasError{false};
        std::string errorMessage;
    };

    LinkerPlanResult planLinkerInvocation(const CommandLineOptions& options);

    LinkerValidationResult validateLinkerInputs(const CommandLineOptions& options, bool skipInputObjectValidation);
}
} // namespace bolt

