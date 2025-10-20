#include "linker_invocation.hpp"

#include <system_error>
#include <string_view>
#include <utility>

namespace bolt
{
namespace linker
{
    namespace
    {
        bool hasPathSeparator(const std::string& value)
        {
            return value.find('/') != std::string::npos || value.find('\\') != std::string::npos;
        }

        std::string formatWindowsLibraryName(const std::string& name)
        {
            if (name.empty())
            {
                return name;
            }

            if (hasPathSeparator(name))
            {
                return name;
            }

            auto hasExtension = name.find('.') != std::string::npos;
            if (hasExtension)
            {
                return name;
            }

            return name + ".lib";
        }

        std::string formatAirLibraryArgument(const std::string& name)
        {
            if (name.empty())
            {
                return name;
            }

            if (hasPathSeparator(name))
            {
                return name;
            }

            if (name.rfind("-l", 0) == 0)
            {
                return name;
            }

            if (name.find('.') != std::string::npos)
            {
                return name;
            }

            return "-l" + name;
        }

        std::string formatPathArgument(const std::filesystem::path& path)
        {
            return path.string();
        }

        std::filesystem::path resolveWindowsLinkerExecutable(const CommandLineOptions& options)
        {
            if (!options.sysrootPath.empty())
            {
                return options.sysrootPath / "bin" / "link.exe";
            }

            return std::filesystem::path{"link.exe"};
        }

        std::filesystem::path resolveWindowsLibraryManagerExecutable(const CommandLineOptions& options)
        {
            if (!options.sysrootPath.empty())
            {
                return options.sysrootPath / "bin" / "lib.exe";
            }

            return std::filesystem::path{"lib.exe"};
        }

        std::filesystem::path resolveAirLinkerExecutable(const CommandLineOptions& options)
        {
            if (!options.sysrootPath.empty())
            {
                return options.sysrootPath / "bin" / "ld.lld";
            }

            return std::filesystem::path{"ld.lld"};
        }

        std::filesystem::path resolveAirArchiverExecutable(const CommandLineOptions& options)
        {
            if (!options.sysrootPath.empty())
            {
                return options.sysrootPath / "bin" / "llvm-ar";
            }

            return std::filesystem::path{"llvm-ar"};
        }

        LinkerPlanResult planWindowsExecutableInvocation(const CommandLineOptions& options)
        {
            LinkerPlanResult result;

            LinkerInvocation invocation;
            invocation.executable = resolveWindowsLinkerExecutable(options);

            invocation.arguments.emplace_back("/NOLOGO");

            if (!options.outputPath.empty())
            {
                invocation.arguments.emplace_back("/OUT:" + options.outputPath.string());
            }

            if (!options.runtimeRootPath.empty())
            {
                invocation.arguments.emplace_back("/LIBPATH:" + options.runtimeRootPath.string());
            }

            for (const auto& searchPath : options.librarySearchPaths)
            {
                invocation.arguments.emplace_back("/LIBPATH:" + searchPath.string());
            }

            for (const auto& object : options.inputObjects)
            {
                invocation.arguments.emplace_back(formatPathArgument(object));
            }

            for (const auto& library : options.libraries)
            {
                invocation.arguments.emplace_back(formatWindowsLibraryName(library));
            }

            result.invocation = std::move(invocation);
            return result;
        }

        LinkerPlanResult planWindowsStaticLibraryInvocation(const CommandLineOptions& options)
        {
            LinkerPlanResult result;

            LinkerInvocation invocation;
            invocation.executable = resolveWindowsLibraryManagerExecutable(options);

            invocation.arguments.emplace_back("/NOLOGO");

            if (!options.outputPath.empty())
            {
                invocation.arguments.emplace_back("/OUT:" + options.outputPath.string());
            }

            if (!options.runtimeRootPath.empty())
            {
                invocation.arguments.emplace_back("/LIBPATH:" + options.runtimeRootPath.string());
            }

            for (const auto& searchPath : options.librarySearchPaths)
            {
                invocation.arguments.emplace_back("/LIBPATH:" + searchPath.string());
            }

            for (const auto& object : options.inputObjects)
            {
                invocation.arguments.emplace_back(formatPathArgument(object));
            }

            for (const auto& library : options.libraries)
            {
                invocation.arguments.emplace_back(formatWindowsLibraryName(library));
            }

            result.invocation = std::move(invocation);
            return result;
        }

        LinkerPlanResult planAirInvocation(const CommandLineOptions& options)
        {
            LinkerPlanResult result;

            if (options.emitKind != EmitKind::AirImage)
            {
                result.hasError = true;
                result.errorMessage = "emit kind is not supported for Air linker planning.";
                return result;
            }

            if (options.linkerScriptPath.empty())
            {
                result.hasError = true;
                result.errorMessage = "linker script is required when targeting x86_64-air-bolt.";
                return result;
            }

            LinkerInvocation invocation;
            invocation.executable = resolveAirLinkerExecutable(options);

            invocation.arguments.emplace_back("-nostdlib");
            invocation.arguments.emplace_back("-static");
            invocation.arguments.emplace_back("--gc-sections");
            invocation.arguments.emplace_back("--no-undefined");

            if (!options.outputPath.empty())
            {
                invocation.arguments.emplace_back("-o");
                invocation.arguments.emplace_back(options.outputPath.string());
            }

            invocation.arguments.emplace_back("-T");
            invocation.arguments.emplace_back(options.linkerScriptPath.string());
            invocation.arguments.emplace_back("-e");
            invocation.arguments.emplace_back("_start");

            if (!options.runtimeRootPath.empty())
            {
                invocation.arguments.emplace_back("-L" + options.runtimeRootPath.string());
            }

            for (const auto& searchPath : options.librarySearchPaths)
            {
                invocation.arguments.emplace_back("-L" + searchPath.string());
            }

            for (const auto& object : options.inputObjects)
            {
                invocation.arguments.emplace_back(formatPathArgument(object));
            }

            for (const auto& library : options.libraries)
            {
                invocation.arguments.emplace_back(formatAirLibraryArgument(library));
            }

            result.invocation = std::move(invocation);
            return result;
        }

        LinkerPlanResult planAirArchiveInvocation(const CommandLineOptions& options)
        {
            LinkerPlanResult result;

            if (options.emitKind != EmitKind::BoltArchive)
            {
                result.hasError = true;
                result.errorMessage = "emit kind is not supported for Air archiver planning.";
                return result;
            }

            if (!options.librarySearchPaths.empty())
            {
                result.hasError = true;
                result.errorMessage
                    = "library search paths (-L) are not supported when emitting Bolt archives.";
                return result;
            }

            if (!options.libraries.empty())
            {
                result.hasError = true;
                result.errorMessage
                    = "link libraries (-l) are not supported when emitting Bolt archives; "
                      "list the archive inputs explicitly.";
                return result;
            }

            LinkerInvocation invocation;
            invocation.executable = resolveAirArchiverExecutable(options);

            invocation.arguments.emplace_back("rcs");
            invocation.arguments.emplace_back(options.outputPath.string());

            for (const auto& object : options.inputObjects)
            {
                invocation.arguments.emplace_back(formatPathArgument(object));
            }

            result.invocation = std::move(invocation);
            return result;
        }
    } // namespace

    LinkerPlanResult planLinkerInvocation(const CommandLineOptions& options)
    {
        if (options.targetTriple == "x86_64-pc-windows-msvc")
        {
            if (options.emitKind == EmitKind::Executable)
            {
                return planWindowsExecutableInvocation(options);
            }

            if (options.emitKind == EmitKind::StaticLibrary)
            {
                return planWindowsStaticLibraryInvocation(options);
            }

            LinkerPlanResult result;
            result.hasError = true;
            result.errorMessage = "emit kind is not supported for Windows linker planning.";
            return result;
        }

        if (options.targetTriple == "x86_64-air-bolt")
        {
            if (options.emitKind == EmitKind::AirImage)
            {
                return planAirInvocation(options);
            }

            if (options.emitKind == EmitKind::BoltArchive)
            {
                return planAirArchiveInvocation(options);
            }

            LinkerPlanResult result;
            result.hasError = true;
            result.errorMessage = "emit kind is not supported for Air linker planning.";
            return result;
        }

        LinkerPlanResult result;
        result.hasError = true;
        result.errorMessage = "linker planning for target '" + options.targetTriple + "' is not implemented.";
        return result;
    }

    namespace
    {
        LinkerValidationResult createValidationError(std::string message)
        {
            LinkerValidationResult result;
            result.hasError = true;
            result.errorMessage = std::move(message);
            return result;
        }
    } // namespace

    LinkerValidationResult validateLinkerInputs(const CommandLineOptions& options, bool skipInputObjectValidation)
    {
        auto requireRegularFile = [](const std::filesystem::path& path, std::string_view description)
        {
            std::error_code ec;
            auto status = std::filesystem::status(path, ec);
            if (ec || status.type() == std::filesystem::file_type::not_found)
            {
                return createValidationError(std::string(description) + " '" + path.string() + "' was not found.");
            }

            if (status.type() != std::filesystem::file_type::regular)
            {
                return createValidationError(std::string(description) + " '" + path.string() + "' is not a file.");
            }

            return LinkerValidationResult{};
        };

        auto requireDirectory = [](const std::filesystem::path& path, std::string_view description)
        {
            std::error_code ec;
            auto status = std::filesystem::status(path, ec);
            if (ec || status.type() == std::filesystem::file_type::not_found)
            {
                return createValidationError(std::string(description) + " '" + path.string() + "' was not found.");
            }

            if (status.type() != std::filesystem::file_type::directory)
            {
                return createValidationError(std::string(description) + " '" + path.string() + "' is not a directory.");
            }

            return LinkerValidationResult{};
        };

        auto checkResult = LinkerValidationResult{};

        if (!options.linkerScriptPath.empty())
        {
            checkResult = requireRegularFile(options.linkerScriptPath, "linker script");
            if (checkResult.hasError)
            {
                return checkResult;
            }
        }

        if (options.emitKind == EmitKind::BoltArchive)
        {
            if (!options.librarySearchPaths.empty())
            {
                return createValidationError(
                    "library search paths (-L) are not supported when emitting Bolt archives.");
            }

            if (!options.libraries.empty())
            {
                return createValidationError(
                    "link libraries (-l) are not supported when emitting Bolt archives; list the archive inputs explicitly.");
            }
        }

        if (!options.importBundlePath.empty())
        {
            checkResult = requireRegularFile(options.importBundlePath, "import bundle");
            if (checkResult.hasError)
            {
                return checkResult;
            }
        }

        if (!options.runtimeRootPath.empty())
        {
            checkResult = requireDirectory(options.runtimeRootPath, "runtime root");
            if (checkResult.hasError)
            {
                return checkResult;
            }
        }

        for (const auto& libraryPath : options.librarySearchPaths)
        {
            checkResult = requireDirectory(libraryPath, "library search path");
            if (checkResult.hasError)
            {
                return checkResult;
            }
        }

        if (!options.outputPath.empty())
        {
            auto parentPath = options.outputPath.parent_path();
            if (!parentPath.empty())
            {
                checkResult = requireDirectory(parentPath, "output directory");
                if (checkResult.hasError)
                {
                    return checkResult;
                }
            }
        }

        if (!skipInputObjectValidation)
        {
            for (const auto& objectPath : options.inputObjects)
            {
                checkResult = requireRegularFile(objectPath, "input object");
                if (checkResult.hasError)
                {
                    return checkResult;
                }
            }
        }

        return LinkerValidationResult{};
    }
} // namespace linker
} // namespace bolt

