#include "cli_options.hpp"

#include <iostream>

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

    static int runLinker(const CommandLineOptions& options)
    {
        std::cout << "[bolt-ld] emit: " << toString(options.emitKind) << "\n";
        std::cout << "[bolt-ld] target: " << options.targetTriple << "\n";

        if (!options.outputPath.empty())
        {
            std::cout << "[bolt-ld] output: " << options.outputPath << "\n";
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

        if (options.verbose)
        {
            std::cout << "[bolt-ld] verbose output enabled." << (options.dryRun ? " (dry run)" : "") << "\n";
        }

        if (options.dryRun)
        {
            std::cout << "[bolt-ld] dry run: platform linker invocation skipped.\n";
        }
        else
        {
            std::cout << "[bolt-ld] (stub) no linking performed yet.\n";
        }

        return 0;
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

