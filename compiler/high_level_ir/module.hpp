#pragma once

#include "ast.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bolt::hir
{
    using bolt::frontend::SourceSpan;

    struct TypeReference
    {
        std::string text;
        SourceSpan span;
    };

    struct Attribute
    {
        std::string name;
        std::vector<bolt::frontend::AttributeArgument> arguments;
        SourceSpan span;
    };

    struct Parameter
    {
        std::string name;
        TypeReference type;
        SourceSpan span;
        bool isLiveValue{false};
    };

    struct Function
    {
        std::string name;
        std::vector<std::string> modifiers;
        std::vector<Attribute> attributes;
        std::vector<Parameter> parameters;
        TypeReference returnType;
        bool hasReturnType{false};
        bool isInterruptHandler{false};
        bool isBareFunction{false};
        bool isPageAligned{false};
        std::optional<std::uint64_t> alignmentBytes;
        std::optional<std::string> sectionName;
        std::optional<std::uint64_t> systemRequestId;
        std::optional<std::string> intrinsicName;
        std::vector<std::string> kernelMarkers;
        bool returnIsLiveValue{false};
        SourceSpan span;
    };

    struct BlueprintField
    {
        std::string name;
        TypeReference type;
        std::vector<Attribute> attributes;
        std::optional<std::uint32_t> bitWidth;
        std::optional<std::uint64_t> alignmentBytes;
        bool isLiveValue{false};
        SourceSpan span;
    };

    struct Blueprint
    {
        std::string name;
        std::vector<std::string> modifiers;
        std::vector<Attribute> attributes;
        std::vector<BlueprintField> fields;
        bool isPacked{false};
        std::optional<std::uint64_t> alignmentBytes;
        SourceSpan span;
    };

    struct Import
    {
        std::string modulePath;
        SourceSpan span;
    };

    struct Module
    {
        std::string packageName;
        std::string moduleName;
        std::vector<Import> imports;
        std::vector<Function> functions;
        std::vector<Blueprint> blueprints;
        std::vector<std::string> kernelMarkers;
        SourceSpan span;
    };
} // namespace bolt::hir
