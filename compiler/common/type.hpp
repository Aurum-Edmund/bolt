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
} // namespace bolt::common

