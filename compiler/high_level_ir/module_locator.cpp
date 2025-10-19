#include "module_locator.hpp"

#include <algorithm>

namespace bolt::hir
{
    namespace
    {
        std::filesystem::path modulePathToRelativePath(std::string_view canonicalPath)
        {
            std::string normalized;
            normalized.reserve(canonicalPath.size());
            for (char ch : canonicalPath)
            {
                if (ch == ':' || ch == '.')
                {
                    // Collapse both namespace separators to directory separators.
                    normalized.push_back(std::filesystem::path::preferred_separator);
                }
                else
                {
                    normalized.push_back(ch);
                }
            }
            return std::filesystem::path{normalized}.replace_extension(".bolt");
        }
    } // namespace

    void ModuleLocator::setSearchRoots(std::vector<std::filesystem::path> roots)
    {
        m_searchRoots = std::move(roots);
        // Normalise roots by removing trailing separators.
        for (auto& root : m_searchRoots)
        {
            root = root.lexically_normal();
        }
    }

    void ModuleLocator::registerModule(std::string canonicalPath, std::filesystem::path filePath)
    {
        m_registeredModules.emplace(std::move(canonicalPath), std::move(filePath));
    }

    std::optional<ModuleLocatorResult> ModuleLocator::locate(std::string_view canonicalPath) const
    {
        if (canonicalPath.empty())
        {
            return std::nullopt;
        }

        auto registered = m_registeredModules.find(std::string{canonicalPath});
        if (registered != m_registeredModules.end())
        {
            ModuleLocatorResult result;
            result.canonicalPath = registered->first;
            result.filePath = registered->second;
            return result;
        }

        return locateInRoots(canonicalPath);
    }

    std::optional<ModuleLocatorResult> ModuleLocator::locateInRoots(std::string_view canonicalPath) const
    {
        if (m_searchRoots.empty())
        {
            return std::nullopt;
        }

        const std::filesystem::path relative = modulePathToRelativePath(canonicalPath);

        for (const auto& root : m_searchRoots)
        {
            std::filesystem::path candidate = root / relative;
            std::error_code error;
            if (std::filesystem::exists(candidate, error) && !error)
            {
                ModuleLocatorResult result;
                result.canonicalPath = std::string{canonicalPath};
                result.filePath = std::move(candidate);
                return result;
            }
        }

        return std::nullopt;
    }
} // namespace bolt::hir
