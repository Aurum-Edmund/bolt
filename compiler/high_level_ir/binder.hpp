#pragma once

#include "diagnostic.hpp"
#include "module.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bolt::hir
{
    class Binder
    {
    public:
        Binder(const bolt::frontend::CompilationUnit& ast, std::string_view modulePath);

        [[nodiscard]] Module bind();
        [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept;

    private:
        enum class AttributeContext
        {
            Function,
            Blueprint,
            BlueprintField
        };

        template <typename AttributeRange>
        void checkDuplicateAttributes(const AttributeRange& attributes);

        void validateAttributes(const std::vector<Attribute>& attributes, AttributeContext context);

        void reportUnknownAttribute(const Attribute& attribute, AttributeContext context);
        void emitError(const std::string& code, const std::string& message, SourceSpan span);
        void emitWarning(const std::string& code, const std::string& message, SourceSpan span);
        const bolt::frontend::AttributeArgument* findAttributeArgument(const Attribute& attribute, std::string_view name) const;
        std::optional<std::uint64_t> parseUnsigned(const bolt::frontend::AttributeArgument& argument) const;
        void applyLiveQualifier(TypeReference& typeRef, bool& isLive, const std::string& subject, const SourceSpan& span);

        Attribute convertAttribute(const bolt::frontend::Attribute& attribute);
        Function convertFunction(const bolt::frontend::FunctionDeclaration& function);
        Blueprint convertBlueprint(const bolt::frontend::BlueprintDeclaration& blueprint);
        BlueprintField convertField(const bolt::frontend::BlueprintField& field, bool parentIsPacked, const std::string& blueprintName);
        void applyBlueprintLifecycle(Function& function);

    private:
        const bolt::frontend::CompilationUnit& m_ast;
        std::string m_modulePath;
        std::vector<Diagnostic> m_diagnostics;
        std::unordered_map<std::string, SourceSpan> m_functionSymbols;
        std::unordered_map<std::string, SourceSpan> m_blueprintSymbols;
        std::unordered_set<std::string> m_knownBlueprintNames;
    };
} // namespace bolt::hir
