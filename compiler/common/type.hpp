#pragma once

#include "../frontend/token.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bolt::common
{
    struct QualifiedName
    {
        std::vector<std::string> components;

        [[nodiscard]] bool empty() const noexcept
        {
            return components.empty();
        }

        [[nodiscard]] std::string toString(std::string_view separator = ".") const
        {
            if (components.empty())
            {
                return std::string{};
            }

            std::string result = components.front();
            for (std::size_t index = 1; index < components.size(); ++index)
            {
                result.append(separator);
                result.append(components[index]);
            }
            return result;
        }

        [[nodiscard]] std::string_view last() const noexcept
        {
            if (components.empty())
            {
                return std::string_view{};
            }
            return components.back();
        }
    };

    enum class TypeKind
    {
        Invalid,
        Named,
        Pointer,
        Reference,
        Array
    };

    struct TypeReference
    {
        TypeKind kind{TypeKind::Invalid};
        QualifiedName name;
        std::vector<TypeReference> genericArguments;
        std::vector<std::string> qualifiers;
        std::optional<std::uint64_t> arrayLength;
        bool isBuiltin{false};
        std::string text;
        std::string originalText;
        std::string normalizedText;
        bolt::frontend::SourceSpan span{};

        [[nodiscard]] bool isValid() const noexcept
        {
            return kind != TypeKind::Invalid;
        }

        [[nodiscard]] bool isPointer() const noexcept
        {
            return kind == TypeKind::Pointer;
        }

        [[nodiscard]] bool isReference() const noexcept
        {
            return kind == TypeKind::Reference;
        }

        [[nodiscard]] bool isArray() const noexcept
        {
            return kind == TypeKind::Array;
        }

        [[nodiscard]] bool isGeneric() const noexcept
        {
            return !genericArguments.empty();
        }

        [[nodiscard]] std::string qualifiedName(std::string_view separator = ".") const
        {
            return name.toString(separator);
        }

        [[nodiscard]] std::string_view baseName() const noexcept
        {
            return name.last();
        }

        [[nodiscard]] bool hasQualifier(std::string_view qualifier) const noexcept
        {
            return std::find(qualifiers.begin(), qualifiers.end(), qualifier) != qualifiers.end();
        }
    };

    namespace detail
    {
        inline std::string joinQualifiers(const std::vector<std::string>& qualifiers)
        {
            if (qualifiers.empty())
            {
                return {};
            }

            std::string result = qualifiers.front();
            for (std::size_t index = 1; index < qualifiers.size(); ++index)
            {
                result.push_back(' ');
                result += qualifiers[index];
            }
            return result;
        }
    } // namespace detail

    inline std::string buildNormalizedTypeText(const TypeReference& type)
    {
        if (type.kind == TypeKind::Invalid)
        {
            return {};
        }

        std::string result;

        if (!type.qualifiers.empty())
        {
            result = detail::joinQualifiers(type.qualifiers);
            result.push_back(' ');
        }

        if (type.kind == TypeKind::Array)
        {
            if (!type.genericArguments.empty())
            {
                result += buildNormalizedTypeText(type.genericArguments.front());
            }
            else
            {
                result += type.text;
            }

            result.push_back('[');
            if (type.arrayLength.has_value())
            {
                result += std::to_string(*type.arrayLength);
            }
            result.push_back(']');
            return result;
        }

        const std::string qualified = type.qualifiedName();
        if (!qualified.empty())
        {
            result += qualified;
        }
        else
        {
            result += type.text;
        }

        if (!type.genericArguments.empty())
        {
            result.push_back('<');
            for (std::size_t index = 0; index < type.genericArguments.size(); ++index)
            {
                if (index > 0)
                {
                    result += ", ";
                }
                result += buildNormalizedTypeText(type.genericArguments[index]);
            }
            result.push_back('>');
        }

        return result;
    }

    inline void populateNormalizedTypeText(TypeReference& type)
    {
        for (auto& argument : type.genericArguments)
        {
            populateNormalizedTypeText(argument);
        }

        type.normalizedText = buildNormalizedTypeText(type);
    }
} // namespace bolt::common

