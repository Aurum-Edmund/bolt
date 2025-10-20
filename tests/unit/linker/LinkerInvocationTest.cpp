#include "linker_invocation.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <string>
#include <vector>

using namespace bolt::linker;

namespace
{
    std::string buildMissingRuntimeMessage(const std::filesystem::path& runtimeRoot,
        std::initializer_list<const char*> candidateNames,
        const std::string& targetTriple)
    {
        std::vector<std::filesystem::path> searchRoots;
        searchRoots.push_back(runtimeRoot);
        searchRoots.push_back(runtimeRoot / "lib");
        if (!targetTriple.empty())
        {
            searchRoots.push_back(runtimeRoot / targetTriple);
            searchRoots.push_back(runtimeRoot / "lib" / targetTriple);
        }

        std::string searchRootsMessage;
        bool firstRoot = true;
        for (const auto& root : searchRoots)
        {
            if (!firstRoot)
            {
                searchRootsMessage += ", ";
            }
            firstRoot = false;

            searchRootsMessage += "'";
            searchRootsMessage += root.string();
            searchRootsMessage += "'";
        }

        std::string candidateMessage;
        bool firstCandidate = true;
        for (const auto* name : candidateNames)
        {
            if (!firstCandidate)
            {
                candidateMessage += ", ";
            }
            firstCandidate = false;

            candidateMessage += "'";
            candidateMessage += name;
            candidateMessage += "'";
        }

        return "runtime root '" + runtimeRoot.string()
            + "' is missing required runtime archive (searched: " + searchRootsMessage
            + "; expected one of: " + candidateMessage + ").";
    }

    class LinkerTestWorkspace : public ::testing::Test
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

        std::filesystem::path workspace;
    };

    class LinkerInvocationPlanningTest : public LinkerTestWorkspace
    {
    protected:
        CommandLineOptions createWindowsExecutableOptions()
        {
            CommandLineOptions options;
            options.targetTriple = "x86_64-pc-windows-msvc";
            options.emitKind = EmitKind::Executable;
            auto outputDirectory = createDirectory("out");
            options.outputPath = outputDirectory / "app.exe";
            options.inputObjects = {createFile("obj/main.obj"), createFile("obj/runtime.obj")};
            options.librarySearchPaths = {createDirectory("lib"), createDirectory("vendor/lib")};
            options.libraries = {"UserProvided"};

            auto runtimeDirectory = createDirectory("runtime");
            createDirectory("runtime/lib");
            windowsRuntimeLibrary = createFile("runtime/lib/bolt_runtime.lib");
            airRuntimeLibrary = createFile("runtime/lib/libbolt_runtime.a");
            options.runtimeRootPath = runtimeDirectory;

            return options;
        }

        std::filesystem::path windowsRuntimeLibrary;
        std::filesystem::path airRuntimeLibrary;
    };

    class LinkerValidationTest : public LinkerTestWorkspace
    {
    protected:
        CommandLineOptions createValidOptions()
        {
            CommandLineOptions options;
            options.targetTriple = "x86_64-pc-windows-msvc";
            options.emitKind = EmitKind::Executable;
            auto outputDirectory = createDirectory("out");
            options.outputPath = outputDirectory / "app.exe";
            options.inputObjects = {createFile("obj/main.obj")};
            auto runtimeDirectory = createDirectory("runtime");
            options.runtimeRootPath = runtimeDirectory;
            createFile("runtime/lib/bolt_runtime.lib");
            options.librarySearchPaths = {createDirectory("lib")};
            return options;
        }
    };
}

TEST_F(LinkerInvocationPlanningTest, PlansWindowsExecutableInvocation)
{
    auto options = createWindowsExecutableOptions();
    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    EXPECT_EQ(plan.invocation.executable, std::filesystem::path{"link.exe"});

    std::vector<std::string> expected{
        "/NOLOGO",
        std::string{"/OUT:"} + options.outputPath.string(),
        std::string{"/LIBPATH:"} + options.runtimeRootPath.string(),
        std::string{"/LIBPATH:"} + options.librarySearchPaths[0].string(),
        std::string{"/LIBPATH:"} + options.librarySearchPaths[1].string(),
        options.inputObjects[0].string(),
        options.inputObjects[1].string(),
        "UserProvided.lib",
        windowsRuntimeLibrary.string(),
    };

    ASSERT_EQ(plan.invocation.arguments.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_EQ(plan.invocation.arguments[index], expected[index]) << "mismatch at index " << index;
    }
}

TEST_F(LinkerInvocationPlanningTest, SkipsRuntimeInjectionWhenDisabled)
{
    auto options = createWindowsExecutableOptions();
    options.disableRuntimeInjection = true;
    options.runtimeRootPath.clear();

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    std::vector<std::string> expected{
        "/NOLOGO",
        std::string{"/OUT:"} + options.outputPath.string(),
        std::string{"/LIBPATH:"} + options.librarySearchPaths[0].string(),
        std::string{"/LIBPATH:"} + options.librarySearchPaths[1].string(),
        options.inputObjects[0].string(),
        options.inputObjects[1].string(),
        "UserProvided.lib",
    };

    ASSERT_EQ(plan.invocation.arguments.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_EQ(plan.invocation.arguments[index], expected[index]) << "mismatch at index " << index;
    }
}

TEST_F(LinkerInvocationPlanningTest, UsesCustomLinkerExecutableForWindows)
{
    auto options = createWindowsExecutableOptions();
    options.linkerExecutableOverride = "C:/Tools/custom-link.exe";

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    EXPECT_EQ(plan.invocation.executable, std::filesystem::path{"C:/Tools/custom-link.exe"});
}

TEST_F(LinkerInvocationPlanningTest, PlansWindowsExecutableWithCustomEntry)
{
    auto options = createWindowsExecutableOptions();
    options.entryPoint = "BoltStart";

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    ASSERT_FALSE(plan.invocation.arguments.empty());

    EXPECT_NE(std::find(plan.invocation.arguments.begin(), plan.invocation.arguments.end(), "/ENTRY:BoltStart"),
        plan.invocation.arguments.end());
}

TEST_F(LinkerInvocationPlanningTest, RejectsUnsupportedEmitKind)
{
    auto options = createWindowsExecutableOptions();
    options.emitKind = EmitKind::BoltArchive;

    auto plan = planLinkerInvocation(options);
    EXPECT_TRUE(plan.hasError);
    EXPECT_EQ(plan.errorMessage, "emit kind is not supported for Windows linker planning.");
}

TEST_F(LinkerInvocationPlanningTest, PlansWindowsStaticLibraryInvocation)
{
    auto options = createWindowsExecutableOptions();
    options.emitKind = EmitKind::StaticLibrary;
    options.outputPath = options.outputPath.parent_path() / "runtime.lib";

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    EXPECT_EQ(plan.invocation.executable, std::filesystem::path{"lib.exe"});

    std::vector<std::string> expected{
        "/NOLOGO",
        std::string{"/OUT:"} + options.outputPath.string(),
        std::string{"/LIBPATH:"} + options.runtimeRootPath.string(),
        std::string{"/LIBPATH:"} + options.librarySearchPaths[0].string(),
        std::string{"/LIBPATH:"} + options.librarySearchPaths[1].string(),
        options.inputObjects[0].string(),
        options.inputObjects[1].string(),
        "UserProvided.lib",
    };

    ASSERT_EQ(plan.invocation.arguments.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_EQ(plan.invocation.arguments[index], expected[index]) << "mismatch at index " << index;
    }
}

TEST_F(LinkerInvocationPlanningTest, UsesCustomArchiverForWindowsLibraries)
{
    auto options = createWindowsExecutableOptions();
    options.emitKind = EmitKind::StaticLibrary;
    options.outputPath = options.outputPath.parent_path() / "runtime.lib";
    options.archiverExecutableOverride = "C:/Tools/custom-lib.exe";

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    EXPECT_EQ(plan.invocation.executable, std::filesystem::path{"C:/Tools/custom-lib.exe"});
}

TEST_F(LinkerInvocationPlanningTest, RejectsAirTargetWithoutAirEmitKind)
{
    auto options = createWindowsExecutableOptions();
    options.targetTriple = "x86_64-air-bolt";

    auto plan = planLinkerInvocation(options);
    EXPECT_TRUE(plan.hasError);
    EXPECT_EQ(plan.errorMessage, "emit kind is not supported for Air linker planning.");
}

TEST_F(LinkerInvocationPlanningTest, RejectsAirTargetWithoutLinkerScript)
{
    auto options = createWindowsExecutableOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;

    auto plan = planLinkerInvocation(options);
    EXPECT_TRUE(plan.hasError);
    EXPECT_EQ(plan.errorMessage, "linker script is required when targeting x86_64-air-bolt.");
}

TEST_F(LinkerInvocationPlanningTest, RejectsWindowsExecutableWhenRuntimeLibraryMissing)
{
    auto options = createWindowsExecutableOptions();
    std::error_code ec;
    std::filesystem::remove(windowsRuntimeLibrary, ec);
    std::filesystem::remove(airRuntimeLibrary, ec);

    auto plan = planLinkerInvocation(options);
    ASSERT_TRUE(plan.hasError);
    EXPECT_EQ(plan.errorMessage,
        buildMissingRuntimeMessage(options.runtimeRootPath,
            {"bolt_runtime.lib", "libbolt_runtime.a"},
            options.targetTriple));
}

TEST_F(LinkerInvocationPlanningTest, RejectsAirInvocationWithoutRuntimeRoot)
{
    auto options = createWindowsExecutableOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;
    options.outputPath = options.outputPath.parent_path() / "kernel.air";
    options.linkerScriptPath = options.outputPath.parent_path() / "bolt_air.ld";
    options.runtimeRootPath.clear();

    auto plan = planLinkerInvocation(options);
    ASSERT_TRUE(plan.hasError);
    EXPECT_EQ(plan.errorMessage, "Air images require --runtime-root to locate runtime stubs.");
}

TEST_F(LinkerInvocationPlanningTest, PlansAirInvocationWithoutRuntimeWhenDisabled)
{
    auto options = createWindowsExecutableOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;
    options.outputPath = options.outputPath.parent_path() / "kernel.air";
    options.linkerScriptPath = options.outputPath.parent_path() / "bolt_air.ld";
    options.runtimeRootPath.clear();
    options.disableRuntimeInjection = true;

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    std::vector<std::string> expected{
        "-nostdlib",
        "-static",
        "--gc-sections",
        "--no-undefined",
        "-o",
        options.outputPath.string(),
        "-T",
        options.linkerScriptPath.string(),
        "-e",
        "_start",
        std::string{"-L"} + options.librarySearchPaths[0].string(),
        std::string{"-L"} + options.librarySearchPaths[1].string(),
        options.inputObjects[0].string(),
        options.inputObjects[1].string(),
        "-lUserProvided",
    };

    ASSERT_EQ(plan.invocation.arguments.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_EQ(plan.invocation.arguments[index], expected[index]) << "mismatch at index " << index;
    }
}

TEST_F(LinkerInvocationPlanningTest, RejectsAirInvocationWhenRuntimeLibraryMissing)
{
    auto options = createWindowsExecutableOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;
    options.outputPath = options.outputPath.parent_path() / "kernel.air";
    options.linkerScriptPath = options.outputPath.parent_path() / "bolt_air.ld";
    std::error_code ec;
    std::filesystem::remove(airRuntimeLibrary, ec);
    std::filesystem::remove(windowsRuntimeLibrary, ec);

    auto plan = planLinkerInvocation(options);
    ASSERT_TRUE(plan.hasError);
    EXPECT_EQ(plan.errorMessage,
        buildMissingRuntimeMessage(options.runtimeRootPath,
            {"libbolt_runtime.a", "bolt_runtime.lib"},
            options.targetTriple));
}

TEST_F(LinkerInvocationPlanningTest, PlansAirImageInvocation)
{
    auto options = createWindowsExecutableOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;
    options.outputPath = options.outputPath.parent_path() / "kernel.air";
    options.linkerScriptPath = options.outputPath.parent_path() / "bolt_air.ld";
    options.entryPoint.clear();

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    EXPECT_EQ(plan.invocation.executable, std::filesystem::path{"ld.lld"});

    std::vector<std::string> expected{
        "-nostdlib",
        "-static",
        "--gc-sections",
        "--no-undefined",
        "-o",
        options.outputPath.string(),
        "-T",
        options.linkerScriptPath.string(),
        "-e",
        "_start",
        std::string{"-L"} + options.runtimeRootPath.string(),
        std::string{"-L"} + options.librarySearchPaths[0].string(),
        std::string{"-L"} + options.librarySearchPaths[1].string(),
        options.inputObjects[0].string(),
        options.inputObjects[1].string(),
        "-lUserProvided",
        airRuntimeLibrary.string(),
    };

    ASSERT_EQ(plan.invocation.arguments.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_EQ(plan.invocation.arguments[index], expected[index]) << "mismatch at index " << index;
    }
}

TEST_F(LinkerInvocationPlanningTest, UsesCustomLinkerExecutableForAirImages)
{
    auto options = createWindowsExecutableOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;
    options.outputPath = options.outputPath.parent_path() / "kernel.air";
    options.linkerScriptPath = options.outputPath.parent_path() / "bolt_air.ld";
    options.linkerExecutableOverride = "/opt/bolt/bin/custom-ld";

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    EXPECT_EQ(plan.invocation.executable, std::filesystem::path{"/opt/bolt/bin/custom-ld"});
}

TEST_F(LinkerInvocationPlanningTest, PlansAirImageWithCustomEntry)
{
    auto options = createWindowsExecutableOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;
    options.outputPath = options.outputPath.parent_path() / "kernel.air";
    options.linkerScriptPath = options.outputPath.parent_path() / "bolt_air.ld";
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

TEST(LinkerInvocationTest, UsesCustomArchiverForBoltArchives)
{
    CommandLineOptions options;
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::BoltArchive;
    options.outputPath = "libbolt.zap";
    options.inputObjects = {"module.o", "runtime.o"};
    options.archiverExecutableOverride = "/opt/bolt/bin/custom-ar";

    auto plan = planLinkerInvocation(options);

    ASSERT_FALSE(plan.hasError);
    EXPECT_EQ(plan.invocation.executable, std::filesystem::path{"/opt/bolt/bin/custom-ar"});
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

TEST_F(LinkerValidationTest, ReportsMissingLinkerOverride)
{
    auto options = createValidOptions();
    options.linkerExecutableOverride = workspace / "missing-link.exe";

    auto result = validateLinkerInputs(options, false);
    ASSERT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage,
        "linker executable '" + options.linkerExecutableOverride.string() + "' was not found.");
}

TEST_F(LinkerValidationTest, ReportsMissingArchiverOverride)
{
    auto options = createValidOptions();
    options.emitKind = EmitKind::StaticLibrary;
    options.outputPath = options.outputPath.parent_path() / "runtime.lib";
    options.archiverExecutableOverride = workspace / "missing-lib.exe";

    auto result = validateLinkerInputs(options, false);
    ASSERT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage,
        "archiver executable '" + options.archiverExecutableOverride.string() + "' was not found.");
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

TEST_F(LinkerValidationTest, ReportsMissingRuntimeLibraryForWindowsExecutable)
{
    auto options = createValidOptions();
    auto runtimeLibrary = options.runtimeRootPath / "lib" / "bolt_runtime.lib";
    std::error_code ec;
    std::filesystem::remove(runtimeLibrary, ec);

    auto result = validateLinkerInputs(options, false);
    ASSERT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage,
        buildMissingRuntimeMessage(options.runtimeRootPath,
            {"bolt_runtime.lib", "libbolt_runtime.a"},
            options.targetTriple));
}

TEST_F(LinkerValidationTest, AllowsWindowsExecutableWithoutRuntimeWhenDisabled)
{
    auto options = createValidOptions();
    options.disableRuntimeInjection = true;
    auto runtimeLibrary = options.runtimeRootPath / "lib" / "bolt_runtime.lib";
    std::error_code ec;
    std::filesystem::remove(runtimeLibrary, ec);

    auto result = validateLinkerInputs(options, false);
    EXPECT_FALSE(result.hasError);
}

TEST_F(LinkerValidationTest, ReportsMissingRuntimeRootForAirImages)
{
    auto options = createValidOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;
    options.runtimeRootPath.clear();
    options.linkerScriptPath = createFile("scripts/air.ld");

    auto result = validateLinkerInputs(options, false);
    ASSERT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage, "Air images require --runtime-root to locate runtime stubs.");
}

TEST_F(LinkerValidationTest, AllowsAirImageWithoutRuntimeWhenDisabled)
{
    auto options = createValidOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;
    options.runtimeRootPath.clear();
    options.linkerScriptPath = createFile("scripts/air.ld");
    options.disableRuntimeInjection = true;

    auto result = validateLinkerInputs(options, false);
    EXPECT_FALSE(result.hasError);
}

TEST_F(LinkerValidationTest, ReportsMissingRuntimeLibraryForAirImages)
{
    auto options = createValidOptions();
    options.targetTriple = "x86_64-air-bolt";
    options.emitKind = EmitKind::AirImage;
    options.linkerScriptPath = createFile("scripts/air.ld");
    auto runtimeLibrary = options.runtimeRootPath / "lib" / "bolt_runtime.lib";
    std::error_code ec;
    std::filesystem::remove(runtimeLibrary, ec);

    auto result = validateLinkerInputs(options, false);
    ASSERT_TRUE(result.hasError);
    EXPECT_EQ(result.errorMessage,
        buildMissingRuntimeMessage(options.runtimeRootPath,
            {"libbolt_runtime.a", "bolt_runtime.lib"},
            options.targetTriple));
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

