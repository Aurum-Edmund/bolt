#include "../front_end/lexer.hpp"
#include "../front_end/parser.hpp"
#include "../hir/binder.hpp"
#include "../mir/module.hpp"
#include "../mir/builder.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef BOLT_BUILD_PROFILE
#define BOLT_BUILD_PROFILE "Stage0-local"
#endif

namespace bolt
{
    struct CommandLineOptions
    {
        std::vector<std::string> inputPaths;
        std::string outputPath;
        std::string targetTriple{"x64-freestanding"};
        std::string emitKind{"obj"};
        bool showHelp{false};
        bool showVersion{false};
    };

    class CommandLineParser
    {
    public:
        std::optional<CommandLineOptions> parse(int argc, char** argv)
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

    void printHelp()
    {
        std::cout << "boltcc - Bolt Stage-0 compiler driver\n"
                  << "Usage: boltcc [options] <input>\n\n"
                  << "Options:\n"
                  << "  --help                 Show this help text and exit.\n"
                  << "  --version              Show version information and exit.\n"
                  << "  --emit=<kind>          Select output kind (obj, ir, bin). Default: obj.\n"
                  << "  --target=<triple>      Select target profile. Default: x64-freestanding.\n"
                  << "  -o <path>              Write output to the specified path.\n";
    }

    void printVersion()
    {
        std::cout << "boltcc Stage-0 prototype (build profile: " << BOLT_BUILD_PROFILE << ")\n";
        std::cout << "Aligned with Bolt Language v2.3 / Master Spec v3.1\n";
    }

    std::optional<std::string> loadFile(const std::string& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return std::nullopt;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        return buffer.str();
    }

    void runMirStub(const hir::Module& module)
    {
        mir::Module mirModule;
        mirModule.name = module.moduleName;

        mir::Builder builder{mirModule};

        for (const auto& function : module.functions)
        {
            auto& mirFunction = builder.createFunction(function.name);
            auto& entryBlock = builder.appendBlock(mirFunction, "entry");
            (void)builder.appendInstruction(entryBlock, mir::InstructionKind::Return);
        }

        std::cout << "[notice] MIR stub generated for module '"
                  << mirModule.name
                  << "' with " << mirModule.functions.size()
                  << " functions.\n";
    }

    int runCompiler(const CommandLineOptions& options)
    {
        std::cout << "[information] Starting boltcc stage-0 pipeline.\n";
        std::cout << "  emit: " << options.emitKind << "\n";
        std::cout << "  target: " << options.targetTriple << "\n";

        if (!options.outputPath.empty())
        {
            std::cout << "  output: " << options.outputPath << "\n";
        }

        for (const auto& path : options.inputPaths)
        {
            std::cout << "  input: " << path << "\n";
        }

        int exitCode = 0;
        for (const auto& path : options.inputPaths)
        {
            const auto content = loadFile(path);
            if (!content.has_value())
            {
                std::cerr << "BOLT-E3000 InputReadFailed: unable to open '" << path << "'.\n";
                exitCode = 1;
                continue;
            }

            frontend::Lexer lexer{*content, path};
            lexer.lex();

            const auto& diagnostics = lexer.diagnostics();
            if (!diagnostics.empty())
            {
                for (const auto& diagnostic : diagnostics)
                {
                    std::cerr << diagnostic.code << ' '
                              << "L" << diagnostic.span.begin.line << ":C" << diagnostic.span.begin.column
                              << " -> "
                              << diagnostic.message << '\n';
                }
                exitCode = 1;
            }
            else
            {
                const auto& tokens = lexer.tokens();
                std::cout << "[notice] Lexed " << tokens.size() << " tokens from '" << path << "'.\n";
                std::size_t previewCount = std::min<std::size_t>(tokens.size(), 8);
                for (std::size_t index = 0; index < previewCount; ++index)
                {
                    const auto& token = tokens[index];
                    std::cout << "    "
                              << toString(token.kind)
                              << " @ L" << token.span.begin.line << ":C" << token.span.begin.column;
                    if (!token.text.empty())
                    {
                        std::cout << " -> '" << token.text << "'";
                    }
                    std::cout << '\n';
                }

                frontend::Parser parser{tokens, path};
                frontend::CompilationUnit unit = parser.parse();
                const auto& parserDiagnostics = parser.diagnostics();

                if (!parserDiagnostics.empty())
                {
                    for (const auto& diagnostic : parserDiagnostics)
                    {
                        std::cerr << diagnostic.code << ' '
                                  << "L" << diagnostic.span.begin.line << ":C" << diagnostic.span.begin.column
                                  << " -> "
                                  << diagnostic.message << '\n';
                    }
                    exitCode = 1;
                }
                else
                {
                    std::cout << "[notice] Parsed module "
                              << unit.module.packageName << "::" << unit.module.moduleName
                              << " (functions: " << unit.functions.size()
                              << ", blueprints: " << unit.blueprints.size() << ").\n";

                    hir::Binder binder{unit, path};
                    hir::Module boundModule = binder.bind();
                    const auto& binderDiagnostics = binder.diagnostics();

                    if (!binderDiagnostics.empty())
                    {
                        for (const auto& diagnostic : binderDiagnostics)
                        {
                            std::cerr << diagnostic.code << ' '
                                      << "L" << diagnostic.span.begin.line << ":C" << diagnostic.span.begin.column
                                      << " -> "
                                      << diagnostic.message << '\n';
                        }
                        exitCode = 1;
                    }
                    else
                    {
                        std::cout << "[notice] Bound module symbols (functions: "
                                  << boundModule.functions.size()
                                  << ", blueprints: " << boundModule.blueprints.size()
                                  << ").\n";

                        runMirStub(boundModule);
                    }
                }
            }
        }

        if (exitCode == 0)
        {
            std::cout << "[notice] Front-end and MIR stub stages completed.\n";
        }
        else
        {
            std::cerr << "BOLT-W3001 Stage0 pipeline halted during front-end analysis.\n";
        }

        return exitCode;
    }
} // namespace bolt

int main(int argc, char** argv)
{
    bolt::CommandLineParser parser;
    const auto options = parser.parse(argc, argv);

    if (!options.has_value())
    {
        return 1;
    }

    if (options->showHelp)
    {
        bolt::printHelp();
        return 0;
    }

    if (options->showVersion)
    {
        bolt::printVersion();
        return 0;
    }

    return bolt::runCompiler(*options);
}
