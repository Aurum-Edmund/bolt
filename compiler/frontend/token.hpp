#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace bolt::frontend
{
    enum class TokenKind : std::uint16_t
    {
        EndOfFile,
        Identifier,
        IntegerLiteral,
        StringLiteral,

        // Keywords (full words, per language glossary)
        KeywordPackage,
        KeywordModule,
        KeywordImport,
        KeywordBlueprint,
        KeywordEnumeration,
        KeywordInterface,
        KeywordFunction,
        KeywordConstant,
        KeywordMutable,
        KeywordFixed,
        KeywordAlias,
        KeywordMatch,
        KeywordGuard,
        KeywordReturn,
        KeywordBreak,
        KeywordContinue,
        KeywordPublic,
        KeywordUse,
        KeywordExtern,
        KeywordIntrinsic,
        KeywordTrue,
        KeywordFalse,
        KeywordNull,
        KeywordVoid,
        KeywordLink,
        KeywordIf,
        KeywordElse,
        KeywordWhile,
        KeywordFor,
        KeywordSwitch,
        KeywordCase,

        // Punctuation
        LeftBrace,
        RightBrace,
        LeftParen,
        RightParen,
        LeftBracket,
        RightBracket,
        Comma,
        Colon,
        Semicolon,
        Dot,
        Arrow,
        Equals,
        Plus,
        Minus,
        Asterisk,
        Slash,
        Percent,
        Ampersand,
        Pipe,
        Caret,
        Bang,
        LessThan,
        GreaterThan,
        LessEquals,
        GreaterEquals,
        EqualsEquals,
        BangEquals
    };

    struct SourceLocation
    {
        std::uint32_t line{1};
        std::uint32_t column{1};
    };

    struct SourceSpan
    {
        SourceLocation begin{};
        SourceLocation end{};
    };

    struct Token
    {
        TokenKind kind{TokenKind::EndOfFile};
        SourceSpan span{};
        std::string text{};
    };

    [[nodiscard]] std::string_view toString(TokenKind kind);
} // namespace bolt::frontend
