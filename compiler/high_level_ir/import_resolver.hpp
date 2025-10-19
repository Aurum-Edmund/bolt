#pragma once

#include "diagnostic.hpp"
#include "module.hpp"
#include "module_locator.hpp"

#include <string>
#include <vector>
#include <optional>

namespace bolt::hir
{
    enum class ImportStatus
    {
        Pending,
        Resolved,
        NotFound,
        SelfImport
    };

    struct ImportResolution
    {
        std::string modulePath;
        ImportStatus status{ImportStatus::Pending};
        std::optional<std::string> resolvedFilePath;
    };

    struct ImportResolutionResult
    {
        std::vector<ImportResolution> imports;
    };

    class ImportResolver
    {
    public:
        ImportResolver() = default;

        void setModuleLocator(const ModuleLocator* locator);

        ImportResolutionResult resolve(const Module& module);
        [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept;

    private:
        void reportSelfImport(const Import& importDecl, const Module& module);
        void reportMissingImport(const Import& importDecl);

        std::vector<Diagnostic> m_diagnostics;
        const ModuleLocator* m_locator{nullptr};
    };
} // namespace bolt::hir
