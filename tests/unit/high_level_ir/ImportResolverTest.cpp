#include <gtest/gtest.h>

#include <filesystem>

#include "import_resolver.hpp"
#include "module_locator.hpp"

namespace bolt::hir
{
namespace
{
    static SourceSpan makeSpan(std::uint32_t line, std::uint32_t column)
    {
        SourceSpan span;
        span.begin.line = line;
        span.begin.column = column;
        span.end = span.begin;
        return span;
    }

    static Module makeModule(std::string packageName, std::string moduleName)
    {
        Module module;
        module.packageName = std::move(packageName);
        module.moduleName = std::move(moduleName);
        return module;
    }

    TEST(ImportResolverTest, SelfImportProducesDiagnostic)
    {
        Module module = makeModule("demo.tests", "demo.tests");

        Import importDecl;
        importDecl.modulePath = "demo.tests";
        importDecl.span = makeSpan(3, 1);
        module.imports.emplace_back(importDecl);

        ImportResolver resolver;
        ImportResolutionResult result = resolver.resolve(module);

        ASSERT_EQ(result.imports.size(), 1u);
        EXPECT_EQ(result.imports.front().status, ImportStatus::SelfImport);

        const auto& diagnostics = resolver.diagnostics();
        ASSERT_EQ(diagnostics.size(), 1u);
        EXPECT_EQ(diagnostics.front().code, "BOLT-E2219");
    }

    TEST(ImportResolverTest, PendingWithoutLocator)
    {
        Module module = makeModule("demo.tests", "demo.tests");

        Import importDecl;
        importDecl.modulePath = "demo.utils.core";
        importDecl.span = makeSpan(4, 5);
        module.imports.emplace_back(importDecl);

        ImportResolver resolver;
        ImportResolutionResult result = resolver.resolve(module);

        ASSERT_EQ(result.imports.size(), 1u);
        EXPECT_EQ(result.imports.front().status, ImportStatus::Pending);
        EXPECT_TRUE(resolver.diagnostics().empty());
    }

    TEST(ImportResolverTest, ResolvedWhenModuleRegistered)
    {
        Module module = makeModule("demo.tests", "demo.tests");

        Import importDecl;
        importDecl.modulePath = "demo.utils.core";
        importDecl.span = makeSpan(5, 3);
        module.imports.emplace_back(importDecl);

        ModuleLocator locator;
        locator.registerModule("demo.utils.core", std::filesystem::path{"C:/bolt/demo/utils/core.bolt"});

        ImportResolver resolver;
        resolver.setModuleLocator(&locator);
        ImportResolutionResult result = resolver.resolve(module);

        ASSERT_EQ(result.imports.size(), 1u);
        const auto& resolved = result.imports.front();
        EXPECT_EQ(resolved.status, ImportStatus::Resolved);
        ASSERT_TRUE(resolved.resolvedFilePath.has_value());
        EXPECT_NE(resolved.resolvedFilePath->find("demo"), std::string::npos);
        EXPECT_TRUE(resolver.diagnostics().empty());
    }

    TEST(ImportResolverTest, MissingImportEmitsDiagnostic)
    {
        Module module = makeModule("demo.tests", "demo.tests");

        Import importDecl;
        importDecl.modulePath = "demo.unknown.module";
        importDecl.span = makeSpan(6, 2);
        module.imports.emplace_back(importDecl);

        ModuleLocator locator;
        locator.setSearchRoots({std::filesystem::current_path()});

        ImportResolver resolver;
        resolver.setModuleLocator(&locator);
        ImportResolutionResult result = resolver.resolve(module);

        ASSERT_EQ(result.imports.size(), 1u);
        EXPECT_EQ(result.imports.front().status, ImportStatus::NotFound);

        const auto& diagnostics = resolver.diagnostics();
        ASSERT_EQ(diagnostics.size(), 1u);
        EXPECT_EQ(diagnostics.front().code, "BOLT-E2220");
    }
}
} // namespace bolt::hir
