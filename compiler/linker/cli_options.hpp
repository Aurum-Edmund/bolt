#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace bolt
{
namespace linker
{
    enum class EmitKind
    {
        Executable,
        StaticLibrary,
        AirImage,
        BoltArchive,
    };

    struct CommandLineOptions
    {
        std::vector<std::filesystem::path> inputObjects;
        std::vector<std::filesystem::path> librarySearchPaths;
        std::vector<std::string> libraries;
        std::filesystem::path outputPath;
        std::filesystem::path linkerScriptPath;
        std::filesystem::path importBundlePath;
        std::filesystem::path sysrootPath;
        std::filesystem::path runtimeRootPath;
        std::string entryPoint;
        std::string targetTriple{"x86_64-pc-windows-msvc"};
        EmitKind emitKind{EmitKind::Executable};
        bool verbose{false};
        bool dryRun{false};
    };

    struct CommandLineParseResult
    {
        CommandLineOptions options;
        bool showHelp{false};
        bool showVersion{false};
        bool hasError{false};
        std::string errorMessage;
    };

    CommandLineParseResult parseCommandLine(int argc, char** argv);

    const char* toString(EmitKind kind);
} // namespace linker
} // namespace bolt

