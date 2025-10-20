#pragma once

#include "../high_level_ir/import_resolver.hpp"
#include "../high_level_ir/module.hpp"

#include <filesystem>
#include <string>

namespace bolt
{
    bool writeImportBundle(const std::filesystem::path& outputPath,
        const hir::Module& module,
        const hir::ImportResolutionResult& resolution,
        std::string& errorMessage);
}

