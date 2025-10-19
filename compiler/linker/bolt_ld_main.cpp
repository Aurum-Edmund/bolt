#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef BOLT_LD_VERSION
#define BOLT_LD_VERSION "Stage0"
#endif

namespace bolt::linker
{
    struct CommandLineOptions
    {
        std::vector<std::filesystem::path> inputObjects;
        std::filesystem::path outputPath;
        std::string emitKind{"exe"}; // exe, link, air, zap
        bool showHelp{false};
        bool showVersion{false};
    };

    static void printHelp()
    {
        std::cout << "bolt-ld - Bolt Stage-0 linker wrapper\n"
                     "Usage: bolt-ld [options] <object files>\n\n"
                     "Options:\n"
                     "  --help                 Show this help text and exit.\n"
                     "  --version              Show version information and exit.\n"
                     "  --emit=<kind>          Output kind: exe, link, air, zap. Default: exe.\n"
                     "  -o <path>              Write output to the specified path.\n";
    }

    static void printVersion()
    {
        std::cout << "bolt-ld Stage-0 wrapper (build profile: " << BOLT_LD_VERSION << ")\n";
    }

    static std::optional<CommandLineOptions> parseCommandLine(int argc, char** argv)
    {
        CommandLineOptions options;

        for (int index = 1; index < argc; ++index)
        {
            std::string_view argument{argv[index]};

            if (argument == "--help")
            {
                options.showHelp = true;
                return options;
            }

            if (argument == "--version")
            {
                options.showVersion = true;
                return options;
            }

            if (argument.rfind("--emit=", 0) == 0)
            {
                options.emitKind = std::string{argument.substr(7)};
                continue;
            }

            if (argument == "-o")
            {
                if (index + 1 < argc)
                {
                    options.outputPath = argv[++index];
                }
                else
                {
                    std::cerr << "bolt-ld: missing path after -o option.\n";
                    return std::nullopt;
                }
                continue;
            }

            if (!argument.empty() && argument.front() == '-')
            {
                std::cerr << "bolt-ld: unknown option '" << argument << "'.\n";
                return std::nullopt;
            }

            options.inputObjects.emplace_back(argument);
        }

        if (options.inputObjects.empty() && !options.showHelp && !options.showVersion)
        {
            std::cerr << "bolt-ld: at least one input object is required.\n";
            return std::nullopt;
        }

        return options;
    }

    static int runLinker(const CommandLineOptions& options)
    {
        std::cout << "[bolt-ld] emit: " << options.emitKind << "\n";
        if (!options.outputPath.empty())
        {
            std::cout << "[bolt-ld] output: " << options.outputPath << "\n";
        }

        for (const auto& object : options.inputObjects)
        {
            std::cout << "[bolt-ld] input: " << object << "\n";
        }

        std::cout << "[bolt-ld] (stub) no linking performed yet.\n";
        return 0;
    }
} // namespace bolt::linker

int main(int argc, char** argv)
{
    using namespace bolt::linker;

    const auto options = parseCommandLine(argc, argv);
    if (!options.has_value())
    {
        return 1;
    }

    if (options->showHelp)
    {
        printHelp();
        return 0;
    }

    if (options->showVersion)
    {
        printVersion();
        return 0;
    }

    return runLinker(*options);
}

