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
#include "../middle_ir/passes/live_enforcement.hpp"
#include "../middle_ir/passes/ssa_conversion.hpp"
#include "command_line.hpp"
#include "import_bundle_writer.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifndef BOLT_BUILD_PROFILE
#define BOLT_BUILD_PROFILE "Stage0-local"
#endif

namespace bolt
{
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
                  << "  --import-root <path>   Add a directory to import search roots (repeatable).\n"
                  << "  --import-root=<path>   Add a directory to import search roots (shorthand).\n"
                  << "  --emit-import-bundle=<path> Write resolved import metadata to the given path.\n"
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

        std::vector<mir::LiveDiagnostic> liveDiagnostics;
        if (!mir::enforceLive(mirModule, liveDiagnostics))
        {
            std::cerr << "BOLT-E4100 Mir live enforcement failed: module '" << module.moduleName << "'.\n";
            for (const auto& diagnostic : liveDiagnostics)
            {
                std::cerr << diagnostic.code << " live invariant violation in function '" << diagnostic.functionName
                          << "': " << diagnostic.detail << "\n";
            }
            return false;
        }

        std::vector<mir::passes::SsaDiagnostic> ssaDiagnostics;
        if (!mir::passes::convertToSsa(mirModule, ssaDiagnostics))
        {
            std::cerr << "BOLT-E4300 MirSsaConversionFailed: module '" << mirModule.canonicalModulePath << "'.\n";
            for (const auto& diagnostic : ssaDiagnostics)
            {
                std::cerr << diagnostic.code << " ssa invariant violation in function '" << diagnostic.functionName
                          << "': " << diagnostic.detail << "\n";
            }
            return false;
        }
        mirModule.resolvedImports.clear();
        for (const auto& entry : resolvedImports.imports)
        {
            if (entry.status == hir::ImportStatus::Resolved)
            {
                mir::Module::ResolvedImport resolved;
                resolved.modulePath = entry.modulePath;
                if (entry.canonicalModulePath.has_value())
                {
                    resolved.canonicalModulePath = entry.canonicalModulePath;
                }
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
        if (options.importBundleOutputPath.has_value() && options.inputPaths.size() > 1)
        {
            std::cerr << "BOLT-E3002 ImportBundleSingleInput: --emit-import-bundle requires a single input module.\n";
            return 1;
        }

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
                    if (diagnostic.fixItHint.has_value())
                    {
                        std::cerr << "    fix-it: " << *diagnostic.fixItHint << '\n';
                    }
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
                        if (diagnostic.fixItHint.has_value())
                        {
                            std::cerr << "    fix-it: " << *diagnostic.fixItHint << '\n';
                        }
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
                        searchRoots.reserve(options.importRoots.size() + 1);
                        auto appendRoot = [&searchRoots](const std::filesystem::path& candidate) {
                            if (candidate.empty())
                            {
                                return;
                            }

                            std::error_code ec;
                            std::filesystem::path normalised = candidate;
                            if (normalised.is_relative())
                            {
                                normalised = std::filesystem::absolute(normalised, ec);
                                if (ec)
                                {
                                    return;
                                }
                            }
                            normalised = normalised.lexically_normal();
                            if (std::find(searchRoots.begin(), searchRoots.end(), normalised) == searchRoots.end())
                            {
                                searchRoots.emplace_back(std::move(normalised));
                            }
                        };

                        for (const auto& root : options.importRoots)
                        {
                            appendRoot(std::filesystem::path(root));
                        }

                        std::error_code inputPathError;
                        std::filesystem::path inputPathFs = std::filesystem::absolute(std::filesystem::path(path), inputPathError);
                        if (inputPathError)
                        {
                            inputPathFs = std::filesystem::path(path).lexically_normal();
                        }
                        else
                        {
                            inputPathFs = inputPathFs.lexically_normal();
                        }

                        if (inputPathFs.has_parent_path())
                        {
                            appendRoot(inputPathFs.parent_path());
                        }

                        if (!searchRoots.empty())
                        {
                            moduleLocator.setSearchRoots(searchRoots);
                            hir::ModuleLocatorDiscoveryResult discovery = moduleLocator.discoverModules();

                            if (!discovery.discoveredModules.empty())
                            {
                                std::cout << "[notice] Module locator discovered "
                                          << discovery.discoveredModules.size()
                                          << " module(s) from import roots.\n";
                            }

                            if (!discovery.issues.empty())
                            {
                                for (const auto& issue : discovery.issues)
                                {
                                    std::cerr << "BOLT-E2225 ModuleLocatorImportRoot: "
                                              << issue.message
                                              << " -> '" << issue.path.string() << "'\n";
                                }
                                exitCode = 1;
                            }

                            if (!discovery.duplicates.empty())
                            {
                                for (const auto& duplicate : discovery.duplicates)
                                {
                                    std::cerr << "BOLT-E2226 ModuleLocatorDuplicate: Module '"
                                              << duplicate.canonicalPath
                                              << "' resolves to both '"
                                              << duplicate.existingPath.string()
                                              << "' and '"
                                              << duplicate.duplicatePath.string()
                                              << "'.\n";
                                }
                                exitCode = 1;
                            }
                        }

                        if (exitCode == 0)
                        {
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
                                        std::string detail = entry.modulePath;
                                        if (entry.canonicalModulePath.has_value())
                                        {
                                            detail += " [" + *entry.canonicalModulePath + "]";
                                        }
                                        if (entry.resolvedFilePath.has_value())
                                        {
                                            detail += " -> " + *entry.resolvedFilePath;
                                        }
                                        resolvedImports.emplace_back(std::move(detail));
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

                                bool pipelineSuccess = runMirPipeline(boundModule, resolution, options);

                                if (pipelineSuccess && options.importBundleOutputPath.has_value())
                                {
                                    std::string errorMessage;
                                    if (!writeImportBundle(*options.importBundleOutputPath, boundModule, resolution, errorMessage))
                                    {
                                        std::cerr << "BOLT-E3003 ImportBundleWriteFailed: " << errorMessage << "\n";
                                        pipelineSuccess = false;
                                    }
                                    else
                                    {
                                        std::cout << "[notice] Import bundle written to "
                                                  << *options.importBundleOutputPath << "\n";
                                    }
                                }

                                if (!pipelineSuccess)
                                {
                                    exitCode = 1;
                                }
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









