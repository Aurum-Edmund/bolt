#pragma once

#include "token.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bolt::frontend
{
    struct Diagnostic
    {
        std::string code;
        std::string message;
        SourceSpan span;
    };

    class Lexer
    {
    public:
        explicit Lexer(std::string_view source, std::string_view moduleName = {});

        [[nodiscard]] const std::vector<Token>& tokens() const noexcept;
        [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept;

        void lex();

    private:
        void pushToken(TokenKind kind, SourceLocation start, SourceLocation end, std::string_view text);
        void lexIdentifierOrKeyword();
        void lexNumber();
        void lexString();
        void lexSlashOrComment();
        void emitSingle(TokenKind kind);
        bool match(char expected);
        char peek() const;
        char peekNext() const;
        char advance();
        bool isAtEnd() const;
        void advanceLine();

    private:
        std::string_view m_source;
        std::string_view m_moduleName;
        std::vector<Token> m_tokens;
        std::vector<Diagnostic> m_diagnostics;
        std::size_t m_current{0};
        SourceLocation m_location{};
    };
} // namespace bolt::frontend
