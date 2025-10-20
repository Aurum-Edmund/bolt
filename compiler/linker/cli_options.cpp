#include "cli_options.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace bolt
{
namespace linker
{
    namespace
    {
        std::optional<EmitKind> parseEmitKind(std::string_view value)
        {
            if (value == "exe")
            {
                return EmitKind::Executable;
            }
            if (value == "lib" || value == "link")
            {
                return EmitKind::StaticLibrary;
            }
            if (value == "air")
            {
                return EmitKind::AirImage;
            }
            if (value == "zap")
            {
                return EmitKind::BoltArchive;
            }
            return std::nullopt;
        }

        const char* emitKindToString(EmitKind kind)
        {
            switch (kind)
            {
            case EmitKind::Executable:
                return "exe";
            case EmitKind::StaticLibrary:
                return "lib";
            case EmitKind::AirImage:
                return "air";
            case EmitKind::BoltArchive:
                return "zap";
            }
            return "unknown";
        }

        std::optional<std::string_view> parseOptionWithValue(std::string_view argument,
            std::string_view name,
            int& index,
            int argc,
            char** argv,
            CommandLineParseResult& result)
        {
            if (argument == name)
            {
                if (index + 1 >= argc)
                {
                    result.hasError = true;
                    result.errorMessage = std::string{name} + " requires a value.";
                    return std::nullopt;
                }
                return std::string_view{argv[++index]};
            }

            if (argument.rfind(name, 0) == 0 && argument.size() > name.size() && argument[name.size()] == '=')
            {
                return argument.substr(name.size() + 1);
            }

            return std::nullopt;
        }

        std::optional<std::string_view> parseShortOptionWithValue(std::string_view argument,
            char name,
            int& index,
            int argc,
            char** argv,
            CommandLineParseResult& result)
        {
            if (argument.size() >= 2 && argument[0] == '-' && argument[1] == name)
            {
                if (argument.size() > 2)
                {
                    return argument.substr(2);
                }

                if (index + 1 >= argc)
                {
                    result.hasError = true;
                    result.errorMessage = std::string{"-"} + name + " requires a value.";
                    return std::nullopt;
                }

                return std::string_view{argv[++index]};
            }

            return std::nullopt;
        }

        bool isRecognizedTarget(std::string_view target)
        {
            return target == "x86_64-pc-windows-msvc" || target == "x86_64-air-bolt";
        }
    } // namespace

    const char* toString(EmitKind kind)
    {
        return emitKindToString(kind);
    }

    CommandLineParseResult parseCommandLine(int argc, char** argv)
    {
        CommandLineParseResult result;
        if (argc <= 1)
        {
            result.hasError = true;
            result.errorMessage = "at least one input object is required.";
            return result;
        }

        bool targetExplicitlySet = false;

        for (int index = 1; index < argc; ++index)
        {
            std::string_view argument{argv[index]};

            if (argument == "--help")
            {
                result.showHelp = true;
                return result;
            }

            if (argument == "--version")
            {
                result.showVersion = true;
                return result;
            }

            if (argument == "--verbose")
            {
                result.options.verbose = true;
                continue;
            }

            if (argument == "--dry-run")
            {
                result.options.dryRun = true;
                continue;
            }

            if (auto emitValue = parseOptionWithValue(argument, "--emit", index, argc, argv, result))
            {
                if (result.hasError)
                {
                    return result;
                }

                if (auto emitKind = parseEmitKind(*emitValue))
                {
                    result.options.emitKind = *emitKind;
                    continue;
                }

                result.hasError = true;
                result.errorMessage = "unknown emit kind '" + std::string{*emitValue} + "'.";
                return result;
            }

            if (auto targetValue = parseOptionWithValue(argument, "--target", index, argc, argv, result))
            {
                if (result.hasError)
                {
                    return result;
                }

                if (!isRecognizedTarget(*targetValue))
                {
                    result.hasError = true;
                    result.errorMessage = "unsupported target '" + std::string{*targetValue} + "'.";
                    return result;
                }

                result.options.targetTriple = std::string{*targetValue};
                targetExplicitlySet = true;
                continue;
            }

            if (auto sysrootValue = parseOptionWithValue(argument, "--sysroot", index, argc, argv, result))
            {
                if (result.hasError)
                {
                    return result;
                }
                result.options.sysrootPath = std::filesystem::path{*sysrootValue};
                continue;
            }

            if (auto runtimeValue = parseOptionWithValue(argument, "--runtime-root", index, argc, argv, result))
            {
                if (result.hasError)
                {
                    return result;
                }
                result.options.runtimeRootPath = std::filesystem::path{*runtimeValue};
                continue;
            }

            if (auto scriptValue = parseOptionWithValue(argument, "--linker-script", index, argc, argv, result))
            {
                if (result.hasError)
                {
                    return result;
                }
                result.options.linkerScriptPath = std::filesystem::path{*scriptValue};
                continue;
            }

            if (auto bundleValue = parseOptionWithValue(argument, "--import-bundle", index, argc, argv, result))
            {
                if (result.hasError)
                {
                    return result;
                }
                result.options.importBundlePath = std::filesystem::path{*bundleValue};
                continue;
            }

            if (auto entryValue = parseOptionWithValue(argument, "--entry", index, argc, argv, result))
            {
                if (result.hasError)
                {
                    return result;
                }

                if (entryValue->empty())
                {
                    result.hasError = true;
                    result.errorMessage = "--entry requires a symbol name.";
                    return result;
                }

                result.options.entryPoint = std::string{*entryValue};
                continue;
            }

            if (auto outputValue = parseShortOptionWithValue(argument, 'o', index, argc, argv, result))
            {
                if (result.hasError)
                {
                    return result;
                }
                result.options.outputPath = std::filesystem::path{*outputValue};
                continue;
            }

            if (auto libraryPath = parseShortOptionWithValue(argument, 'L', index, argc, argv, result))
            {
                if (result.hasError)
                {
                    return result;
                }
                result.options.librarySearchPaths.emplace_back(*libraryPath);
                continue;
            }

            if (auto library = parseShortOptionWithValue(argument, 'l', index, argc, argv, result))
            {
                if (result.hasError)
                {
                    return result;
                }
                result.options.libraries.emplace_back(*library);
                continue;
            }

            if (!argument.empty() && argument.front() == '-')
            {
                result.hasError = true;
                result.errorMessage = "unknown option '" + std::string{argument} + "'.";
                return result;
            }

            result.options.inputObjects.emplace_back(argument);
        }

        if (!result.showHelp && !result.showVersion)
        {
            if (result.options.outputPath.empty())
            {
                result.hasError = true;
                result.errorMessage = "output path is required (use -o).";
                return result;
            }

            if (result.options.inputObjects.empty())
            {
                result.hasError = true;
                result.errorMessage = "at least one input object is required.";
                return result;
            }

            const auto& target = result.options.targetTriple;
            const bool targetsWindows = target == "x86_64-pc-windows-msvc";
            const bool targetsAir = target == "x86_64-air-bolt";

            switch (result.options.emitKind)
            {
            case EmitKind::Executable:
            case EmitKind::StaticLibrary:
                if (targetsAir)
                {
                    result.hasError = true;
                    result.errorMessage = std::string{"emit kind '"} + toString(result.options.emitKind)
                        + "' is not supported for target '" + target + "'.";
                    return result;
                }
                break;

            case EmitKind::AirImage:
            case EmitKind::BoltArchive:
                if (!targetsAir)
                {
                    if (!targetExplicitlySet && targetsWindows)
                    {
                        result.options.targetTriple = "x86_64-air-bolt";
                        break;
                    }

                    result.hasError = true;
                    result.errorMessage = std::string{"emit kind '"} + toString(result.options.emitKind)
                        + "' requires target 'x86_64-air-bolt'.";
                    return result;
                }
                break;
            }
        }

        return result;
    }
} // namespace linker
} // namespace bolt

