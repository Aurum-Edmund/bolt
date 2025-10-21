#include "lexer.hpp"

#include <cctype>

namespace
{
    using namespace bolt::frontend;

    bool isIdentifierStart(char ch)
    {
        return std::isalpha(static_cast<unsigned char>(ch));
    }

    bool isIdentifierPart(char ch)
    {
        return std::isalnum(static_cast<unsigned char>(ch));
    }

    TokenKind keywordLookup(std::string_view text)
    {
        if (text == "package") return TokenKind::KeywordPackage;
        if (text == "module") return TokenKind::KeywordModule;
        if (text == "import") return TokenKind::KeywordImport;
        if (text == "blueprint") return TokenKind::KeywordBlueprint;
        if (text == "enumeration") return TokenKind::KeywordEnumeration;
        if (text == "interface") return TokenKind::KeywordInterface;
        if (text == "function") return TokenKind::KeywordFunction;
        if (text == "constant") return TokenKind::KeywordConstant;
        if (text == "mutable") return TokenKind::KeywordMutable;
        if (text == "fixed") return TokenKind::KeywordFixed;
        if (text == "alias") return TokenKind::KeywordAlias;
        if (text == "match") return TokenKind::KeywordMatch;
        if (text == "guard") return TokenKind::KeywordGuard;
        if (text == "return") return TokenKind::KeywordReturn;
        if (text == "break") return TokenKind::KeywordBreak;
        if (text == "continue") return TokenKind::KeywordContinue;
        if (text == "public") return TokenKind::KeywordPublic;
        if (text == "use") return TokenKind::KeywordUse;
        if (text == "external") return TokenKind::KeywordExternal;
        if (text == "intrinsic") return TokenKind::KeywordIntrinsic;
        if (text == "new") return TokenKind::KeywordNew;
        if (text == "delete") return TokenKind::KeywordDelete;
        if (text == "true") return TokenKind::KeywordTrue;
        if (text == "false") return TokenKind::KeywordFalse;
        if (text == "null") return TokenKind::KeywordNull;
        if (text == "void") return TokenKind::KeywordVoid;
        if (text == "link") return TokenKind::KeywordLink;
        if (text == "if") return TokenKind::KeywordIf;
        if (text == "else") return TokenKind::KeywordElse;
        if (text == "while") return TokenKind::KeywordWhile;
        if (text == "for") return TokenKind::KeywordFor;
        if (text == "switch") return TokenKind::KeywordSwitch;
        if (text == "case") return TokenKind::KeywordCase;
        return TokenKind::Identifier;
    }
} // namespace

namespace bolt::frontend
{
    Lexer::Lexer(std::string_view source, std::string_view moduleName)
        : m_source(source)
        , m_moduleName(moduleName)
        , m_location{1, 1}
    {
    }

    const std::vector<Token>& Lexer::tokens() const noexcept
    {
        return m_tokens;
    }

    const std::vector<Diagnostic>& Lexer::diagnostics() const noexcept
    {
        return m_diagnostics;
    }

    void Lexer::lex()
    {
        m_tokens.clear();
        m_diagnostics.clear();
        m_current = 0;
        m_location = {1, 1};

        while (!isAtEnd())
        {
            const char ch = peek();
            if (std::isspace(static_cast<unsigned char>(ch)))
            {
                advance();
                if (ch == '\n')
                {
                    advanceLine();
                }
                continue;
            }

            const SourceLocation startLocation = m_location;

            if (isIdentifierStart(ch))
            {
                lexIdentifierOrKeyword();
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(ch)))
            {
                lexNumber();
                continue;
            }

            switch (ch)
            {
            case '"':
                lexString();
                break;
            case '[':
                emitSingle(TokenKind::LeftBracket);
                break;
            case ']':
                emitSingle(TokenKind::RightBracket);
                break;
            case '(':
                emitSingle(TokenKind::LeftParen);
                break;
            case ')':
                emitSingle(TokenKind::RightParen);
                break;
            case '{':
                emitSingle(TokenKind::LeftBrace);
                break;
            case '}':
                emitSingle(TokenKind::RightBrace);
                break;
            case ',':
                emitSingle(TokenKind::Comma);
                break;
            case ':':
                emitSingle(TokenKind::Colon);
                break;
            case ';':
                emitSingle(TokenKind::Semicolon);
                break;
            case '.':
                emitSingle(TokenKind::Dot);
                break;
            case '+':
            {
                advance();
                if (match('+'))
                {
                    pushToken(TokenKind::PlusPlus, startLocation, m_location, "++");
                }
                else if (match('='))
                {
                    pushToken(TokenKind::PlusEquals, startLocation, m_location, "+=");
                }
                else
                {
                    pushToken(TokenKind::Plus, startLocation, m_location, "+");
                }
                break;
            }
            case '-':
                advance();
                if (match('>'))
                {
                    pushToken(TokenKind::Arrow, startLocation, m_location, "->");
                }
                else if (match('-'))
                {
                    pushToken(TokenKind::MinusMinus, startLocation, m_location, "--");
                }
                else if (match('='))
                {
                    pushToken(TokenKind::MinusEquals, startLocation, m_location, "-=");
                }
                else
                {
                    pushToken(TokenKind::Minus, startLocation, m_location, "-");
                }
                break;
            case '=':
                advance();
                if (match('='))
                {
                    pushToken(TokenKind::EqualsEquals, startLocation, m_location, "==");
                }
                else
                {
                    pushToken(TokenKind::Equals, startLocation, m_location, "=");
                }
                break;
            case '!':
                advance();
                if (match('='))
                {
                    pushToken(TokenKind::BangEquals, startLocation, m_location, "!=");
                }
                else
                {
                    pushToken(TokenKind::Bang, startLocation, m_location, "!");
                }
                break;
            case '<':
                advance();
                if (match('='))
                {
                    pushToken(TokenKind::LessEquals, startLocation, m_location, "<=");
                }
                else
                {
                    pushToken(TokenKind::LessThan, startLocation, m_location, "<");
                }
                break;
            case '>':
                advance();
                if (match('='))
                {
                    pushToken(TokenKind::GreaterEquals, startLocation, m_location, ">=");
                }
                else
                {
                    pushToken(TokenKind::GreaterThan, startLocation, m_location, ">");
                }
                break;
            case '*':
                emitSingle(TokenKind::Asterisk);
                break;
            case '/':
                lexSlashOrComment();
                break;
            case '%':
                emitSingle(TokenKind::Percent);
                break;
            case '&':
            {
                advance();
                if (match('&'))
                {
                    pushToken(TokenKind::AmpersandAmpersand, startLocation, m_location, "&&");
                }
                else
                {
                    pushToken(TokenKind::Ampersand, startLocation, m_location, "&");
                }
                break;
            }
            case '|':
                emitSingle(TokenKind::Pipe);
                break;
            case '^':
                emitSingle(TokenKind::Caret);
                break;
            case '~':
                emitSingle(TokenKind::Tilde);
                break;
            default:
                advance();
                Diagnostic diag;
                diag.code = "BOLT-E2000";
                diag.message = "Unexpected character in source.";
                diag.span = {startLocation, m_location};
                m_diagnostics.emplace_back(std::move(diag));
                break;
            }
        }

        SourceLocation eofLocation = m_location;
        pushToken(TokenKind::EndOfFile, eofLocation, eofLocation, "");
    }

    void Lexer::pushToken(TokenKind kind, SourceLocation start, SourceLocation end, std::string_view text)
    {
        Token token;
        token.kind = kind;
        token.span = {start, end};
        token.text = std::string{text};
        m_tokens.emplace_back(std::move(token));
    }

    void Lexer::lexIdentifierOrKeyword()
    {
        const std::size_t startIndex = m_current;
        const SourceLocation startLocation = m_location;

        advance(); // consume first character
        while (isIdentifierPart(peek()))
        {
            advance();
        }

        const std::size_t endIndex = m_current;
        const SourceLocation endLocation = m_location;
        const std::string_view text = m_source.substr(startIndex, endIndex - startIndex);
        const TokenKind kind = keywordLookup(text);

        if (text.find('_') != std::string_view::npos || text.find('-') != std::string_view::npos)
        {
            Diagnostic diag;
            diag.code = "BOLT-E2001";
            diag.message = "Identifiers must avoid underscores and hyphens.";
            diag.span = {startLocation, endLocation};
            m_diagnostics.emplace_back(std::move(diag));
        }

        pushToken(kind, startLocation, endLocation, text);
    }

    void Lexer::lexNumber()
    {
        const std::size_t startIndex = m_current;
        const SourceLocation startLocation = m_location;

        advance(); // consume first digit

        if (peek() == 'x' || peek() == 'X')
        {
            advance();
            while (std::isxdigit(static_cast<unsigned char>(peek())))
            {
                advance();
            }
        }
        else if (peek() == 'b' || peek() == 'B')
        {
            advance();
            while (peek() == '0' || peek() == '1')
            {
                advance();
            }
        }
        else
        {
            while (std::isdigit(static_cast<unsigned char>(peek())))
            {
                advance();
            }
        }

        const std::size_t endIndex = m_current;
        const SourceLocation endLocation = m_location;
        const std::string_view text = m_source.substr(startIndex, endIndex - startIndex);
        pushToken(TokenKind::IntegerLiteral, startLocation, endLocation, text);
    }

    void Lexer::lexString()
    {
        const std::size_t startIndex = m_current;
        const SourceLocation startLocation = m_location;

        advance(); // consume opening quote
        bool closed = false;
        while (!isAtEnd())
        {
            const char ch = advance();
            if (ch == '"')
            {
                closed = true;
                break;
            }
            if (ch == '\n')
            {
                advanceLine();
            }
            if (ch == '\\' && !isAtEnd())
            {
                advance(); // skip escaped char
            }
        }

        if (!closed)
        {
            Diagnostic diag;
            diag.code = "BOLT-E2002";
            diag.message = "Unterminated string literal.";
            diag.span = {startLocation, m_location};
            m_diagnostics.emplace_back(std::move(diag));
            return;
        }

        const std::size_t endIndex = m_current;
        const SourceLocation endLocation = m_location;
        const std::string_view text = m_source.substr(startIndex + 1, (endIndex - startIndex) - 2);
        pushToken(TokenKind::StringLiteral, startLocation, endLocation, text);
    }

    void Lexer::lexSlashOrComment()
    {
        const SourceLocation startLocation = m_location;
        advance(); // consume '/'

        if (match('/'))
        {
            while (!isAtEnd() && peek() != '\n')
            {
                advance();
            }
            return;
        }

        if (match('*'))
        {
            while (!isAtEnd())
            {
                if (peek() == '\n')
                {
                    advance();
                    advanceLine();
                    continue;
                }

                if (peek() == '*' && peekNext() == '/')
                {
                    advance();
                    advance();
                    return;
                }

                advance();
            }

            Diagnostic diag;
            diag.code = "BOLT-E2003";
            diag.message = "Unterminated block comment.";
            diag.span = {startLocation, m_location};
            m_diagnostics.emplace_back(std::move(diag));
            return;
        }

        pushToken(TokenKind::Slash, startLocation, m_location, "/");
    }

    void Lexer::emitSingle(TokenKind kind)
    {
        const SourceLocation startLocation = m_location;
        advance();
        pushToken(kind, startLocation, m_location, m_source.substr(m_current - 1, 1));
    }

    bool Lexer::match(char expected)
    {
        if (isAtEnd()) return false;
        if (m_source[m_current] != expected) return false;
        advance();
        return true;
    }

    char Lexer::peek() const
    {
        if (isAtEnd()) return '\0';
        return m_source[m_current];
    }

    char Lexer::peekNext() const
    {
        if (m_current + 1 >= m_source.size()) return '\0';
        return m_source[m_current + 1];
    }

    char Lexer::advance()
    {
        const char ch = m_source[m_current++];
        if (ch == '\n')
        {
            // newline handled by caller through advanceLine
        }
        else
        {
            ++m_location.column;
        }
        return ch;
    }

    bool Lexer::isAtEnd() const
    {
        return m_current >= m_source.size();
    }

    void Lexer::advanceLine()
    {
        ++m_location.line;
        m_location.column = 1;
    }
} // namespace bolt::frontend

