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

    struct ModuleLocatorDuplicate
    {
        std::string canonicalPath;
        std::filesystem::path existingPath;
        std::filesystem::path duplicatePath;
    };

    struct ModuleLocatorIssue
    {
        std::filesystem::path path;
        std::string message;
    };

    struct ModuleLocatorDiscoveryResult
    {
        std::vector<ModuleLocatorResult> discoveredModules;
        std::vector<ModuleLocatorDuplicate> duplicates;
        std::vector<ModuleLocatorIssue> issues;
    };

    class ModuleLocator
    {
    public:
        ModuleLocator() = default;

        void setSearchRoots(std::vector<std::filesystem::path> roots);
        void registerModule(std::string canonicalPath, std::filesystem::path filePath);
        [[nodiscard]] ModuleLocatorDiscoveryResult discoverModules();

        [[nodiscard]] std::optional<ModuleLocatorResult> locate(std::string_view canonicalPath) const;

    private:
        [[nodiscard]] std::optional<ModuleLocatorResult> locateInRoots(std::string_view canonicalPath) const;

        void registerCanonical(std::string canonicalPath, const std::filesystem::path& filePath);
        void registerDottedAlias(const std::string& canonicalPath);

        std::vector<std::filesystem::path> m_searchRoots;
        std::unordered_map<std::string, std::filesystem::path> m_registeredModules;
        std::unordered_map<std::string, std::string> m_aliases;
    };
} // namespace bolt::hir

