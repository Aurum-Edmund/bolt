#include "../frontend/lexer.hpp"
#include "../frontend/parser.hpp"
#include "../high_level_ir/binder.hpp"
#include "../high_level_ir/import_resolver.hpp"
#include "../high_level_ir/module_locator.hpp"
#include "../middle_ir/module.hpp"
#include "../middle_ir/builder.hpp"
#include "../middle_ir/printer.hpp"
#include "../middle_ir/lowering.hpp"
#include "../middle_ir/verifier.hpp"
#include "../middle_ir/canonical.hpp"

#include <algorithm>
#include <filesystem>
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
        bool dumpMir{true};
        bool showMirHash{false};
        std::optional<std::string> mirCanonicalPath;
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
                  << "  --dump-mir             Emit MIR lowering output (default).\n"
                  << "  --no-dump-mir          Suppress MIR debug output.\n"
                  << "  --show-mir-hash        Print MIR canonical hash after lowering.\n"
                  << "  --emit-mir-canonical=<path> Write MIR canonical dump to the given path.\n"
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

    bool runMirPipeline(const hir::Module& module, const hir::ImportResolutionResult& resolvedImports, const CommandLineOptions& options)
    {
        mir::Module mirModule = mir::lowerFromHir(module);
        mirModule.resolvedImports.clear();
        for (const auto& entry : resolvedImports.imports)
        {
            if (entry.status == hir::ImportStatus::Resolved)
            {
                mir::Module::ResolvedImport resolved;
                resolved.modulePath = entry.modulePath;
                if (entry.resolvedFilePath.has_value())
                {
                    resolved.filePath = entry.resolvedFilePath;
                }
                mirModule.resolvedImports.emplace_back(std::move(resolved));
            }
        }

        if (!mir::verify(mirModule))
        {
            std::cerr << "BOLT-E4000 MirVerificationFailed: module '" << mirModule.canonicalModulePath << "'.\n";
            return false;
        }

        std::cout << "[notice] MIR module " << mirModule.canonicalModulePath
                  << " lowered with " << mirModule.functions.size() << " functions.\n";
        if (options.dumpMir)
        {
            std::cout << "[debug] MIR dump:\n";
            mir::print(mirModule, std::cout);
        }
        else
        {
            std::cout << "[debug] MIR dump suppressed (--no-dump-mir).\n";
        }

        if (options.showMirHash || options.mirCanonicalPath.has_value())
        {
            const std::string canonical = mir::canonicalPrint(mirModule);
            const std::uint64_t hash = mir::canonicalHash(mirModule);

            if (options.showMirHash)
            {
                std::cout << "[debug] MIR canonical hash: 0x" << std::hex << hash << std::dec << '\n';
            }

            if (options.mirCanonicalPath.has_value())
            {
                std::ofstream out(*options.mirCanonicalPath, std::ios::binary);
                if (!out)
                {
                    std::cerr << "BOLT-W4001 MirCanonicalWriteFailed: '" << *options.mirCanonicalPath << "'.\n";
                }
                else
                {
                    out << canonical;
                    std::cout << "[notice] MIR canonical written to " << *options.mirCanonicalPath << '\n';
                }
            }
        }
        return true;
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
                                  << ", imports: " << boundModule.imports.size()
                                  << ").\n";

                        hir::ModuleLocator moduleLocator;
                        std::vector<std::filesystem::path> searchRoots;
                        std::filesystem::path inputPathFs = std::filesystem::path(path).lexically_normal();
                        if (inputPathFs.has_parent_path())
                        {
                            searchRoots.emplace_back(inputPathFs.parent_path());
                        }
                        if (!searchRoots.empty())
                        {
                            moduleLocator.setSearchRoots(std::move(searchRoots));
                        }

                        std::string canonicalModulePath = boundModule.moduleName;
                        if (!boundModule.packageName.empty())
                        {
                            moduleLocator.registerModule(boundModule.packageName, inputPathFs);
                            if (boundModule.packageName != boundModule.moduleName)
                            {
                                canonicalModulePath = boundModule.packageName + "::" + boundModule.moduleName;
                            }
                        }
                        moduleLocator.registerModule(boundModule.moduleName, inputPathFs);
                        moduleLocator.registerModule(canonicalModulePath, inputPathFs);

                        hir::ImportResolver importResolver;
                        importResolver.setModuleLocator(&moduleLocator);
                        hir::ImportResolutionResult resolution = importResolver.resolve(boundModule);
                        const auto& importDiagnostics = importResolver.diagnostics();

                        if (!importDiagnostics.empty())
                        {
                            for (const auto& diagnostic : importDiagnostics)
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
                            std::vector<std::string> pendingImports;
                            std::vector<std::string> resolvedImports;
                            for (const auto& entry : resolution.imports)
                            {
                                if (entry.status == hir::ImportStatus::Pending)
                                {
                                    pendingImports.emplace_back(entry.modulePath);
                                }
                                else if (entry.status == hir::ImportStatus::Resolved)
                                {
                                    if (entry.resolvedFilePath.has_value())
                                    {
                                        resolvedImports.emplace_back(entry.modulePath + " -> " + *entry.resolvedFilePath);
                                    }
                                    else
                                    {
                                        resolvedImports.emplace_back(entry.modulePath);
                                    }
                                }
                            }

                            if (!resolvedImports.empty())
                            {
                                std::cout << "[notice] Resolved imports (" << resolvedImports.size() << "):\n";
                                for (const auto& entry : resolvedImports)
                                {
                                    std::cout << "    " << entry << '\n';
                                }
                            }

                            if (!pendingImports.empty())
                            {
                                std::cout << "[notice] Pending import resolution for "
                                          << pendingImports.size() << " module(s):\n";
                                for (const auto& name : pendingImports)
                                {
                                    std::cout << "    " << name << '\n';
                                }
                            }

                            if (!runMirPipeline(boundModule, resolution, options))
                            {
                                exitCode = 1;
                            }
                        }
                    }
                }
            }
        }

        if (exitCode == 0)
        {
            std::cout << "[notice] Front-end and MIR stages completed.\n";
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

    return bolt::runCompiler(options.value());
}






