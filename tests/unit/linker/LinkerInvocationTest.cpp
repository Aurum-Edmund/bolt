#include "linker_invocation.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <string>

using namespace bolt::linker;

namespace
{
    CommandLineOptions createBaseOptions()
    {
        CommandLineOptions options;
        options.outputPath = "app.exe";
        options.inputObjects = {"main.obj", "runtime.obj"};
        options.librarySearchPaths = {"lib", "C:/Bolt/lib"};
        options.libraries = {"bolt-runtime", "UserProvided.lib"};
        options.runtimeRootPath = "C:/Bolt/runtime";
        options.emitKind = EmitKind::Executable;
        options.targetTriple = "x86_64-pc-windows-msvc";
        return options;
    }

    class LinkerValidationTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
            workspace = std::filesystem::temp_directory_path() / std::filesystem::path{"bolt_linker_validation"}
                / std::to_string(timestamp) / std::to_string(reinterpret_cast<std::uintptr_t>(this));
            std::filesystem::create_directories(workspace);
        }

        void TearDown() override
        {
            std::error_code ec;
            std::filesystem::remove_all(workspace, ec);
        }

        std::filesystem::path createDirectory(const std::string& name)
        {
            auto path = workspace / name;
            std::filesystem::create_directories(path);
            return path;
        }

        std::filesystem::path createFile(const std::string& name)
        {
            auto path = workspace / name;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream stream{path};
            stream << "bolt";
            return path;
        }

        CommandLineOptions createValidOptions()
        {
            CommandLineOptions options;
            options.targetTriple = "x86_64-pc-windows-msvc";
            options.emitKind = EmitKind::Executable;
            auto outputDirectory = createDirectory("out");
            options.outputPath = outputDirectory / "app.exe";
            options.inputObjects = {createFile("obj/main.obj")};
            options.runtimeRootPath = createDirectory("runtime");
            options.librarySearchPaths = {createDirectory("lib")};
            return options;
        }

        std::filesystem::path workspace;
    };
}

TEST(LinkerInvocationTest, PlansWindowsExecutableInvocation)
{
    auto options = createBaseOptions();
    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    EXPECT_EQ(plan.invocation.executable, std::filesystem::path{"link.exe"});

    std::vector<std::string> expected{
        "/NOLOGO",
        "/OUT:app.exe",
        "/LIBPATH:C:/Bolt/runtime",
        "/LIBPATH:lib",
        "/LIBPATH:C:/Bolt/lib",
        "main.obj",
        "runtime.obj",
        "bolt-runtime.lib",
        "UserProvided.lib",
    };

    ASSERT_EQ(plan.invocation.arguments.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_EQ(plan.invocation.arguments[index], expected[index]) << "mismatch at index " << index;
    }
}

TEST(LinkerInvocationTest, PlansWindowsExecutableWithCustomEntry)
{
    auto options = createBaseOptions();
    options.entryPoint = "BoltStart";

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    ASSERT_FALSE(plan.invocation.arguments.empty());

    EXPECT_NE(std::find(plan.invocation.arguments.begin(), plan.invocation.arguments.end(), "/ENTRY:BoltStart"),
        plan.invocation.arguments.end());
}

TEST(LinkerInvocationTest, RejectsUnsupportedEmitKind)
{
    auto options = createBaseOptions();
    options.emitKind = EmitKind::BoltArchive;

    auto plan = planLinkerInvocation(options);
    EXPECT_TRUE(plan.hasError);
    EXPECT_EQ(plan.errorMessage, "emit kind is not supported for Windows linker planning.");
}

TEST(LinkerInvocationTest, PlansWindowsStaticLibraryInvocation)
{
    auto options = createBaseOptions();
    options.emitKind = EmitKind::StaticLibrary;
    options.outputPath = "runtime.lib";

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    EXPECT_EQ(plan.invocation.executable, std::filesystem::path{"lib.exe"});

    std::vector<std::string> expected{
        "/NOLOGO",
        "/OUT:runtime.lib",
        "/LIBPATH:C:/Bolt/runtime",
        "/LIBPATH:lib",
        "/LIBPATH:C:/Bolt/lib",
        "main.obj",
        "runtime.obj",
        "bolt-runtime.lib",
        "UserProvided.lib",
    };

    ASSERT_EQ(plan.invocation.arguments.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_EQ(plan.invocation.arguments[index], expected[index]) << "mismatch at index " << index;
    }
}

TEST(LinkerInvocationTest, RejectsAirTargetWithoutAirEmitKind)
{
    auto options = createBaseOptions();
    options.targetTriple = "x86_64-air-bolt";

    auto plan = planLinkerInvocation(options);
    EXPECT_TRUE(plan.hasError);
    EXPECT_EQ(plan.errorMessage, "emit kind is not supported for Air linker planning.");
}

TEST(LinkerInvocationTest, RejectsAirTargetWithoutLinkerScript)
{
    auto options = createBaseOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;

    auto plan = planLinkerInvocation(options);
    EXPECT_TRUE(plan.hasError);
    EXPECT_EQ(plan.errorMessage, "linker script is required when targeting x86_64-air-bolt.");
}

TEST(LinkerInvocationTest, PlansAirImageInvocation)
{
    auto options = createBaseOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;
    options.outputPath = "kernel.air";
    options.linkerScriptPath = "scripts/bolt_air.ld";

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    EXPECT_EQ(plan.invocation.executable, std::filesystem::path{"ld.lld"});

    std::vector<std::string> expected{
        "-nostdlib",
        "-static",
        "--gc-sections",
        "--no-undefined",
        "-o",
        "kernel.air",
        "-T",
        "scripts/bolt_air.ld",
        "-e",
        "_start",
        "-LC:/Bolt/runtime",
        "-Llib",
        "-LC:/Bolt/lib",
        "main.obj",
        "runtime.obj",
        "-lbolt-runtime",
        "UserProvided.lib",
    };

    ASSERT_EQ(plan.invocation.arguments.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_EQ(plan.invocation.arguments[index], expected[index]) << "mismatch at index " << index;
    }
}

TEST(LinkerInvocationTest, PlansAirImageWithCustomEntry)
{
    auto options = createBaseOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;
    options.outputPath = "kernel.air";
    options.linkerScriptPath = "scripts/bolt_air.ld";
    options.entryPoint = "boot";

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    ASSERT_GE(plan.invocation.arguments.size(), 2u);

    auto it = std::find(plan.invocation.arguments.begin(), plan.invocation.arguments.end(), "-e");
    ASSERT_NE(it, plan.invocation.arguments.end());
    ++it;
    ASSERT_NE(it, plan.invocation.arguments.end());
    EXPECT_EQ(*it, "boot");
}

TEST(LinkerInvocationTest, PlansAirBoltArchiveInvocation)
{
    CommandLineOptions options;
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::BoltArchive;
    options.outputPath = "libbolt.zap";
    options.inputObjects = {"module.o", "runtime.o"};

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    EXPECT_EQ(plan.invocation.executable, std::filesystem::path{"llvm-ar"});

    std::vector<std::string> expected{
        "rcs",
        "libbolt.zap",
        "module.o",
        "runtime.o",
    };

    ASSERT_EQ(plan.invocation.arguments.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_EQ(plan.invocation.arguments[index], expected[index]) << "mismatch at index " << index;
    }
}

TEST(LinkerInvocationTest, RejectsBoltArchiveWithLibraryFlags)
{
    CommandLineOptions options;
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::BoltArchive;
    options.outputPath = "libbolt.zap";
    options.inputObjects = {"module.o"};
    options.librarySearchPaths = {"/sdk/lib"};
    options.libraries = {"bolt-runtime"};

    auto plan = planLinkerInvocation(options);

    ASSERT_TRUE(plan.hasError);
    EXPECT_EQ(plan.errorMessage,
        "library search paths (-L) are not supported when emitting Bolt archives.");
}

TEST_F(LinkerValidationTest, ReportsMissingImportBundle)
{
    auto options = createValidOptions();
    options.importBundlePath = workspace / "missing.bundle";

    auto result = validateLinkerInputs(options, false);
    ASSERT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage, "import bundle '" + options.importBundlePath.string() + "' was not found.");
}

TEST_F(LinkerValidationTest, ReportsRuntimeRootThatIsNotADirectory)
{
    auto options = createValidOptions();
    auto filePath = createFile("runtime.dat");
    options.runtimeRootPath = filePath;

    auto result = validateLinkerInputs(options, false);
    ASSERT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage, "runtime root '" + filePath.string() + "' is not a directory.");
}

TEST_F(LinkerValidationTest, ReportsMissingOutputDirectory)
{
    auto options = createValidOptions();
    auto missingDir = workspace / "no_such_dir" / "app.exe";
    options.outputPath = missingDir;

    auto result = validateLinkerInputs(options, false);
    ASSERT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage, "output directory '" + missingDir.parent_path().string() + "' was not found.");
}

TEST_F(LinkerValidationTest, SkipsObjectValidationDuringDryRun)
{
    auto options = createValidOptions();
    options.inputObjects = {workspace / "does_not_exist.obj"};

    auto result = validateLinkerInputs(options, true);
    EXPECT_FALSE(result.hasError);
}

TEST_F(LinkerValidationTest, AcceptsValidConfiguration)
{
    auto options = createValidOptions();
    options.importBundlePath = createFile("imports.bin");

    auto result = validateLinkerInputs(options, false);
    EXPECT_FALSE(result.hasError);
}

TEST_F(LinkerValidationTest, RejectsBoltArchiveLibraryFlags)
{
    auto options = createValidOptions();
    options.emitKind = EmitKind::BoltArchive;
    options.librarySearchPaths = {createDirectory("lib")};
    options.libraries = {"bolt-runtime"};

    auto result = validateLinkerInputs(options, false);

    ASSERT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage,
        "library search paths (-L) are not supported when emitting Bolt archives.");
}

TEST_F(LinkerValidationTest, RejectsEntryOverrideForStaticLibrary)
{
    auto options = createValidOptions();
    options.emitKind = EmitKind::StaticLibrary;
    options.outputPath = options.outputPath.parent_path() / "runtime.lib";
    options.entryPoint = "StaticStart";

    auto result = validateLinkerInputs(options, false);

    ASSERT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage,
        "entry overrides are only supported when emitting executables or Air images.");
}

TEST_F(LinkerValidationTest, RejectsEntryOverrideForBoltArchive)
{
    auto options = createValidOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::BoltArchive;
    options.outputPath = options.outputPath.parent_path() / "runtime.zap";
    options.inputObjects = {createFile("obj/runtime.o")};
    options.librarySearchPaths.clear();
    options.libraries.clear();
    options.entryPoint = "ArchiveStart";

    auto result = validateLinkerInputs(options, false);

    ASSERT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage,
        "entry overrides are only supported when emitting executables or Air images.");
}

