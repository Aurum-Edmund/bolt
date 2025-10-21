#include "import_resolver.hpp"

#include <filesystem>
#include <utility>

namespace bolt::hir
{
    void ImportResolver::setModuleLocator(const ModuleLocator* locator)
    {
        m_locator = locator;
    }

    ImportResolutionResult ImportResolver::resolve(const Module& module)
    {
        m_diagnostics.clear();

        ImportResolutionResult result;
        result.imports.reserve(module.imports.size());

        std::string canonicalModulePath = module.moduleName;
        if (!module.packageName.empty() && module.packageName != module.moduleName)
        {
            canonicalModulePath = module.packageName + "::" + module.moduleName;
        }

        for (const auto& importDecl : module.imports)
        {
            ImportResolution entry;
            entry.modulePath = importDecl.modulePath;

            if (importDecl.modulePath == module.moduleName
                || importDecl.modulePath == module.packageName
                || importDecl.modulePath == canonicalModulePath)
            {
                entry.status = ImportStatus::SelfImport;
                reportSelfImport(importDecl, module);
            }
            else if (m_locator != nullptr)
            {
                auto located = m_locator->locate(importDecl.modulePath);
                if (located.has_value())
                {
                    entry.status = ImportStatus::Resolved;
                    entry.canonicalModulePath = located->canonicalPath;
                    entry.resolvedFilePath = located->filePath.lexically_normal().string();
                }
                else
                {
                    entry.status = ImportStatus::NotFound;
                    reportMissingImport(importDecl);
                }
            }

            result.imports.emplace_back(std::move(entry));
        }

        return result;
    }

    const std::vector<Diagnostic>& ImportResolver::diagnostics() const noexcept
    {
        return m_diagnostics;
    }

    void ImportResolver::reportSelfImport(const Import& importDecl, const Module& module)
    {
        Diagnostic diag;
        diag.code = "BOLT-E2219";
        diag.message = "Module '" + module.moduleName + "' cannot import itself ('" + importDecl.modulePath + "').";
        diag.span = importDecl.span;
        m_diagnostics.emplace_back(std::move(diag));
    }

    void ImportResolver::reportMissingImport(const Import& importDecl)
    {
        Diagnostic diag;
        diag.code = "BOLT-E2220";
        diag.message = "Import '" + importDecl.modulePath + "' could not be resolved.";
        diag.span = importDecl.span;
        m_diagnostics.emplace_back(std::move(diag));
    }
} // namespace bolt::hir
