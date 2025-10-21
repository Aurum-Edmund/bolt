#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bolt
{
    struct CommandLineOptions
    {
        std::vector<std::string> inputPaths;
        std::vector<std::string> importRoots;
        std::string outputPath;
        std::string targetTriple{"x64-freestanding"};
        std::string emitKind{"obj"};
        bool showHelp{false};
        bool showVersion{false};
        bool dumpMir{true};
        bool showMirHash{false};
        std::optional<std::string> mirCanonicalPath;
        std::optional<std::string> importBundleOutputPath;
    };

    class CommandLineParser
    {
    public:
        std::optional<CommandLineOptions> parse(int argc, char** argv) const
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

                if (argument.rfind("--target=", 0) == 0)
                {
                    options.targetTriple = std::string{argument.substr(9)};
                    continue;
                }

                if (argument == "--dump-mir")
                {
                    options.dumpMir = true;
                    continue;
                }

                if (argument == "--no-dump-mir")
                {
                    options.dumpMir = false;
                    continue;
                }

                if (argument == "--show-mir-hash")
                {
                    options.showMirHash = true;
                    continue;
                }

                if (argument.rfind("--emit-mir-canonical=", 0) == 0)
                {
                    constexpr std::string_view canonicalOpt = "--emit-mir-canonical=";
                    options.mirCanonicalPath = std::string{argument.substr(canonicalOpt.size())};
                    continue;
                }

                if (argument.rfind("--import-root=", 0) == 0)
                {
                    options.importRoots.emplace_back(std::string{argument.substr(14)});
                    continue;
                }

                if (argument == "--import-root")
                {
                    if (index + 1 < argc)
                    {
                        options.importRoots.emplace_back(std::string{argv[++index]});
                    }
                    else
                    {
                        std::cerr << "BOLT-E1003 MissingImportRoot: expected path after --import-root option.\n";
                        return std::nullopt;
                    }
                    continue;
                }

                if (argument.rfind("--emit-import-bundle=", 0) == 0)
                {
                    constexpr std::string_view bundleOpt = "--emit-import-bundle=";
                    options.importBundleOutputPath = std::string{argument.substr(bundleOpt.size())};
                    continue;
                }

                if (argument == "--emit-import-bundle")
                {
                    if (index + 1 < argc)
                    {
                        options.importBundleOutputPath = std::string{argv[++index]};
                    }
                    else
                    {
                        std::cerr << "BOLT-E1004 MissingImportBundle: expected path after --emit-import-bundle option.\n";
                        return std::nullopt;
                    }
                    continue;
                }

                if (argument.rfind("-o", 0) == 0)
                {
                    if (argument.size() > 2)
                    {
                        options.outputPath = std::string{argument.substr(2)};
                    }
                    else if (index + 1 < argc)
                    {
                        options.outputPath = std::string{argv[++index]};
                    }
                    else
                    {
                        std::cerr << "BOLT-E1000 MissingOutput: expected path after -o option.\n";
                        return std::nullopt;
                    }

                    continue;
                }

                if (!argument.empty() && argument[0] == '-')
                {
                    std::cerr << "BOLT-E1001 UnknownOption: unrecognised option '" << argument << "'.\n";
                    return std::nullopt;
                }

                options.inputPaths.emplace_back(argument);
            }

            if (!options.showHelp && !options.showVersion && options.inputPaths.empty())
            {
                std::cerr << "BOLT-E1002 MissingInput: at least one input file is required.\n";
                return std::nullopt;
            }

            return options;
        }
    };
} // namespace bolt
