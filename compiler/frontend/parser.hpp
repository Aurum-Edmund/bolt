#pragma once

#include "ast.hpp"
#include "lexer.hpp"

#include <initializer_list>
#include <optional>
#include <utility>
#include <string_view>

namespace bolt::frontend
{
    class Parser
    {
    public:
        Parser(const std::vector<Token>& tokens, std::string_view moduleName);

        [[nodiscard]] CompilationUnit parse();
        [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept;

    private:
        const Token& peek() const;
        const Token& previous() const;
        const Token& advance();
        bool isAtEnd() const;
        bool check(TokenKind kind) const;
        bool match(TokenKind kind);
        const Token& consume(TokenKind kind, std::string_view messageCode, std::string_view messageText);
        [[nodiscard]] SourceSpan spanFrom(const Token& begin, const Token& end) const;
        bool isTerminator(TokenKind kind, std::initializer_list<TokenKind> terminators, int angleDepth) const;

        ModuleDeclaration parseModule();
        ImportDeclaration parseImport();
        std::vector<std::string> parseModifiers();
        FunctionDeclaration parseFunction(std::vector<std::string> modifiers);
        BlueprintDeclaration parseBlueprint(std::vector<std::string> modifiers);
        std::vector<Attribute> parseAttributes();
        Attribute parseAttribute();
        AttributeArgument parseAttributeArgument();
        Parameter parseParameter();
        BlueprintField parseField();
        std::pair<std::string, SourceSpan> parseQualifiedName(std::string_view code, std::string_view message);
        SourceSpan mergeSpans(const SourceSpan& a, const SourceSpan& b) const;

        struct TypeCapture
        {
            std::string text;
            SourceSpan span{};
            bool valid{false};
        };

        TypeCapture parseTypeUntil(std::initializer_list<TokenKind> terminators);

    private:
        const std::vector<Token>& m_tokens;
        std::string_view m_moduleName;
        std::size_t m_current{0};
        std::vector<Diagnostic> m_diagnostics;
    };
} // namespace bolt::frontend
