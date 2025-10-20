#include "linker_invocation.hpp"

#include <gtest/gtest.h>

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

