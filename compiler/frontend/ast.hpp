#pragma once

#include "token.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bolt::frontend
{
    struct AttributeArgument
    {
        std::string name;
        std::string value;
        SourceSpan span;
    };

    struct Attribute
    {
        std::string name;
        std::vector<AttributeArgument> arguments;
        SourceSpan span;
    };

    struct Parameter
    {
        std::string name;
        std::string typeName;
        SourceSpan span;
        SourceSpan typeSpan;
        bool isLiveValue{false};
        bool hasKernelMarker{false};
        std::string kernelMarkerName;
    };

    struct FunctionDeclaration
    {
        std::vector<Attribute> attributes;
        std::vector<std::string> modifiers;
        std::string name;
        std::vector<Parameter> parameters;
        std::optional<std::string> returnType;
        std::optional<SourceSpan> returnTypeSpan;
        SourceSpan span;
        bool returnIsLiveValue{false};
        bool hasKernelMarker{false};
        std::string kernelMarkerName;
    };

    struct BlueprintField
    {
        std::vector<Attribute> attributes;
        std::string name;
        std::string typeName;
        SourceSpan span;
        SourceSpan typeSpan;
        bool isLiveValue{false};
    };

    struct BlueprintDeclaration
    {
        std::vector<Attribute> attributes;
        std::vector<std::string> modifiers;
        std::string name;
        std::vector<BlueprintField> fields;
        SourceSpan span;
    };

    struct ImportDeclaration
    {
        std::string modulePath;
        SourceSpan span;
    };

    struct ModuleDeclaration
    {
        std::string packageName;
        std::string moduleName;
        SourceSpan span;
    };

    struct CompilationUnit
    {
        ModuleDeclaration module;
        std::vector<ImportDeclaration> imports;
        std::vector<FunctionDeclaration> functions;
        std::vector<BlueprintDeclaration> blueprints;
    };
} // namespace bolt::frontend
