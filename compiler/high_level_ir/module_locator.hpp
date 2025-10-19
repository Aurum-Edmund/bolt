#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace bolt::hir
{
    struct ModuleLocatorResult
    {
        std::string canonicalPath;
        std::filesystem::path filePath;
    };

    class ModuleLocator
    {
    public:
        ModuleLocator() = default;

        void setSearchRoots(std::vector<std::filesystem::path> roots);
        void registerModule(std::string canonicalPath, std::filesystem::path filePath);

        [[nodiscard]] std::optional<ModuleLocatorResult> locate(std::string_view canonicalPath) const;

    private:
        [[nodiscard]] std::optional<ModuleLocatorResult> locateInRoots(std::string_view canonicalPath) const;

        std::vector<std::filesystem::path> m_searchRoots;
        std::unordered_map<std::string, std::filesystem::path> m_registeredModules;
    };
} // namespace bolt::hir

