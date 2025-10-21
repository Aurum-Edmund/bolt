#include "module_locator.hpp"

#include <algorithm>
#include <optional>
#include <utility>

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

        std::optional<std::string> relativePathToCanonicalPath(const std::filesystem::path& relativePath)
        {
            if (relativePath.empty())
            {
                return std::nullopt;
            }

            std::filesystem::path normalized = relativePath.lexically_normal();
            if (normalized.has_root_directory() || normalized.has_root_name())
            {
                return std::nullopt;
            }

            if (normalized.extension() != ".bolt")
            {
                return std::nullopt;
            }

            normalized.replace_extension();

            std::vector<std::string> parts;
            parts.reserve(4);
            for (const auto& part : normalized)
            {
                auto partString = part.string();
                if (partString.empty() || partString == "." || partString == "..")
                {
                    return std::nullopt;
                }
                parts.emplace_back(std::move(partString));
            }

            if (parts.empty())
            {
                return std::nullopt;
            }

            std::string canonical;
            canonical.reserve(normalized.string().size());
            bool first = true;
            for (const auto& part : parts)
            {
                if (!first)
                {
                    canonical.append("::");
                }
                first = false;
                canonical.append(part);
            }

            return canonical;
        }

        std::string canonicalToDotted(std::string_view canonicalPath)
        {
            std::string dotted;
            dotted.reserve(canonicalPath.size());

            for (std::size_t index = 0; index < canonicalPath.size(); ++index)
            {
                if (canonicalPath[index] == ':' && index + 1 < canonicalPath.size()
                    && canonicalPath[index + 1] == ':')
                {
                    dotted.push_back('.');
                    ++index;
                }
                else
                {
                    dotted.push_back(canonicalPath[index]);
                }
            }

            return dotted;
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
        if (canonicalPath.empty() || filePath.empty())
        {
            return;
        }

        std::filesystem::path normalisedPath = filePath.lexically_normal();
        registerCanonical(std::move(canonicalPath), normalisedPath);
    }

    ModuleLocatorDiscoveryResult ModuleLocator::discoverModules()
    {
        ModuleLocatorDiscoveryResult result;

        for (const auto& root : m_searchRoots)
        {
            if (root.empty())
            {
                continue;
            }

            std::error_code statusError;
            if (!std::filesystem::exists(root, statusError) || statusError)
            {
                ModuleLocatorIssue issue;
                issue.path = root;
                issue.message = "import root does not exist";
                result.issues.emplace_back(std::move(issue));
                continue;
            }

            if (!std::filesystem::is_directory(root, statusError) || statusError)
            {
                ModuleLocatorIssue issue;
                issue.path = root;
                issue.message = "import root is not a directory";
                result.issues.emplace_back(std::move(issue));
                continue;
            }

            std::error_code iteratorError;
            std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, iteratorError);
            if (iteratorError)
            {
                ModuleLocatorIssue issue;
                issue.path = root;
                issue.message = "failed to enumerate import root";
                result.issues.emplace_back(std::move(issue));
                continue;
            }

            std::filesystem::recursive_directory_iterator end;
            while (it != end)
            {
                const auto& entry = *it;

                std::error_code entryError;
                if (!entry.is_regular_file(entryError) || entryError)
                {
                    std::error_code incrementError;
                    it.increment(incrementError);
                    if (incrementError)
                    {
                        ModuleLocatorIssue issue;
                        issue.path = entry.path();
                        issue.message = "failed to enumerate import root";
                        result.issues.emplace_back(std::move(issue));
                        break;
                    }
                    continue;
                }

                if (entry.path().extension() != ".bolt")
                {
                    std::error_code incrementError;
                    it.increment(incrementError);
                    if (incrementError)
                    {
                        ModuleLocatorIssue issue;
                        issue.path = entry.path();
                        issue.message = "failed to enumerate import root";
                        result.issues.emplace_back(std::move(issue));
                        break;
                    }
                    continue;
                }

                std::filesystem::path relativePath = std::filesystem::relative(entry.path(), root, entryError);
                if (entryError)
                {
                    ModuleLocatorIssue issue;
                    issue.path = entry.path();
                    issue.message = "failed to compute module path relative to import root";
                    result.issues.emplace_back(std::move(issue));

                    std::error_code incrementError;
                    it.increment(incrementError);
                    if (incrementError)
                    {
                        ModuleLocatorIssue iteratorIssue;
                        iteratorIssue.path = entry.path();
                        iteratorIssue.message = "failed to enumerate import root";
                        result.issues.emplace_back(std::move(iteratorIssue));
                        break;
                    }
                    continue;
                }

                auto canonical = relativePathToCanonicalPath(relativePath);
                if (!canonical.has_value())
                {
                    ModuleLocatorIssue issue;
                    issue.path = entry.path();
                    issue.message = "could not derive canonical module path";
                    result.issues.emplace_back(std::move(issue));

                    std::error_code incrementError;
                    it.increment(incrementError);
                    if (incrementError)
                    {
                        ModuleLocatorIssue iteratorIssue;
                        iteratorIssue.path = entry.path();
                        iteratorIssue.message = "failed to enumerate import root";
                        result.issues.emplace_back(std::move(iteratorIssue));
                        break;
                    }
                    continue;
                }

                std::filesystem::path normalisedPath = entry.path().lexically_normal();
                auto existing = m_registeredModules.find(*canonical);
                if (existing != m_registeredModules.end())
                {
                    std::filesystem::path existingNormalised = existing->second.lexically_normal();
                    if (existingNormalised != normalisedPath)
                    {
                        ModuleLocatorDuplicate duplicate;
                        duplicate.canonicalPath = *canonical;
                        duplicate.existingPath = existing->second;
                        duplicate.duplicatePath = normalisedPath;
                        result.duplicates.emplace_back(std::move(duplicate));
                    }

                    std::error_code incrementError;
                    it.increment(incrementError);
                    if (incrementError)
                    {
                        ModuleLocatorIssue iteratorIssue;
                        iteratorIssue.path = entry.path();
                        iteratorIssue.message = "failed to enumerate import root";
                        result.issues.emplace_back(std::move(iteratorIssue));
                        break;
                    }
                    continue;
                }

                registerCanonical(*canonical, normalisedPath);

                ModuleLocatorResult registered;
                registered.canonicalPath = *canonical;
                registered.filePath = normalisedPath;
                result.discoveredModules.emplace_back(std::move(registered));

                std::error_code incrementError;
                it.increment(incrementError);
                if (incrementError)
                {
                    ModuleLocatorIssue iteratorIssue;
                    iteratorIssue.path = entry.path();
                    iteratorIssue.message = "failed to enumerate import root";
                    result.issues.emplace_back(std::move(iteratorIssue));
                    break;
                }
            }
        }

        return result;
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

        auto aliasLookup = m_aliases.find(std::string{canonicalPath});
        if (aliasLookup != m_aliases.end())
        {
            auto canonicalRegistered = m_registeredModules.find(aliasLookup->second);
            if (canonicalRegistered != m_registeredModules.end())
            {
                ModuleLocatorResult result;
                result.canonicalPath = canonicalRegistered->first;
                result.filePath = canonicalRegistered->second;
                return result;
            }
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

    void ModuleLocator::registerCanonical(std::string canonicalPath, const std::filesystem::path& filePath)
    {
        auto [iterator, inserted] = m_registeredModules.emplace(canonicalPath, filePath);
        if (!inserted)
        {
            iterator->second = filePath;
        }

        registerDottedAlias(iterator->first);
    }

    void ModuleLocator::registerDottedAlias(const std::string& canonicalPath)
    {
        std::string dotted = canonicalToDotted(canonicalPath);
        if (dotted == canonicalPath)
        {
            return;
        }

        m_aliases.emplace(std::move(dotted), canonicalPath);
    }
} // namespace bolt::hir
