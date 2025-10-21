#include "cli_options.hpp"
#include "linker_invocation.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#    include <process.h>
#else
#    include <spawn.h>
#    include <sys/wait.h>
#    include <unistd.h>

extern char** environ;
#endif

#ifndef BOLT_LD_VERSION
#define BOLT_LD_VERSION "Stage0"
#endif

namespace bolt
{
namespace linker
{
    static void printHelp()
    {
        std::cout << "bolt-ld - Bolt Stage-0 linker wrapper\n"
                     "Usage: bolt-ld [options] <object files>\n\n"
                     "Options:\n"
                     "  --help                    Show this help text and exit.\n"
                     "  --version                 Show version information and exit.\n"
                     "  --emit=<kind>             Output kind: exe, lib (alias link), air, zap. Default: exe.\n"
                     "  --target=<triple>         Target triple (x86_64-pc-windows-msvc, x86_64-air-bolt).\n"
                     "  --sysroot=<path>          Override the sysroot used when invoking the platform linker.\n"
                     "  --runtime-root=<path>     Locate runtime libraries and stubs from the provided directory.\n"
                     "  --linker-script=<path>    Use the given linker script when producing freestanding images.\n"
                     "  --import-bundle=<path>    Path to resolved import metadata to embed into the image.\n"
                     "  --map=<path>             Write a linker map file to the provided path.\n"
                     "  --linker=<path>           Override the detected platform linker executable.\n"
                     "  --archiver=<path>         Override the detected static library or archive tool.\n"
                     "  --entry=<symbol>          Override the entry point symbol when linking.\n"
                     "  --verbose                 Print the computed linker command line.\n"
                     "  --dry-run                 Resolve options but skip invoking the platform linker.\n"
                     "  -L<path>                  Add a library search directory. (Also accepts '-L <path>').\n"
                     "  -l<name>                  Link against a library. (Also accepts '-l <name>').\n"
                     "  -o <path>                 Write output to the specified path.\n";
    }

    static void printVersion()
    {
        std::cout << "bolt-ld Stage-0 wrapper (build profile: " << BOLT_LD_VERSION << ")\n";
    }

    static std::string quoteIfNeeded(const std::string& value)
    {
        if (value.find_first_of(" \"\t") == std::string::npos)
        {
            return value;
        }

        std::string quoted{"\""};
        for (char ch : value)
        {
            if (ch == '\\' || ch == '"')
            {
                quoted.push_back('\\');
            }
            quoted.push_back(ch);
        }
        quoted.push_back('"');
        return quoted;
    }

    static void printInvocation(const LinkerInvocation& invocation)
    {
        std::cout << "[bolt-ld] platform linker: " << quoteIfNeeded(invocation.executable.string()) << "\n";
        for (const auto& argument : invocation.arguments)
        {
            std::cout << "[bolt-ld]   " << quoteIfNeeded(argument) << "\n";
        }
    }

    static std::filesystem::path computeImportBundleOutputPath(const CommandLineOptions& options)
    {
        auto targetPath = options.outputPath;
        targetPath += ".imports";
        return targetPath;
    }

    static bool persistImportBundle(const CommandLineOptions& options)
    {
        auto targetPath = computeImportBundleOutputPath(options);

        std::error_code existsError;
        auto targetExists = std::filesystem::exists(targetPath, existsError);
        if (existsError)
        {
            std::cerr << "bolt-ld: failed to inspect existing import bundle target '" << targetPath
                      << "': " << existsError.message() << "\n";
            return false;
        }

        if (targetExists)
        {
            std::error_code equivalentError;
            if (std::filesystem::equivalent(options.importBundlePath, targetPath, equivalentError))
            {
                std::cout << "[bolt-ld] import bundle already present at " << targetPath << "\n";
                return true;
            }

            if (equivalentError)
            {
                std::cerr << "bolt-ld: failed to compare import bundle paths '" << options.importBundlePath << "' and '"
                          << targetPath << "': " << equivalentError.message() << "\n";
                return false;
            }
        }

        std::error_code copyError;
        std::filesystem::copy_file(options.importBundlePath,
            targetPath,
            std::filesystem::copy_options::overwrite_existing,
            copyError);
        if (copyError)
        {
            std::cerr << "bolt-ld: failed to copy import bundle '" << options.importBundlePath << "' to '" << targetPath
                      << "': " << copyError.message() << "\n";
            return false;
        }

        std::cout << "[bolt-ld] import bundle copied to " << targetPath << "\n";
        return true;
    }

    static int executeLinker(const LinkerInvocation& invocation)
    {
        std::vector<std::string> argumentStorage;
        argumentStorage.reserve(invocation.arguments.size() + 1);
        argumentStorage.push_back(invocation.executable.string());
        for (const auto& argument : invocation.arguments)
        {
            argumentStorage.push_back(argument);
        }

        std::vector<char*> argv;
        argv.reserve(argumentStorage.size() + 1);
        for (auto& entry : argumentStorage)
        {
            argv.push_back(entry.data());
        }
        argv.push_back(nullptr);

#if defined(_WIN32)
        int exitCode = _spawnvp(_P_WAIT, argv[0], argv.data());
        if (exitCode == -1)
        {
            std::cerr << "bolt-ld: failed to launch platform linker '" << invocation.executable
                      << "': " << std::strerror(errno) << "\n";
            return 1;
        }
        return exitCode;
#else
        pid_t pid = 0;
        int spawnResult = posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ);
        if (spawnResult != 0)
        {
            std::cerr << "bolt-ld: failed to launch platform linker '" << invocation.executable
                      << "': " << std::strerror(spawnResult) << "\n";
            return 1;
        }

        int status = 0;
        if (waitpid(pid, &status, 0) == -1)
        {
            std::cerr << "bolt-ld: failed to wait for platform linker '" << invocation.executable
                      << "': " << std::strerror(errno) << "\n";
            return 1;
        }

        if (WIFEXITED(status))
        {
            return WEXITSTATUS(status);
        }

        if (WIFSIGNALED(status))
        {
            int signal = WTERMSIG(status);
            std::cerr << "bolt-ld: platform linker terminated by signal " << signal << ".\n";
            return 128 + signal;
        }

        return status;
#endif
    }

    static int runLinker(const CommandLineOptions& options)
    {
        std::cout << "[bolt-ld] emit: " << toString(options.emitKind) << "\n";
        std::cout << "[bolt-ld] target: " << options.targetTriple << "\n";

        if (!options.outputPath.empty())
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.mapFilePath.empty())
        {
            std::cout << "[bolt-ld] map file: " << options.mapFilePath << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.mapFilePath.empty())
        {
            std::cout << "[bolt-ld] map file: " << options.mapFilePath << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.mapFilePath.empty())
        {
            std::cout << "[bolt-ld] map file: " << options.mapFilePath << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.mapFilePath.empty())
        {
            std::cout << "[bolt-ld] map file: " << options.mapFilePath << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.mapFilePath.empty())
        {
            std::cout << "[bolt-ld] map file: " << options.mapFilePath << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.mapFilePath.empty())
        {
            std::cout << "[bolt-ld] map file: " << options.mapFilePath << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.mapFilePath.empty())
        {
            std::cout << "[bolt-ld] map file: " << options.mapFilePath << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        std::string effectiveEntry = options.entryPoint;
        if (effectiveEntry.empty() && options.targetTriple == "x86_64-air-bolt"
            && options.emitKind == EmitKind::AirImage)
        {
            effectiveEntry = "_start";
        }

        if (!effectiveEntry.empty())
        {
            std::cout << "[bolt-ld] entry: " << effectiveEntry << "\n";
        }

        if (!options.mapFilePath.empty())
        {
            std::cout << "[bolt-ld] map file: " << options.mapFilePath << "\n";
        }

        if (!options.linkerScriptPath.empty())
        {
            std::cout << "[bolt-ld] linker script: " << options.linkerScriptPath << "\n";
        }

        if (!options.importBundlePath.empty())
        {
            std::cout << "[bolt-ld] import bundle: " << options.importBundlePath << "\n";
        }

        if (!options.sysrootPath.empty())
        {
            std::cout << "[bolt-ld] sysroot: " << options.sysrootPath << "\n";
        }

        if (!options.runtimeRootPath.empty())
        {
            std::cout << "[bolt-ld] runtime root: " << options.runtimeRootPath << "\n";
        }

        for (const auto& searchPath : options.librarySearchPaths)
        {
            std::cout << "[bolt-ld] library search: " << searchPath << "\n";
        }

        for (const auto& library : options.libraries)
        {
            std::cout << "[bolt-ld] link library: " << library << "\n";
        }

        for (const auto& object : options.inputObjects)
        {
            std::cout << "[bolt-ld] input: " << object << "\n";
        }

        auto validation = validateLinkerInputs(options, options.dryRun);
        if (validation.hasError)
        {
            std::cerr << "bolt-ld: " << validation.errorMessage << "\n";
            return 1;
        }

        if (options.verbose)
        {
            std::cout << "[bolt-ld] verbose output enabled." << (options.dryRun ? " (dry run)" : "") << "\n";
        }

        auto plan = planLinkerInvocation(options);
        if (plan.hasError)
        {
            std::cerr << "bolt-ld: " << plan.errorMessage << "\n";
            return 1;
        }

        if (options.verbose || options.dryRun)
        {
            printInvocation(plan.invocation);
        }

        if (options.dryRun)
        {
            if (!options.importBundlePath.empty())
            {
                auto targetPath = computeImportBundleOutputPath(options);
                std::cout << "[bolt-ld] dry run: import bundle '" << options.importBundlePath
                          << "' would copy to '" << targetPath << "'.\n";
            }

            if (!options.mapFilePath.empty())
            {
                std::cout << "[bolt-ld] dry run: map file will be written to '" << options.mapFilePath << "'.\n";
            }

            std::cout << "[bolt-ld] dry run: platform linker invocation skipped.\n";
            return 0;
        }

        if (!std::filesystem::exists(plan.invocation.executable))
        {
            std::cerr << "bolt-ld: linker executable '" << plan.invocation.executable
                      << "' was not found. Use --dry-run to inspect the command plan.\n";
            return 1;
        }

        auto exitCode = executeLinker(plan.invocation);
        if (exitCode != 0)
        {
            std::cerr << "bolt-ld: platform linker exited with code " << exitCode << ".\n";
            return exitCode;
        }

        if (!options.importBundlePath.empty())
        {
            if (!persistImportBundle(options))
            {
                return 1;
            }
        }

        return exitCode;
    }
} // namespace linker
} // namespace bolt

int main(int argc, char** argv)
{
    using namespace bolt::linker;

    auto result = parseCommandLine(argc, argv);
    if (result.hasError)
    {
        std::cerr << "bolt-ld: " << result.errorMessage << "\n";
        return 1;
    }

    if (result.showHelp)
    {
        printHelp();
        return 0;
    }

    if (result.showVersion)
    {
        printVersion();
        return 0;
    }

    return runLinker(result.options);
}

