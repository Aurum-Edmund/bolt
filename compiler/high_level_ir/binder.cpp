
#include "binder.hpp"

#include <algorithm>
#include <array>
#include <unordered_set>

namespace bolt::hir
{
    namespace
    {
        bool isRepeatableAttribute(const std::string& name)
        {
            // Stage-0: no attributes are repeatable; reserved for future spec updates.
            (void)name;
            return false;
        }

        template <std::size_t N>
        bool containsAttribute(const std::array<std::string_view, N>& allowed, std::string_view name)
        {
            return std::find(allowed.begin(), allowed.end(), name) != allowed.end();
        }
    } // namespace

    Binder::Binder(const bolt::frontend::CompilationUnit& ast, std::string_view modulePath)
        : m_ast(ast)
        , m_modulePath(modulePath)
    {
    }

    Module Binder::bind()
    {
        m_diagnostics.clear();
        m_functionSymbols.clear();
        m_blueprintSymbols.clear();

        Module module{};
        module.packageName = m_ast.module.packageName;
        module.moduleName = m_ast.module.moduleName;
        module.span = m_ast.module.span;

        std::unordered_map<std::string, SourceSpan> importSymbols;

        for (const auto& importDecl : m_ast.imports)
        {
            if (importDecl.modulePath.empty())
            {
                continue;
            }

            auto inserted = importSymbols.emplace(importDecl.modulePath, importDecl.span);
            if (!inserted.second)
            {
                Diagnostic diag;
                diag.code = "BOLT-E2218";
                diag.message = "Duplicate import '" + importDecl.modulePath + "' in module.";
                diag.span = importDecl.span;
                m_diagnostics.emplace_back(std::move(diag));
                continue;
            }

            Import importEntry{};
            importEntry.modulePath = importDecl.modulePath;
            importEntry.span = importDecl.span;
            module.imports.emplace_back(std::move(importEntry));
        }

        for (const auto& function : m_ast.functions)
        {
            module.functions.emplace_back(convertFunction(function));
        }

        for (const auto& blueprint : m_ast.blueprints)
        {
            module.blueprints.emplace_back(convertBlueprint(blueprint));
        }

        return module;
    }

    const std::vector<Diagnostic>& Binder::diagnostics() const noexcept
    {
        return m_diagnostics;
    }

    Attribute Binder::convertAttribute(const bolt::frontend::Attribute& attribute)
    {
        Attribute converted{};
        converted.name = attribute.name;
        converted.arguments = attribute.arguments;
        converted.span = attribute.span;
        return converted;
    }

    template <typename AttributeRange>
    void Binder::checkDuplicateAttributes(const AttributeRange& attributes)
    {
        std::unordered_map<std::string, SourceSpan> seen;
        for (const auto& attribute : attributes)
        {
            auto it = seen.find(attribute.name);
            if (it != seen.end())
            {
                if (!isRepeatableAttribute(attribute.name))
                {
                    Diagnostic diag;
                    diag.code = "BOLT-E2200";
                    diag.message = "Duplicate attribute '" + attribute.name + "' is not allowed.";
                    diag.span = attribute.span;
                    m_diagnostics.emplace_back(std::move(diag));
                }
            }
            else
            {
                seen.emplace(attribute.name, attribute.span);
            }
        }
    }

    void Binder::validateAttributes(const std::vector<Attribute>& attributes, AttributeContext context)
    {
        for (const auto& attribute : attributes)
        {
            reportUnknownAttribute(attribute, context);
        }
    }

    void Binder::reportUnknownAttribute(const Attribute& attribute, AttributeContext context)
    {
        bool allowed = false;

        switch (context)
        {
        case AttributeContext::Function:
        {
            static constexpr std::array<std::string_view, 7> allowedAttributes{
                "interruptHandler",
                "bareFunction",
                "inSection",
                "aligned",
                "pageAligned",
                "systemRequest",
                "intrinsic"
            };
            allowed = containsAttribute(allowedAttributes, attribute.name);
            break;
        }
        case AttributeContext::Blueprint:
        {
            static constexpr std::array<std::string_view, 2> allowedAttributes{
                "packed",
                "aligned"
            };
            allowed = containsAttribute(allowedAttributes, attribute.name);
            break;
        }
        case AttributeContext::BlueprintField:
        {
            static constexpr std::array<std::string_view, 2> allowedAttributes{
                "bits",
                "aligned"
            };
            allowed = containsAttribute(allowedAttributes, attribute.name);
            break;
        }
        }

        if (!allowed && attribute.name.rfind("kernel_", 0) == 0)
        {
            allowed = true;
        }

        if (!allowed)
        {
            Diagnostic diag;
            diag.code = "BOLT-E2201";
            std::string contextName;
            switch (context)
            {
            case AttributeContext::Function: contextName = "function"; break;
            case AttributeContext::Blueprint: contextName = "blueprint"; break;
            case AttributeContext::BlueprintField: contextName = "blueprint field"; break;
            }
            diag.message = "Unknown or misplaced attribute '" + attribute.name + "' on " + contextName + ".";
            diag.span = attribute.span;
            m_diagnostics.emplace_back(std::move(diag));
        }
    }

    void Binder::emitError(const std::string& code, const std::string& message, SourceSpan span)
    {
        Diagnostic diag;
        diag.code = code;
        diag.message = message;
        diag.span = span;
        m_diagnostics.emplace_back(std::move(diag));
    }

    const bolt::frontend::AttributeArgument* Binder::findAttributeArgument(const Attribute& attribute, std::string_view name) const
    {
        if (!name.empty())
        {
            for (const auto& argument : attribute.arguments)
            {
                if (argument.name == name)
                {
                    return &argument;
                }
            }
        }

        for (const auto& argument : attribute.arguments)
        {
            if (argument.name.empty())
            {
                return &argument;
            }
        }

        if (name.empty() && !attribute.arguments.empty())
        {
            return &attribute.arguments.front();
        }

        return nullptr;
    }

std::optional<std::uint64_t> Binder::parseUnsigned(const bolt::frontend::AttributeArgument& argument) const
{
    try
    {
        std::size_t index = 0;
        std::uint64_t value = std::stoull(argument.value, &index, 0);
        if (index == argument.value.size())
        {
            return value;
        }
    }
    catch (...)
    {
    }
    return std::nullopt;
}

void Binder::applyLiveValueQualifier(TypeReference& typeRef, bool& isLiveValue, const std::string& subject, const SourceSpan& span)
{
    constexpr std::string_view qualifier = "LiveValue";
    if (typeRef.text.rfind(qualifier, 0) != 0)
    {
        return;
    }

    std::string remainder = typeRef.text.substr(qualifier.size());
    const auto first = remainder.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        emitError("BOLT-E2217", "LiveValue qualifier on " + subject + " must reference a concrete type.", span);
        return;
    }
    const auto last = remainder.find_last_not_of(" \t\r\n");
    typeRef.text = remainder.substr(first, last - first + 1);
    isLiveValue = true;
}

    Function Binder::convertFunction(const bolt::frontend::FunctionDeclaration& function)
    {
        Function converted{};
        converted.name = function.name;
        converted.modifiers = function.modifiers;
        converted.span = function.span;

        auto existing = m_functionSymbols.find(converted.name);
        if (existing != m_functionSymbols.end())
        {
            Diagnostic diag;
            diag.code = "BOLT-E2210";
            diag.message = "Duplicate function '" + converted.name + "' in module.";
            diag.span = converted.span;
            m_diagnostics.emplace_back(std::move(diag));
        }
        else
        {
            m_functionSymbols.emplace(converted.name, converted.span);
        }

        for (const auto& attribute : function.attributes)
        {
            converted.attributes.emplace_back(convertAttribute(attribute));
        }
        checkDuplicateAttributes(converted.attributes);
        validateAttributes(converted.attributes, AttributeContext::Function);

        for (const auto& attribute : converted.attributes)
        {
            if (attribute.name == "interruptHandler")
            {
                if (converted.isBareFunction)
                {
                    emitError("BOLT-E2215", "Attributes 'interruptHandler' and 'bareFunction' cannot be combined.", attribute.span);
                }
                converted.isInterruptHandler = true;
            }
            else if (attribute.name == "bareFunction")
            {
                if (converted.isInterruptHandler)
                {
                    emitError("BOLT-E2215", "Attributes 'interruptHandler' and 'bareFunction' cannot be combined.", attribute.span);
                }
                converted.isBareFunction = true;
            }
            else if (attribute.name == "inSection")
            {
                const auto* argument = findAttributeArgument(attribute, "name");
                if (argument == nullptr || argument->value.empty())
                {
                    emitError("BOLT-E2214", "Attribute 'inSection' requires a section name.", attribute.span);
                }
                else
                {
                    converted.sectionName = argument->value;
                }
            }
            else if (attribute.name == "aligned")
            {
                const auto* argument = findAttributeArgument(attribute, "bytes");
                if (argument == nullptr)
                {
                    emitError("BOLT-E2214", "Attribute 'aligned' requires a positive integer argument.", attribute.span);
                }
                else
                {
                    auto value = parseUnsigned(*argument);
                    if (!value.has_value() || *value == 0)
                    {
                        emitError("BOLT-E2214", "Attribute 'aligned' requires a positive integer argument.", argument->span);
                    }
                    else
                    {
                        converted.alignmentBytes = *value;
                    }
                }
            }
            else if (attribute.name == "pageAligned")
            {
                converted.isPageAligned = true;
            }
            else if (attribute.name == "systemRequest")
            {
                const auto* argument = findAttributeArgument(attribute, "identifier");
                if (argument == nullptr)
                {
                    emitError("BOLT-E2214", "Attribute 'systemRequest' requires an identifier argument.", attribute.span);
                }
                else
                {
                    auto value = parseUnsigned(*argument);
                    if (!value.has_value())
                    {
                        emitError("BOLT-E2214", "Attribute 'systemRequest' requires an integer identifier.", argument->span);
                    }
                    else
                    {
                        converted.systemRequestId = *value;
                    }
                }
            }
            else if (attribute.name == "intrinsic")
            {
                const auto* argument = findAttributeArgument(attribute, "name");
                if (argument == nullptr || argument->value.empty())
                {
                    emitError("BOLT-E2214", "Attribute 'intrinsic' requires a non-empty name argument.", attribute.span);
                }
                else
                {
                    converted.intrinsicName = argument->value;
                }
            }
            else if (attribute.name.rfind("kernel_", 0) == 0)
            {
                converted.kernelMarkers.emplace_back(attribute.name);
            }
        }

        for (const auto& parameter : function.parameters)
        {
            Parameter param;
            param.name = parameter.name;
            param.type.text = parameter.typeName;
            param.type.span = parameter.typeSpan;
            param.span = parameter.span;
            applyLiveValueQualifier(param.type, param.isLiveValue, "parameter '" + param.name + "' in function '" + converted.name + "'", param.type.span);
            converted.parameters.emplace_back(std::move(param));
        }

        std::unordered_map<std::string, SourceSpan> parameterSymbols;
        for (const auto& parameter : converted.parameters)
        {
            auto inserted = parameterSymbols.emplace(parameter.name, parameter.span);
            if (!inserted.second)
            {
                Diagnostic diag;
                diag.code = "BOLT-E2212";
                diag.message = "Duplicate parameter '" + parameter.name + "' in function '" + converted.name + "'.";
                diag.span = parameter.span;
                m_diagnostics.emplace_back(std::move(diag));
            }
        }

        if (function.returnType.has_value())
        {
            converted.returnType.text = *function.returnType;
            if (function.returnTypeSpan.has_value())
            {
                converted.returnType.span = *function.returnTypeSpan;
            }
            converted.hasReturnType = true;
            applyLiveValueQualifier(converted.returnType, converted.returnIsLiveValue, "return type of function '" + converted.name + "'", converted.returnType.span);
        }

        return converted;
    }

    BlueprintField Binder::convertField(const bolt::frontend::BlueprintField& field, bool parentIsPacked, const std::string& blueprintName)
    {
        BlueprintField converted{};
        converted.name = field.name;
        converted.type.text = field.typeName;
        converted.type.span = field.typeSpan;
        converted.span = field.span;

        for (const auto& attribute : field.attributes)
        {
            converted.attributes.emplace_back(convertAttribute(attribute));
        }
        checkDuplicateAttributes(converted.attributes);
        validateAttributes(converted.attributes, AttributeContext::BlueprintField);

        for (const auto& attribute : converted.attributes)
        {
            if (attribute.name == "bits")
            {
                const auto* argument = findAttributeArgument(attribute, "width");
                if (argument == nullptr)
                {
                    emitError("BOLT-E2214", "Attribute 'bits' requires a width argument.", attribute.span);
                }
                else
                {
                    auto value = parseUnsigned(*argument);
                    if (!value.has_value() || *value == 0 || *value > 64)
                    {
                        emitError("BOLT-E2214", "Attribute 'bits' requires a width between 1 and 64.", argument->span);
                    }
                    else
                    {
                        converted.bitWidth = static_cast<std::uint32_t>(*value);
                        if (!parentIsPacked)
                        {
                            emitError("BOLT-E2216", "Attribute 'bits' requires the containing blueprint to be marked 'packed'.", attribute.span);
                        }
                    }
                }
            }
            else if (attribute.name == "aligned")
            {
                const auto* argument = findAttributeArgument(attribute, "bytes");
                if (argument == nullptr)
                {
                    emitError("BOLT-E2214", "Field-level 'aligned' requires a positive integer argument.", attribute.span);
                }
                else
                {
                    auto value = parseUnsigned(*argument);
                    if (!value.has_value() || *value == 0)
                    {
                        emitError("BOLT-E2214", "Field-level 'aligned' requires a positive integer argument.", argument->span);
                    }
                    else
                    {
                        converted.alignmentBytes = *value;
                    }
                }
            }
        }

        applyLiveValueQualifier(converted.type, converted.isLiveValue, "field '" + converted.name + "' in blueprint '" + blueprintName + "'", converted.type.span);
        return converted;
    }

    Blueprint Binder::convertBlueprint(const bolt::frontend::BlueprintDeclaration& blueprint)
    {
        Blueprint converted{};
        converted.name = blueprint.name;
        converted.modifiers = blueprint.modifiers;
        converted.span = blueprint.span;

        auto existing = m_blueprintSymbols.find(converted.name);
        if (existing != m_blueprintSymbols.end())
        {
            Diagnostic diag;
            diag.code = "BOLT-E2211";
            diag.message = "Duplicate blueprint '" + converted.name + "' in module.";
            diag.span = converted.span;
            m_diagnostics.emplace_back(std::move(diag));
        }
        else
        {
            m_blueprintSymbols.emplace(converted.name, converted.span);
        }

        for (const auto& attribute : blueprint.attributes)
        {
            converted.attributes.emplace_back(convertAttribute(attribute));
        }
        checkDuplicateAttributes(converted.attributes);
        validateAttributes(converted.attributes, AttributeContext::Blueprint);

        for (const auto& attribute : converted.attributes)
        {
            if (attribute.name == "packed")
            {
                converted.isPacked = true;
            }
            else if (attribute.name == "aligned")
            {
                const auto* argument = findAttributeArgument(attribute, "bytes");
                if (argument == nullptr)
                {
                    emitError("BOLT-E2214", "Blueprint-level 'aligned' requires a positive integer argument.", attribute.span);
                }
                else
                {
                    auto value = parseUnsigned(*argument);
                    if (!value.has_value() || *value == 0)
                    {
                        emitError("BOLT-E2214", "Blueprint-level 'aligned' requires a positive integer argument.", argument->span);
                    }
                    else
                    {
                        converted.alignmentBytes = *value;
                    }
                }
            }
        }

        std::unordered_map<std::string, SourceSpan> fieldSymbols;
        for (const auto& field : blueprint.fields)
        {
            converted.fields.emplace_back(convertField(field, converted.isPacked, converted.name));
            auto inserted = fieldSymbols.emplace(converted.fields.back().name, converted.fields.back().span);
            if (!inserted.second)
            {
                Diagnostic diag;
                diag.code = "BOLT-E2213";
                diag.message = "Duplicate field '" + converted.fields.back().name + "' in blueprint '" + converted.name + "'.";
                diag.span = converted.fields.back().span;
                m_diagnostics.emplace_back(std::move(diag));
            }
        }

        return converted;
    }

    template void Binder::checkDuplicateAttributes<std::vector<Attribute>>(const std::vector<Attribute>& attributes);
} // namespace bolt::hir

