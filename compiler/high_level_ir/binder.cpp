
#include "binder.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <optional>
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

        std::string normalizeTypeToken(std::string_view token)
        {
            if (token == "integer32")
            {
                return "integer";
            }
            if (token == "float32")
            {
                return "float";
            }
            if (token == "float64")
            {
                return "double";
            }
            return std::string{token};
        }

        std::string normalizeTypeTokens(std::string_view text)
        {
            std::string result;
            result.reserve(text.size());

            std::size_t index = 0;
            while (index < text.size())
            {
                const unsigned char ch = static_cast<unsigned char>(text[index]);
                if (std::isalnum(ch))
                {
                    const std::size_t start = index;
                    while (index < text.size() && std::isalnum(static_cast<unsigned char>(text[index])))
                    {
                        ++index;
                    }
                    result += normalizeTypeToken(text.substr(start, index - start));
                }
                else
                {
                    result.push_back(static_cast<char>(ch));
                    ++index;
                }
            }

            return result;
        }

        std::string trimCopy(std::string_view text)
        {
            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos)
            {
                return std::string{};
            }

            const auto last = text.find_last_not_of(" \t\r\n");
            return std::string{text.substr(first, last - first + 1)};
        }

        std::string normalizeTypeText(std::string_view text)
        {
            std::string trimmed = trimCopy(text);
            if (trimmed.empty())
            {
                return trimmed;
            }

            std::vector<char> qualifiers;
            std::size_t index = trimmed.size();
            while (index > 0)
            {
                unsigned char ch = static_cast<unsigned char>(trimmed[index - 1]);
                if (std::isspace(ch))
                {
                    --index;
                    continue;
                }

                if (ch == '*' || ch == '&')
                {
                    qualifiers.push_back(static_cast<char>(ch));
                    --index;
                    continue;
                }

                break;
            }

            std::string core = trimmed.substr(0, index);
            if (!core.empty())
            {
                core = trimCopy(core);
            }

            std::string result = normalizeTypeTokens(core);

            for (std::size_t i = qualifiers.size(); i > 0; --i)
            {
                const char qualifier = qualifiers[i - 1];
                if (qualifier == '*')
                {
                    if (result.empty())
                    {
                        result = "pointer<void>";
                    }
                    else
                    {
                        result = "pointer<" + result + ">";
                    }
                }
                else
                {
                    if (result.empty())
                    {
                        result = "reference<void>";
                    }
                    else
                    {
                        result = "reference<" + result + ">";
                    }
                }
            }

            return result;
        }

        bool startsWith(std::string_view text, std::string_view prefix)
        {
            return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
        }

        bool isPointerType(std::string_view typeText)
        {
            return startsWith(typeText, "pointer<") || startsWith(typeText, "sharedPointer<");
        }

        bool isReferenceType(std::string_view typeText)
        {
            return startsWith(typeText, "reference<");
        }

        std::optional<std::string> defaultValueForType(std::string_view typeText)
        {
            if (typeText.empty())
            {
                return std::string{"{}"};
            }

            if (isReferenceType(typeText))
            {
                return std::nullopt;
            }

            if (isPointerType(typeText))
            {
                return std::string{"null"};
            }

            if (startsWith(typeText, "integer") || startsWith(typeText, "natural") || startsWith(typeText, "signed")
                || startsWith(typeText, "unsigned"))
            {
                return std::string{"0"};
            }

            if (startsWith(typeText, "float") || startsWith(typeText, "double"))
            {
                return std::string{"0.0"};
            }

            if (typeText == "bool")
            {
                return std::string{"false"};
            }

            return std::string{"{}"};
        }

        bool isIdentifierChar(char ch)
        {
            return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
        }

        bool isIdentifierStart(char ch)
        {
            return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_';
        }

        bool hasPrefix(std::string_view text, std::string_view prefix)
        {
            return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
        }

        bool isBuiltinIdentifier(std::string_view identifier)
        {
            static constexpr std::array<std::string_view, 7> exactBuiltins{
                "void",
                "bool",
                "byte",
                "char",
                "float",
                "double",
                "pointerSizedInteger"
            };

            if (std::find(exactBuiltins.begin(), exactBuiltins.end(), identifier) != exactBuiltins.end())
            {
                return true;
            }

            return hasPrefix(identifier, "integer") || hasPrefix(identifier, "natural") || hasPrefix(identifier, "signed")
                || hasPrefix(identifier, "unsigned") || hasPrefix(identifier, "float");
        }

        bool isBuiltinQualified(const QualifiedName& name)
        {
            if (name.components.size() != 1)
            {
                return false;
            }
            return isBuiltinIdentifier(name.components.front());
        }

        bool isPointerName(const QualifiedName& name)
        {
            if (name.components.size() != 1)
            {
                return false;
            }
            const auto& base = name.components.front();
            return base == "pointer" || base == "sharedPointer";
        }

        bool isReferenceName(const QualifiedName& name)
        {
            return name.components.size() == 1 && name.components.front() == "reference";
        }

        bool isQualifierIdentifier(std::string_view identifier)
        {
            static constexpr std::array<std::string_view, 1> qualifiers{"constant"};
            return std::find(qualifiers.begin(), qualifiers.end(), identifier) != qualifiers.end();
        }

        class TypeParser
        {
        public:
            explicit TypeParser(std::string_view text)
                : m_text(text)
            {
            }

            TypeReference parse()
            {
                skipWhitespace();
                const std::size_t start = m_index;
                TypeReference type = parseType();
                skipWhitespace();
                if (!atEnd())
                {
                    m_failed = true;
                }

                if (!m_failed)
                {
                    const std::size_t end = m_index;
                    type.text = trimCopy(m_text.substr(start, end - start));
                    type.originalText = type.text;
                }

                return type;
            }

            bool success() const noexcept
            {
                return !m_failed;
            }

            const std::optional<std::string>& duplicateQualifier() const noexcept
            {
                return m_duplicateQualifier;
            }

        private:
            TypeReference parseType()
            {
                skipWhitespace();
                const std::size_t typeStart = m_index;
                std::vector<std::string> qualifiers;
                std::unordered_set<std::string> seenQualifiers;
                while (true)
                {
                    auto qualifier = tryParseQualifier();
                    if (!qualifier.has_value())
                    {
                        break;
                    }
                    if (!seenQualifiers.insert(*qualifier).second)
                    {
                        m_duplicateQualifier = *qualifier;
                        m_failed = true;
                        return TypeReference{};
                    }
                    qualifiers.emplace_back(std::move(*qualifier));
                    skipWhitespace();
                }
                TypeReference base = parsePrimary();
                if (m_failed)
                {
                    return base;
                }

                base.qualifiers = std::move(qualifiers);

                const std::size_t primaryEnd = m_index;
                if (primaryEnd > typeStart)
                {
                    base.text = trimCopy(m_text.substr(typeStart, primaryEnd - typeStart));
                    base.originalText = base.text;
                }

                skipWhitespace();
                while (match('['))
                {
                    skipWhitespace();
                    TypeReference arrayType{};
                    arrayType.kind = TypeKind::Array;
                    arrayType.isBuiltin = false;

                    arrayType.arrayLength = parseArrayLength();
                    skipWhitespace();
                    if (!match(']'))
                    {
                        m_failed = true;
                        return arrayType;
                    }

                    arrayType.genericArguments.emplace_back(std::move(base));
                    TypeReference& element = arrayType.genericArguments.back();

                    std::string elementText = element.text.empty() ? element.originalText : element.text;
                    if (elementText.empty())
                    {
                        elementText = element.qualifiedName();
                    }

                    arrayType.text = elementText;
                    arrayType.text.push_back('[');
                    if (arrayType.arrayLength.has_value())
                    {
                        arrayType.text += std::to_string(*arrayType.arrayLength);
                    }
                    arrayType.text.push_back(']');
                    arrayType.originalText = arrayType.text;

                    base = std::move(arrayType);
                    skipWhitespace();
                }

                const std::size_t typeEnd = m_index;
                base.text = trimCopy(m_text.substr(typeStart, typeEnd - typeStart));
                base.originalText = base.text;
                return base;
            }

            TypeReference parsePrimary()
            {
                QualifiedName name = parseQualifiedName();
                if (m_failed)
                {
                    return TypeReference{};
                }

                TypeReference type{};
                type.kind = TypeKind::Named;
                type.name = std::move(name);
                type.isBuiltin = isBuiltinQualified(type.name);

                skipWhitespace();
                if (match('<'))
                {
                    skipWhitespace();
                    if (match('>'))
                    {
                        m_failed = true;
                        return type;
                    }

                    while (true)
                    {
                        TypeReference argument = parseType();
                        if (m_failed)
                        {
                            return type;
                        }
                        type.genericArguments.emplace_back(std::move(argument));
                        skipWhitespace();
                        if (match('>'))
                        {
                            break;
                        }
                        if (!match(','))
                        {
                            m_failed = true;
                            return type;
                        }
                        skipWhitespace();
                    }
                }

                if (isPointerName(type.name))
                {
                    type.kind = TypeKind::Pointer;
                    type.isBuiltin = true;
                    if (type.genericArguments.size() != 1)
                    {
                        m_failed = true;
                    }
                }
                else if (isReferenceName(type.name))
                {
                    type.kind = TypeKind::Reference;
                    type.isBuiltin = true;
                    if (type.genericArguments.size() != 1)
                    {
                        m_failed = true;
                    }
                }

                return type;
            }

            QualifiedName parseQualifiedName()
            {
                skipWhitespace();
                QualifiedName name{};
                std::string identifier = parseIdentifier();
                if (identifier.empty())
                {
                    m_failed = true;
                    return name;
                }
                name.components.emplace_back(std::move(identifier));

                while (true)
                {
                    skipWhitespace();
                    if (match('.'))
                    {
                        skipWhitespace();
                        std::string part = parseIdentifier();
                        if (part.empty())
                        {
                            m_failed = true;
                            return name;
                        }
                        name.components.emplace_back(std::move(part));
                        continue;
                    }

                    if (m_index + 1 < m_text.size() && m_text[m_index] == ':' && m_text[m_index + 1] == ':')
                    {
                        m_index += 2;
                        skipWhitespace();
                        std::string part = parseIdentifier();
                        if (part.empty())
                        {
                            m_failed = true;
                            return name;
                        }
                        name.components.emplace_back(std::move(part));
                        continue;
                    }

                    break;
                }

                return name;
            }

            std::string parseIdentifier()
            {
                const std::size_t start = m_index;
                while (m_index < m_text.size() && isIdentifierChar(m_text[m_index]))
                {
                    ++m_index;
                }

                if (start == m_index)
                {
                    m_failed = true;
                    return std::string{};
                }

                return std::string{m_text.substr(start, m_index - start)};
            }

            std::optional<std::uint64_t> parseArrayLength()
            {
                skipWhitespace();
                std::uint64_t value = 0;
                bool hasDigits = false;

                while (m_index < m_text.size())
                {
                    const char ch = m_text[m_index];
                    if (!std::isdigit(static_cast<unsigned char>(ch)))
                    {
                        break;
                    }

                    hasDigits = true;
                    const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
                    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
                    {
                        m_failed = true;
                        return std::nullopt;
                    }
                    value = value * 10 + digit;
                    ++m_index;
                }

                if (!hasDigits)
                {
                    return std::nullopt;
                }

                return value;
            }

            std::optional<std::string> tryParseQualifier()
            {
                if (atEnd())
                {
                    return std::nullopt;
                }

                const std::size_t start = m_index;
                if (!isIdentifierStart(m_text[m_index]))
                {
                    return std::nullopt;
                }

                while (m_index < m_text.size() && isIdentifierChar(m_text[m_index]))
                {
                    ++m_index;
                }

                std::string_view candidate = m_text.substr(start, m_index - start);
                if (!isQualifierIdentifier(candidate))
                {
                    m_index = start;
                    return std::nullopt;
                }

                return std::string{candidate};
            }

            void skipWhitespace()
            {
                while (m_index < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_index])) != 0)
                {
                    ++m_index;
                }
            }

            bool match(char expected)
            {
                if (m_index < m_text.size() && m_text[m_index] == expected)
                {
                    ++m_index;
                    return true;
                }
                return false;
            }

            bool atEnd() const noexcept
            {
                return m_index >= m_text.size();
            }

        private:
            std::string_view m_text;
            std::size_t m_index{0};
            bool m_failed{false};
            std::optional<std::string> m_duplicateQualifier;
        };
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
        m_knownBlueprintNames.clear();

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

        for (const auto& blueprint : m_ast.blueprints)
        {
            if (!blueprint.name.empty())
            {
                m_knownBlueprintNames.emplace(blueprint.name);
            }
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

    void Binder::emitWarning(const std::string& code, const std::string& message, SourceSpan span)
    {
        Diagnostic diag;
        diag.code = code;
        diag.message = message;
        diag.span = span;
        diag.isWarning = true;
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

void Binder::applyLiveQualifier(std::string& typeText, bool& isLive, const std::string& subject, const SourceSpan& span)
{
    constexpr std::string_view qualifier = "live";
    std::string trimmed = trimCopy(typeText);
    if (trimmed.empty())
    {
        typeText = std::move(trimmed);
        return;
    }

    if (trimmed.rfind(qualifier, 0) != 0)
    {
        typeText = std::move(trimmed);
        return;
    }

    std::string remainder = trimmed.substr(qualifier.size());
    const auto first = remainder.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        emitError("BOLT-E2217", "live qualifier on " + subject + " must reference a concrete type.", span);
        typeText.clear();
        return;
    }
    const auto last = remainder.find_last_not_of(" \t\r\n");
    typeText = remainder.substr(first, last - first + 1);
    isLive = true;
}

TypeReference Binder::buildTypeReference(
    const std::string& typeText,
    const SourceSpan& typeSpan,
    bool& isLive,
    const std::string& subject)
{
    TypeReference reference{};
    reference.span = typeSpan;
    reference.originalText = typeText;

    std::string workingText = typeText;
    applyLiveQualifier(workingText, isLive, subject, typeSpan);
    std::string normalized = normalizeTypeText(workingText);
    reference.text = normalized;

    if (normalized.empty())
    {
        return reference;
    }

    TypeParser parser{normalized};
    TypeReference parsed = parser.parse();
    if (!parser.success() || !parsed.isValid())
    {
        if (const auto& duplicate = parser.duplicateQualifier())
        {
            emitError(
                "BOLT-E2301",
                "Duplicate '" + *duplicate + "' qualifier is not allowed for " + subject + ".",
                typeSpan);
        }
        else
        {
            emitError("BOLT-E2300", "Unable to parse type '" + normalized + "' for " + subject + ".", typeSpan);
        }
        reference.kind = TypeKind::Invalid;
        reference.isBuiltin = false;
        return reference;
    }

    parsed.text = normalized;
    parsed.originalText = typeText;
    parsed.span = typeSpan;
    return parsed;
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
            param.span = parameter.span;
            param.type = buildTypeReference(
                parameter.typeName,
                parameter.typeSpan,
                param.isLive,
                "parameter '" + param.name + "' in function '" + converted.name + "'");
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
            SourceSpan returnSpan = function.returnTypeSpan.has_value() ? *function.returnTypeSpan : SourceSpan{};
            converted.returnType = buildTypeReference(
                *function.returnType,
                returnSpan,
                converted.returnIsLive,
                "return type of function '" + converted.name + "'");
            converted.hasReturnType = true;
        }

        applyBlueprintLifecycle(converted);

        return converted;
    }

    BlueprintField Binder::convertField(const bolt::frontend::BlueprintField& field, bool parentIsPacked, const std::string& blueprintName)
    {
        BlueprintField converted{};
        converted.name = field.name;
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

        converted.type = buildTypeReference(
            field.typeName,
            field.typeSpan,
            converted.isLive,
            "field '" + converted.name + "' in blueprint '" + blueprintName + "'");
        return converted;
    }

    void Binder::applyBlueprintLifecycle(Function& function)
    {
        if (function.name.empty())
        {
            return;
        }

        const bool isDestructor = function.name.front() == '~';
        std::string candidateName = isDestructor ? function.name.substr(1) : function.name;
        if (candidateName.empty())
        {
            return;
        }

        if (m_knownBlueprintNames.find(candidateName) == m_knownBlueprintNames.end())
        {
            return;
        }

        function.blueprintName = candidateName;
        if (isDestructor)
        {
            function.isBlueprintDestructor = true;
            if (!function.parameters.empty())
            {
                SourceSpan span = function.parameters.front().span;
                emitError("BOLT-E2230", "Destructor '~" + candidateName + "' must not declare parameters.", span);
            }
            return;
        }

        function.isBlueprintConstructor = true;
        for (auto& parameter : function.parameters)
        {
            const auto defaultValue = defaultValueForType(parameter.type.text);
            if (defaultValue.has_value())
            {
                parameter.hasDefaultValue = true;
                parameter.defaultValue = *defaultValue;
                parameter.requiresExplicitValue = false;
            }
            else
            {
                parameter.requiresExplicitValue = true;
                emitWarning(
                    "BOLT-W2210",
                    "Constructor parameter '" + parameter.name + "' in blueprint '" + candidateName + "' requires an explicit value.",
                    parameter.span);
            }
        }
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

