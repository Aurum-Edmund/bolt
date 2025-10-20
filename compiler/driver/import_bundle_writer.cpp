#include "import_bundle_writer.hpp"

#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>

namespace bolt
{
    namespace
    {
        std::string_view toStatusString(hir::ImportStatus status)
        {
            switch (status)
            {
            case hir::ImportStatus::Pending:
                return "pending";
            case hir::ImportStatus::Resolved:
                return "resolved";
            case hir::ImportStatus::NotFound:
                return "notFound";
            case hir::ImportStatus::SelfImport:
                return "self";
            }

            return "unknown";
        }

        char hexDigit(unsigned value)
        {
            return static_cast<char>(value < 10 ? ('0' + value) : ('a' + (value - 10)));
        }

        std::string escapeJson(std::string_view value)
        {
            std::string result;
            result.reserve(value.size() + 8);

            for (unsigned char ch : value)
            {
                switch (ch)
                {
                case '\\':
                    result += "\\\\";
                    break;
                case '"':
                    result += "\\\"";
                    break;
                case '\b':
                    result += "\\b";
                    break;
                case '\f':
                    result += "\\f";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    if (ch < 0x20)
                    {
                        result += "\\u00";
                        result.push_back(hexDigit((ch >> 4) & 0xF));
                        result.push_back(hexDigit(ch & 0xF));
                    }
                    else
                    {
                        result.push_back(static_cast<char>(ch));
                    }
                    break;
                }
            }

            return result;
        }

        std::string computeCanonicalModuleName(const hir::Module& module)
        {
            if (module.packageName.empty() || module.packageName == module.moduleName)
            {
                return module.moduleName;
            }

            return module.packageName + "::" + module.moduleName;
        }
    } // namespace

    bool writeImportBundle(const std::filesystem::path& outputPath,
        const hir::Module& module,
        const hir::ImportResolutionResult& resolution,
        std::string& errorMessage)
    {
        std::ostringstream stream;
        const std::string canonicalName = computeCanonicalModuleName(module);

        stream << "{\n";
        stream << "  \"module\": {\n";
        stream << "    \"package\": \"" << escapeJson(module.packageName) << "\",\n";
        stream << "    \"name\": \"" << escapeJson(module.moduleName) << "\",\n";
        stream << "    \"canonical\": \"" << escapeJson(canonicalName) << "\"\n";
        stream << "  },\n";
        stream << "  \"imports\": [\n";

        for (std::size_t index = 0; index < resolution.imports.size(); ++index)
        {
            const auto& entry = resolution.imports[index];
            stream << "    {\n";
            stream << "      \"module\": \"" << escapeJson(entry.modulePath) << "\",\n";
            stream << "      \"status\": \"" << escapeJson(toStatusString(entry.status)) << "\"";

            if (entry.canonicalModulePath.has_value())
            {
                stream << ",\n";
                stream << "      \"canonical\": \"" << escapeJson(*entry.canonicalModulePath) << "\"";
            }

            if (entry.resolvedFilePath.has_value())
            {
                stream << ",\n";
                stream << "      \"file\": \"" << escapeJson(*entry.resolvedFilePath) << "\"\n";
            }
            else
            {
                stream << "\n";
            }

            stream << "    }";
            if (index + 1 < resolution.imports.size())
            {
                stream << ",";
            }
            stream << "\n";
        }

        stream << "  ]\n";
        stream << "}\n";

        const auto parentDirectory = outputPath.parent_path();
        if (!parentDirectory.empty())
        {
            std::error_code createError;
            std::filesystem::create_directories(parentDirectory, createError);
            if (createError)
            {
                errorMessage = "failed to create directories for '" + outputPath.string() + "': " + createError.message();
                return false;
            }
        }

        std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            errorMessage = "unable to open '" + outputPath.string() + "' for writing.";
            return false;
        }

        file << stream.str();
        if (!file.good())
        {
            errorMessage = "failed while writing import bundle to '" + outputPath.string() + "'.";
            return false;
        }

        return true;
    }
} // namespace bolt

