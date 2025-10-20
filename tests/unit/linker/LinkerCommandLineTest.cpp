#include "cli_options.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace bolt::linker;

namespace
{
    CommandLineParseResult parse(std::initializer_list<std::string> arguments)
    {
        std::vector<std::string> ownedArguments{arguments};
        std::vector<char*> rawArguments;
        rawArguments.reserve(ownedArguments.size());
        for (auto& argument : ownedArguments)
        {
            rawArguments.push_back(argument.data());
        }

        return parseCommandLine(static_cast<int>(rawArguments.size()), rawArguments.data());
    }
}

TEST(LinkerCommandLineTest, ParsesMinimumExecutable)
{
    auto result = parse({"bolt-ld", "-o", "a.exe", "main.obj"});
    ASSERT_FALSE(result.hasError);
    EXPECT_FALSE(result.showHelp);
    EXPECT_FALSE(result.showVersion);
    EXPECT_EQ(result.options.outputPath, std::filesystem::path{"a.exe"});
    ASSERT_EQ(result.options.inputObjects.size(), 1u);
    EXPECT_EQ(result.options.inputObjects.front(), std::filesystem::path{"main.obj"});
    EXPECT_EQ(result.options.emitKind, EmitKind::Executable);
}

TEST(LinkerCommandLineTest, SupportsHelpFlag)
{
    auto result = parse({"bolt-ld", "--help"});
    EXPECT_TRUE(result.showHelp);
    EXPECT_FALSE(result.hasError);
}

TEST(LinkerCommandLineTest, ReportsMissingOutput)
{
    auto result = parse({"bolt-ld", "main.obj"});
    EXPECT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage, "output path is required (use -o).");
}

TEST(LinkerCommandLineTest, RejectsUnknownOption)
{
    auto result = parse({"bolt-ld", "--no-such-option", "-o", "out.exe", "main.obj"});
    EXPECT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage, "unknown option '--no-such-option'.");
}

TEST(LinkerCommandLineTest, ParsesExtendedOptions)
{
    auto result = parse({"bolt-ld",
        "--emit=air",
        "--target=x86_64-air-bolt",
        "--sysroot=/sdk",
        "--runtime-root", "/runtime",
        "--linker-script", "air.ld",
        "--import-bundle", "imports.json",
        "--verbose",
        "--dry-run",
        "-Llib",
        "-L", "/opt/bolt/lib",
        "-lbolt-runtime",
        "-l", "m",
        "-o",
        "image.air",
        "app.o",
        "runtime.o"});

    ASSERT_FALSE(result.hasError);
    EXPECT_EQ(result.options.emitKind, EmitKind::AirImage);
    EXPECT_EQ(result.options.targetTriple, "x86_64-air-bolt");
    EXPECT_EQ(result.options.sysrootPath, std::filesystem::path{"/sdk"});
    EXPECT_EQ(result.options.runtimeRootPath, std::filesystem::path{"/runtime"});
    EXPECT_EQ(result.options.linkerScriptPath, std::filesystem::path{"air.ld"});
    EXPECT_EQ(result.options.importBundlePath, std::filesystem::path{"imports.json"});
    EXPECT_TRUE(result.options.verbose);
    EXPECT_TRUE(result.options.dryRun);
    ASSERT_EQ(result.options.librarySearchPaths.size(), 2u);
    EXPECT_EQ(result.options.librarySearchPaths[0], std::filesystem::path{"lib"});
    EXPECT_EQ(result.options.librarySearchPaths[1], std::filesystem::path{"/opt/bolt/lib"});
    ASSERT_EQ(result.options.libraries.size(), 2u);
    EXPECT_EQ(result.options.libraries[0], "bolt-runtime");
    EXPECT_EQ(result.options.libraries[1], "m");
    ASSERT_EQ(result.options.inputObjects.size(), 2u);
    EXPECT_EQ(result.options.inputObjects[0], std::filesystem::path{"app.o"});
    EXPECT_EQ(result.options.inputObjects[1], std::filesystem::path{"runtime.o"});
    EXPECT_EQ(result.options.outputPath, std::filesystem::path{"image.air"});
}

TEST(LinkerCommandLineTest, RejectsUnknownEmitKind)
{
    auto result = parse({"bolt-ld", "--emit=bin", "-o", "app.exe", "app.obj"});
    EXPECT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage, "unknown emit kind 'bin'.");
}

TEST(LinkerCommandLineTest, ParsesStaticLibraryEmitKind)
{
    auto result = parse({"bolt-ld", "--emit=lib", "-o", "libbolt.lib", "main.obj"});

    ASSERT_FALSE(result.hasError);
    EXPECT_EQ(result.options.emitKind, EmitKind::StaticLibrary);
    EXPECT_EQ(result.options.outputPath, std::filesystem::path{"libbolt.lib"});
}

TEST(LinkerCommandLineTest, RejectsUnknownTarget)
{
    auto result = parse({"bolt-ld", "--target=ppc-none", "-o", "out.exe", "main.obj"});
    EXPECT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage, "unsupported target 'ppc-none'.");
}

TEST(LinkerCommandLineTest, DefaultsAirTargetWhenEmitAirIsRequested)
{
    auto result = parse({"bolt-ld", "--emit=air", "-o", "kernel.air", "kernel.o"});

    ASSERT_FALSE(result.hasError);
    EXPECT_EQ(result.options.emitKind, EmitKind::AirImage);
    EXPECT_EQ(result.options.targetTriple, "x86_64-air-bolt");
}

TEST(LinkerCommandLineTest, DefaultsAirTargetWhenEmitZapIsRequested)
{
    auto result = parse({"bolt-ld", "--emit=zap", "-o", "library.zap", "module.o"});

    ASSERT_FALSE(result.hasError);
    EXPECT_EQ(result.options.emitKind, EmitKind::BoltArchive);
    EXPECT_EQ(result.options.targetTriple, "x86_64-air-bolt");
}

TEST(LinkerCommandLineTest, RejectsWindowsTargetForAirEmit)
{
    auto result = parse({"bolt-ld", "--emit=air", "--target=x86_64-pc-windows-msvc", "-o", "kernel.air", "kernel.o"});

    EXPECT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage, "emit kind 'air' requires target 'x86_64-air-bolt'.");
}

TEST(LinkerCommandLineTest, RejectsAirTargetForExecutableEmit)
{
    auto result = parse({"bolt-ld", "--target=x86_64-air-bolt", "-o", "app.exe", "main.obj"});

    EXPECT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage, "emit kind 'exe' is not supported for target 'x86_64-air-bolt'.");
}

