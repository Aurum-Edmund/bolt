#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "module_locator.hpp"

namespace bolt::hir
{
namespace
{
    struct ScopedDirectory
    {
        std::filesystem::path path;
        explicit ScopedDirectory(std::filesystem::path directory) : path(std::move(directory)) {}
        ~ScopedDirectory()
        {
            if (!path.empty())
            {
                std::error_code ec;
                std::filesystem::remove_all(path, ec);
            }
        }
    };

    std::filesystem::path makeTemporaryRoot(const std::string& prefix)
    {
        auto root = std::filesystem::temp_directory_path()
            / (prefix + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
        return root;
    }

    TEST(ModuleLocatorTest, DiscoversModulesFromSearchRoots)
    {
        ScopedDirectory cleanup{makeTemporaryRoot("bolt-modlocator-discover-")};
        const auto modulePath = cleanup.path / "demo" / "utils" / "core.bolt";
        std::filesystem::create_directories(modulePath.parent_path());
        {
            std::ofstream stream(modulePath);
            stream << "// synthetic module";
        }

        ModuleLocator locator;
        locator.setSearchRoots({cleanup.path});
        ModuleLocatorDiscoveryResult discovery = locator.discoverModules();

        ASSERT_EQ(discovery.discoveredModules.size(), 1u);
        EXPECT_EQ(discovery.discoveredModules.front().canonicalPath, "demo::utils::core");
        EXPECT_EQ(discovery.discoveredModules.front().filePath.lexically_normal(), modulePath.lexically_normal());
        EXPECT_TRUE(discovery.duplicates.empty());
        EXPECT_TRUE(discovery.issues.empty());

        auto locatedCanonical = locator.locate("demo::utils::core");
        ASSERT_TRUE(locatedCanonical.has_value());
        EXPECT_EQ(locatedCanonical->filePath.lexically_normal(), modulePath.lexically_normal());

        auto locatedDotted = locator.locate("demo.utils.core");
        ASSERT_TRUE(locatedDotted.has_value());
        EXPECT_EQ(locatedDotted->filePath.lexically_normal(), modulePath.lexically_normal());
    }

    TEST(ModuleLocatorTest, ReportsDuplicateModulesAcrossRoots)
    {
        ScopedDirectory firstRoot{makeTemporaryRoot("bolt-modlocator-first-")};
        ScopedDirectory secondRoot{makeTemporaryRoot("bolt-modlocator-second-")};

        const auto relativeDir = std::filesystem::path{"demo"} / "utils";
        const auto firstModule = firstRoot.path / relativeDir / "core.bolt";
        const auto secondModule = secondRoot.path / relativeDir / "core.bolt";

        std::filesystem::create_directories(firstModule.parent_path());
        std::filesystem::create_directories(secondModule.parent_path());

        {
            std::ofstream stream(firstModule);
            stream << "// first module";
        }
        {
            std::ofstream stream(secondModule);
            stream << "// duplicate module";
        }

        ModuleLocator locator;
        locator.setSearchRoots({firstRoot.path, secondRoot.path});
        ModuleLocatorDiscoveryResult discovery = locator.discoverModules();

        EXPECT_EQ(discovery.discoveredModules.size(), 1u);
        ASSERT_EQ(discovery.duplicates.size(), 1u);
        EXPECT_EQ(discovery.duplicates.front().canonicalPath, "demo::utils::core");
        EXPECT_EQ(discovery.duplicates.front().existingPath.lexically_normal(), firstModule.lexically_normal());
        EXPECT_EQ(discovery.duplicates.front().duplicatePath.lexically_normal(), secondModule.lexically_normal());
        EXPECT_TRUE(discovery.issues.empty());

        auto located = locator.locate("demo::utils::core");
        ASSERT_TRUE(located.has_value());
        EXPECT_EQ(located->filePath.lexically_normal(), firstModule.lexically_normal());
    }

    TEST(ModuleLocatorTest, ReportsInvalidImportRoots)
    {
        ScopedDirectory base{makeTemporaryRoot("bolt-modlocator-invalid-")};
        const auto missingRoot = base.path / "missing";
        const auto fileRoot = base.path / "not-a-directory.bolt";
        {
            std::ofstream stream(fileRoot);
            stream << "// not a directory";
        }

        ModuleLocator locator;
        locator.setSearchRoots({missingRoot, fileRoot});
        ModuleLocatorDiscoveryResult discovery = locator.discoverModules();

        EXPECT_TRUE(discovery.discoveredModules.empty());
        EXPECT_TRUE(discovery.duplicates.empty());
        ASSERT_EQ(discovery.issues.size(), 2u);

        bool hasMissing = false;
        bool hasNotDirectory = false;
        for (const auto& issue : discovery.issues)
        {
            if (issue.message.find("does not exist") != std::string::npos)
            {
                hasMissing = true;
            }
            if (issue.message.find("not a directory") != std::string::npos)
            {
                hasNotDirectory = true;
            }
        }

        EXPECT_TRUE(hasMissing);
        EXPECT_TRUE(hasNotDirectory);
    }
}
} // namespace bolt::hir
