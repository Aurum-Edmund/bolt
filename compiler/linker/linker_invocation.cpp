#include "linker_invocation.hpp"

namespace bolt
{
namespace linker
{
    namespace
    {
        std::string formatLibraryName(const std::string& name)
        {
            if (name.empty())
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

        std::string formatPathArgument(const std::filesystem::path& path)
        {
            return path.string();
        }

        std::filesystem::path resolveWindowsLinkerExecutable(const CommandLineOptions& options)
        {
            if (!options.sysrootPath.empty())
            {
                auto candidate = options.sysrootPath / "bin" / "link.exe";
                return candidate;
            }

            return std::filesystem::path{"link.exe"};
        }
    } // namespace

    LinkerPlanResult planLinkerInvocation(const CommandLineOptions& options)
    {
        LinkerPlanResult result;

        if (options.targetTriple != "x86_64-pc-windows-msvc")
        {
            result.hasError = true;
            result.errorMessage = "linker planning for target '" + options.targetTriple + "' is not implemented.";
            return result;
        }

        if (options.emitKind != EmitKind::Executable)
        {
            result.hasError = true;
            result.errorMessage = "emit kind is not supported for Windows linker planning.";
            return result;
        }

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
            invocation.arguments.emplace_back(formatLibraryName(library));
        }

        result.invocation = std::move(invocation);
        return result;
    }
} // namespace linker
} // namespace bolt

