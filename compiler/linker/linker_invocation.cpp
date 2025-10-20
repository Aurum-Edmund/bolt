#include "linker_invocation.hpp"

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
            return planAirInvocation(options);
        }

        LinkerPlanResult result;
        result.hasError = true;
        result.errorMessage = "linker planning for target '" + options.targetTriple + "' is not implemented.";
        return result;
    }
} // namespace linker
} // namespace bolt

