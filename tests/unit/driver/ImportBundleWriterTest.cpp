#include <gtest/gtest.h>

#include "import_bundle_writer.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace
{
    std::string readFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    std::filesystem::path makeTempRoot()
    {
        const auto timestamp
            = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::filesystem::path root = std::filesystem::temp_directory_path();
        root /= "bolt-import-" + std::to_string(timestamp);
        return root;
    }

    TEST(ImportBundleWriterTest, WritesManifestWithResolvedAndPendingImports)
    {
        bolt::hir::Module module;
        module.packageName = "demo";
        module.moduleName = "consumer";

        bolt::hir::ImportResolutionResult resolution;
        bolt::hir::ImportResolution resolved;
        resolved.modulePath = "demo.alpha";
        resolved.status = bolt::hir::ImportStatus::Resolved;
        resolved.canonicalModulePath = std::string{"demo.alpha"};
        resolved.resolvedFilePath = std::string{"/modules/alpha.bolt"};
        resolution.imports.emplace_back(resolved);

        bolt::hir::ImportResolution pending;
        pending.modulePath = "demo.beta";
        pending.status = bolt::hir::ImportStatus::Pending;
        resolution.imports.emplace_back(pending);

        const auto tempRoot = makeTempRoot();
        const auto outputPath = tempRoot / "bundle.json";

        std::string errorMessage;
        ASSERT_TRUE(bolt::writeImportBundle(outputPath, module, resolution, errorMessage));
        const std::string content = readFile(outputPath);

        const std::string expected = R"JSON({
  "module": {
    "package": "demo",
    "name": "consumer",
    "canonical": "demo::consumer"
  },
  "imports": [
    {
      "module": "demo.alpha",
      "status": "resolved",
      "canonical": "demo.alpha",
      "file": "/modules/alpha.bolt"
    },
    {
      "module": "demo.beta",
      "status": "pending"
    }
  ]
}
)JSON";

        EXPECT_EQ(content, expected);

        std::error_code cleanupError;
        std::filesystem::remove_all(tempRoot, cleanupError);
    }

    TEST(ImportBundleWriterTest, ComputesCanonicalNameWhenPackageMatchesModule)
    {
        bolt::hir::Module module;
        module.packageName = "runtime";
        module.moduleName = "runtime";

        bolt::hir::ImportResolutionResult resolution;

        const auto tempRoot = makeTempRoot();
        const auto outputPath = tempRoot / "bundle.json";

        std::string errorMessage;
        ASSERT_TRUE(bolt::writeImportBundle(outputPath, module, resolution, errorMessage));
        const std::string content = readFile(outputPath);

        EXPECT_NE(content.find("\"canonical\": \"runtime\""), std::string::npos);

        std::error_code cleanupError;
        std::filesystem::remove_all(tempRoot, cleanupError);
    }
} // namespace

