#include "parser.hpp"`r`n`r`n#include <sstream>`r`n#include <cctype>
#include <cctype>

namespace bolt::frontend
{
    Parser::Parser(const std::vector<Token>& tokens, std::string_view moduleName)
        : m_tokens(tokens)
        , m_moduleName(moduleName)
    {
    }

    CompilationUnit Parser::parse()
    {
        m_current = 0;
        m_diagnostics.clear();

        CompilationUnit unit{};
        unit.module = parseModule();

        while (!isAtEnd())
        {
            if (check(TokenKind::EndOfFile))
            {
                break;
            }

            std::vector<Attribute> attributes;
            if (check(TokenKind::LeftBracket))
            {
                attributes = parseAttributes();
            }

            std::vector<std::string> modifiers = parseModifiers();

            if (check(TokenKind::KeywordImport))
            {
                if (!attributes.empty())
                {
                    Diagnostic diag;
                    diag.code = "BOLT-E2108";
                    diag.message = "Attributes are not allowed on import statements.";
                    diag.span = attributes.front().span;
                    m_diagnostics.emplace_back(std::move(diag));
                }

                if (!modifiers.empty())
                {
                    Diagnostic diag;
                    diag.code = "BOLT-E2109";
                    diag.message = "Modifiers are not allowed before an import statement.";
                    diag.span = previous().span;
                    m_diagnostics.emplace_back(std::move(diag));
                }

                ImportDeclaration importDecl = parseImport();
                unit.imports.emplace_back(std::move(importDecl));
                continue;
            }

            if (match(TokenKind::KeywordBlueprint))
            {
                BlueprintDeclaration bp = parseBlueprint(std::move(modifiers));
                bp.attributes = std::move(attributes);
                unit.blueprints.emplace_back(std::move(bp));
                continue;
            }

            TypeCapture returnTypeCapture = parseTypeUntil({TokenKind::KeywordFunction});
            if (!check(TokenKind::KeywordFunction))
            {
                Diagnostic diag;
                diag.code = "BOLT-E2115";
                diag.message = "Expected return type followed by 'function' declaration.";
                diag.span = peek().span;
                m_diagnostics.emplace_back(std::move(diag));

                if (!isAtEnd())
                {
                    advance();
                }
                continue;
            }

            consume(TokenKind::KeywordFunction, "BOLT-E2116", "Expected 'function' keyword after return type.");

            FunctionDeclaration fn = parseFunction(std::move(modifiers), returnTypeCapture);
            fn.attributes = std::move(attributes);
            unit.functions.emplace_back(std::move(fn));
            continue;
        }

        return unit;
    }

    const std::vector<Diagnostic>& Parser::diagnostics() const noexcept
    {
        return m_diagnostics;
    }

    const Token& Parser::peek() const
    {
        return m_tokens[m_current];
    }

    const Token& Parser::previous() const
    {
        return m_tokens[m_current - 1];
    }

    const Token& Parser::advance()
    {
        if (!isAtEnd())
        {
            ++m_current;
        }

        const std::size_t index = (m_current == 0) ? 0 : (m_current - 1);
        return m_tokens[index];
    }

    const Token& Parser::lookAhead(std::size_t offset) const
    {
        const std::size_t index = m_current + offset;
        if (index >= m_tokens.size())
        {
            return m_tokens.back();
        }
        return m_tokens[index];
    }

    bool Parser::isAtEnd() const
    {
        return peek().kind == TokenKind::EndOfFile;
    }

    bool Parser::check(TokenKind kind) const
    {
        if (isAtEnd()) return false;
        return peek().kind == kind;
    }

    bool Parser::match(TokenKind kind)
    {
        if (check(kind))
        {
            advance();
            return true;
        }
        return false;
    }

    const Token& Parser::consume(TokenKind kind, std::string_view messageCode, std::string_view messageText)
    {
        if (check(kind))
        {
            return advance();
        }

        Diagnostic diag;
        diag.code = std::string{messageCode};
        diag.message = std::string{messageText};
        diag.span = peek().span;
        m_diagnostics.emplace_back(std::move(diag));

        if (!isAtEnd())
        {
            advance();
        }

        return previous();
    }

    SourceSpan Parser::spanFrom(const Token& begin, const Token& end) const
    {
        SourceSpan span{};
        span.begin = begin.span.begin;
        span.end = end.span.end;
        return span;
    }

    bool Parser::isTerminator(TokenKind kind, std::initializer_list<TokenKind> terminators, int angleDepth) const
    {
        for (TokenKind terminator : terminators)
        {
            if (kind != terminator)
            {
                continue;
            }

            if ((terminator == TokenKind::Comma || terminator == TokenKind::RightParen) && angleDepth > 0)
            {
                return false;
            }

            return true;
        }
        return false;
    }

    ModuleDeclaration Parser::parseModule()
    {
        ModuleDeclaration module{};
        SourceSpan span{};
        bool packageSpecified = false;

        if (match(TokenKind::KeywordPackage))
        {
            const Token& keyword = previous();
            const auto packageName = parseQualifiedName("BOLT-E2103", "Expected package identifier.");
            module.packageName = packageName.first;
            const Token& terminator = consume(TokenKind::Semicolon, "BOLT-E2104", "Expected ';' after package declaration.");
            span.begin = keyword.span.begin;
            span.end = terminator.span.end;
            packageSpecified = true;
        }
        else
        {
            Diagnostic diag;
            diag.code = "BOLT-E2102";
            diag.message = "Missing 'package' declaration at file start.";
            diag.span = peek().span;
            m_diagnostics.emplace_back(std::move(diag));
        }

        if (match(TokenKind::KeywordModule))
        {
            const Token& keyword = previous();
            if (!packageSpecified)
            {
                span.begin = keyword.span.begin;
            }
            const auto moduleName = parseQualifiedName("BOLT-E2105", "Expected module identifier.");
            module.moduleName = moduleName.first;
            const Token& terminator = consume(TokenKind::Semicolon, "BOLT-E2106", "Expected ';' after module declaration.");
            span.end = terminator.span.end;
        }
        else
        {
            Diagnostic diag;
            diag.code = "BOLT-E2105";
            diag.message = "Missing 'module' declaration.";
            diag.span = peek().span;
            m_diagnostics.emplace_back(std::move(diag));
        }

        module.span = span;
        if (!packageSpecified)
        {
            module.packageName = module.moduleName;
        }

        return module;
    }

    ImportDeclaration Parser::parseImport()
    {
        ImportDeclaration importDecl{};

        const Token& keyword = advance();
        importDecl.span.begin = keyword.span.begin;
        importDecl.span.end = keyword.span.end;

        auto [path, pathSpan] = parseQualifiedName("BOLT-E2107", "Expected module path after 'import'.");
        if (!path.empty())
        {
            importDecl.modulePath = std::move(path);
            if (pathSpan.begin.line != 0 || pathSpan.begin.column != 0)
            {
                importDecl.span.end = pathSpan.end;
            }
        }

        if (match(TokenKind::Semicolon))
        {
            importDecl.span.end = previous().span.end;
        }

        return importDecl;
    }

    std::vector<std::string> Parser::parseModifiers()
    {
        std::vector<std::string> modifiers;
        while (check(TokenKind::KeywordPublic)
               || check(TokenKind::KeywordLink)
               || check(TokenKind::KeywordExtern))
        {
            const Token& token = advance();
            modifiers.emplace_back(token.text);
        }
        return modifiers;
    }

    FunctionDeclaration Parser::parseFunction(std::vector<std::string> modifiers, TypeCapture returnTypeCapture)
    {
        FunctionDeclaration fn{};
        fn.modifiers = std::move(modifiers);

        if (returnTypeCapture.valid)
        {
            fn.returnType = returnTypeCapture.text;
            fn.returnTypeSpan = returnTypeCapture.span;
        }
        else
        {
            Diagnostic diag;
            diag.code = "BOLT-E2117";
            diag.message = "Expected return type before 'function'.";
            diag.span = previous().span;
            m_diagnostics.emplace_back(std::move(diag));
        }

        const Token& nameToken = consume(TokenKind::Identifier, "BOLT-E2110", "Expected function name.");
        fn.name = nameToken.text;

        consume(TokenKind::LeftParen, "BOLT-E2111", "Expected '(' after function name.");

        while (!check(TokenKind::RightParen) && !isAtEnd())
        {
            Parameter parameter = parseParameter();
            fn.parameters.emplace_back(std::move(parameter));

            if (!match(TokenKind::Comma))
            {
                break;
            }
        }

        consume(TokenKind::RightParen, "BOLT-E2112", "Expected ')' after parameters.");

        if (match(TokenKind::Arrow))
        {
            Diagnostic diag;
            diag.code = "BOLT-E2118";
            diag.message = "Return types must appear before 'function'.";
            diag.span = previous().span;
            m_diagnostics.emplace_back(std::move(diag));
            (void)parseTypeUntil({TokenKind::LeftBrace});
        }

        const Token& bodyStart = consume(TokenKind::LeftBrace, "BOLT-E2114", "Expected '{' to begin function body.");
        int depth = 1;
        SourceSpan bodySpan = bodyStart.span;

        while (!isAtEnd() && depth > 0)
        {
            const Token& token = advance();
            if (token.kind == TokenKind::LeftBrace)
            {
                ++depth;
            }
            else if (token.kind == TokenKind::RightBrace)
            {
                --depth;
                bodySpan.end = token.span.end;
            }
        }

        if (depth != 0)
        {
            Diagnostic diag;
            diag.code = "BOLT-E2115";
            diag.message = "Unterminated function body.";
            diag.span = bodyStart.span;
            m_diagnostics.emplace_back(std::move(diag));
        }

        fn.span = mergeSpans(nameToken.span, bodySpan);
        return fn;
    }

    BlueprintDeclaration Parser::parseBlueprint(std::vector<std::string> modifiers)
    {
        BlueprintDeclaration bp{};
        bp.modifiers = std::move(modifiers);

        const Token& nameToken = consume(TokenKind::Identifier, "BOLT-E2120", "Expected blueprint name.");
        bp.name = nameToken.text;

        const Token& openBrace = consume(TokenKind::LeftBrace, "BOLT-E2121", "Expected '{' after blueprint name.");

        while (!check(TokenKind::RightBrace) && !isAtEnd())
        {
            std::vector<Attribute> attributes;
            if (check(TokenKind::LeftBracket))
            {
                attributes = parseAttributes();
            }

            BlueprintField field = parseField();
            field.attributes = std::move(attributes);
            bp.fields.emplace_back(std::move(field));

            match(TokenKind::Semicolon);
        }

        const Token& closing = consume(TokenKind::RightBrace, "BOLT-E2122", "Expected '}' to close blueprint.");
        bp.span = mergeSpans(openBrace.span, closing.span);
        bp.span.begin = nameToken.span.begin;
        return bp;
    }

    std::vector<Attribute> Parser::parseAttributes()
    {
        std::vector<Attribute> attributes;
        while (match(TokenKind::LeftBracket))
        {
            attributes.emplace_back(parseAttribute());
            consume(TokenKind::RightBracket, "BOLT-E2130", "Expected ']' after attribute.");
        }
        return attributes;
    }

    Attribute Parser::parseAttribute()
    {
        Attribute attribute{};
        const Token& nameToken = consume(TokenKind::Identifier, "BOLT-E2131", "Expected attribute identifier.");
        attribute.name = nameToken.text;
        attribute.span.begin = nameToken.span.begin;

        if (match(TokenKind::LeftParen))
        {
            while (!check(TokenKind::RightParen) && !isAtEnd())
            {
                attribute.arguments.emplace_back(parseAttributeArgument());
                if (!match(TokenKind::Comma))
                {
                    break;
                }
            }
            consume(TokenKind::RightParen, "BOLT-E2132", "Expected ')' after attribute arguments.");
        }

        attribute.span.end = previous().span.end;
        return attribute;
    }

    AttributeArgument Parser::parseAttributeArgument()
    {
        AttributeArgument argument{};

        if (!(check(TokenKind::Identifier) || check(TokenKind::IntegerLiteral) || check(TokenKind::StringLiteral)))
        {
            Diagnostic diag;
            diag.code = "BOLT-E2133";
            diag.message = "Expected attribute argument.";
            diag.span = peek().span;
            m_diagnostics.emplace_back(std::move(diag));
            return argument;
        }

        const Token& firstToken = advance();

        if (match(TokenKind::Equals))
        {
            if (firstToken.kind != TokenKind::Identifier)
            {
                Diagnostic diag;
                diag.code = "BOLT-E2134";
                diag.message = "Named attribute argument must start with an identifier.";
                diag.span = firstToken.span;
                m_diagnostics.emplace_back(std::move(diag));
            }

            if (!(check(TokenKind::Identifier) || check(TokenKind::IntegerLiteral) || check(TokenKind::StringLiteral)))
            {
                Diagnostic diag;
                diag.code = "BOLT-E2134";
                diag.message = "Expected value after '=' in attribute argument.";
                diag.span = peek().span;
                m_diagnostics.emplace_back(std::move(diag));
                return argument;
            }

            const Token& valueToken = advance();
            argument.name = firstToken.text;
            argument.value = valueToken.text;
            argument.span = spanFrom(firstToken, valueToken);
        }
        else
        {
            argument.value = firstToken.text;
            argument.span = firstToken.span;
        }

        return argument;
    }

    Parameter Parser::parseParameter()
    {
        Parameter parameter{};

        if (check(TokenKind::Identifier) && lookAhead(1).kind == TokenKind::Colon)
        {
            const Token& legacyName = advance();
            advance();

            Diagnostic diag;
            diag.code = "BOLT-E2115";
            diag.message = "Parameters must use type-first syntax (for example, 'integer value').";
            diag.span = legacyName.span;
            m_diagnostics.emplace_back(std::move(diag));

            TypeCapture legacyType = parseTypeUntil({TokenKind::Comma, TokenKind::RightParen});
            if (legacyType.valid)
            {
                parameter.typeName = legacyType.text;
                parameter.typeSpan = legacyType.span;
                parameter.span = mergeSpans(legacyName.span, legacyType.span);
            }
            else
            {
                parameter.typeSpan = legacyName.span;
                parameter.span = legacyName.span;
            }

            parameter.name = legacyName.text;
            return parameter;
        }

        TypeCapture typeCapture = parseTypeBeforeName({TokenKind::Comma, TokenKind::RightParen});
        if (!typeCapture.valid)
        {
            Diagnostic diag;
            diag.code = "BOLT-E2142";
            diag.message = "Expected parameter type before name.";
            diag.span = peek().span;
            m_diagnostics.emplace_back(std::move(diag));
        }
        else
        {
            parameter.typeName = typeCapture.text;
            parameter.typeSpan = typeCapture.span;
        }

        const Token* nameToken = nullptr;
        if (check(TokenKind::Identifier))
        {
            nameToken = &advance();
            parameter.name = nameToken->text;
        }
        else
        {
            Diagnostic diag;
            diag.code = "BOLT-E2143";
            diag.message = "Expected parameter name after type.";
            diag.span = peek().span;
            m_diagnostics.emplace_back(std::move(diag));
        }

        if (typeCapture.valid && nameToken != nullptr)
        {
            parameter.span = mergeSpans(typeCapture.span, nameToken->span);
        }
        else if (nameToken != nullptr)
        {
            parameter.span = nameToken->span;
        }
        else if (typeCapture.valid)
        {
            parameter.span = typeCapture.span;
        }

        return parameter;
    }

    BlueprintField Parser::parseField()
    {
        BlueprintField field{};

        if (check(TokenKind::Identifier) && lookAhead(1).kind == TokenKind::Colon)
        {
            const Token& legacyName = advance();
            advance();

            Diagnostic diag;
            diag.code = "BOLT-E2153";
            diag.message = "Fields must use type-first syntax (for example, 'integer value').";
            diag.span = legacyName.span;
            m_diagnostics.emplace_back(std::move(diag));

            TypeCapture legacyType = parseTypeUntil({TokenKind::Semicolon, TokenKind::RightBrace, TokenKind::LeftBracket});
            if (legacyType.valid)
            {
                field.typeName = legacyType.text;
                field.typeSpan = legacyType.span;
                field.span = mergeSpans(legacyName.span, legacyType.span);
            }
            else
            {
                field.typeSpan = legacyName.span;
                field.span = legacyName.span;
            }

            field.name = legacyName.text;
            return field;
        }

        TypeCapture typeCapture = parseTypeBeforeName({TokenKind::Semicolon, TokenKind::RightBrace, TokenKind::LeftBracket});
        if (!typeCapture.valid)
        {
            Diagnostic diag;
            diag.code = "BOLT-E2152";
            diag.message = "Expected field type before name.";
            diag.span = peek().span;
            m_diagnostics.emplace_back(std::move(diag));
        }
        else
        {
            field.typeName = typeCapture.text;
            field.typeSpan = typeCapture.span;
        }

        const Token* nameToken = nullptr;
        if (check(TokenKind::Identifier))
        {
            nameToken = &advance();
            field.name = nameToken->text;
        }
        else
        {
            Diagnostic diag;
            diag.code = "BOLT-E2153";
            diag.message = "Expected field name after type.";
            diag.span = peek().span;
            m_diagnostics.emplace_back(std::move(diag));
        }

        if (typeCapture.valid && nameToken != nullptr)
        {
            field.span = mergeSpans(typeCapture.span, nameToken->span);
        }
        else if (nameToken != nullptr)
        {
            field.span = nameToken->span;
        }
        else if (typeCapture.valid)
        {
            field.span = typeCapture.span;
        }

        return field;
    }

    std::pair<std::string, SourceSpan> Parser::parseQualifiedName(std::string_view code, std::string_view message)
    {
        std::pair<std::string, SourceSpan> result{};

        if (!check(TokenKind::Identifier))
        {
            Diagnostic diag;
            diag.code = std::string{code};
            diag.message = std::string{message};
            diag.span = peek().span;
            m_diagnostics.emplace_back(std::move(diag));
            return result;
        }

        const Token& first = advance();
        result.first = first.text;
        result.second.begin = first.span.begin;
        result.second.end = first.span.end;

        while (match(TokenKind::Dot))
        {
            const Token& dot = previous();
            result.first.append(dot.text);

            if (!check(TokenKind::Identifier))
            {
                Diagnostic diag;
                diag.code = std::string{code};
                diag.message = "Expected identifier segment after '.'.";
                diag.span = peek().span;
                m_diagnostics.emplace_back(std::move(diag));
                break;
            }

            const Token& part = advance();
            result.first.append(part.text);
            result.second.end = part.span.end;
        }

        return result;
    }

    SourceSpan Parser::mergeSpans(const SourceSpan& a, const SourceSpan& b) const
    {
        SourceSpan span = a;
        if (span.begin.line == 0 && span.begin.column == 0)
        {
            span.begin = b.begin;
        }
        if (b.begin.line < span.begin.line || (b.begin.line == span.begin.line && b.begin.column < span.begin.column))
        {
            span.begin = b.begin;
        }
        if (b.end.line > span.end.line || (b.end.line == span.end.line && b.end.column > span.end.column))
        {
            span.end = b.end;
        }
        return span;
    }

    Parser::TypeCapture Parser::parseTypeBeforeName(std::initializer_list<TokenKind> terminators)
    {
        TypeCapture capture{};
        bool lastWasPunctuation = true;
        int angleDepth = 0;

        while (!isAtEnd())
        {
            const Token& token = peek();

            if (token.kind == TokenKind::Identifier)
            {
                const Token& next = lookAhead(1);
                if (next.kind == TokenKind::Comma
                    || next.kind == TokenKind::RightParen
                    || next.kind == TokenKind::Semicolon
                    || next.kind == TokenKind::RightBrace
                    || next.kind == TokenKind::Equals
                    || next.kind == TokenKind::LeftBracket
                    || next.kind == TokenKind::EndOfFile)
                {
                    break;
                }
            }

            if (isTerminator(token.kind, terminators, angleDepth))
            {
                break;
            }

            const Token& consumed = advance();

            if (consumed.kind == TokenKind::LessThan)
            {
                ++angleDepth;
            }
            else if (consumed.kind == TokenKind::GreaterThan && angleDepth > 0)
            {
                --angleDepth;
            }

            if (consumed.text.empty())
            {
                continue;
            }

            const bool punctuation = consumed.kind == TokenKind::LessThan
                                     || consumed.kind == TokenKind::GreaterThan
                                     || consumed.kind == TokenKind::Dot
                                     || consumed.kind == TokenKind::Ampersand
                                     || consumed.kind == TokenKind::Asterisk
                                     || consumed.kind == TokenKind::LeftBracket
                                     || consumed.kind == TokenKind::RightBracket
                                     || consumed.kind == TokenKind::Comma
                                     || consumed.kind == TokenKind::Colon;

            if (!capture.valid)
            {
                capture.span.begin = consumed.span.begin;
            }

            if (!capture.text.empty() && !punctuation && !lastWasPunctuation)
            {
                capture.text.push_back(' ');
            }

            capture.text.append(consumed.text);
            capture.span.end = consumed.span.end;
            capture.valid = true;
            lastWasPunctuation = punctuation;
        }

        return capture;
    }

    Parser::TypeCapture Parser::parseTypeUntil(std::initializer_list<TokenKind> terminators)
    {
        TypeCapture capture{};
        bool lastWasPunctuation = true;
        int angleDepth = 0;

        while (!isAtEnd())
        {
            const Token& token = peek();

            if (token.kind == TokenKind::GreaterThan && angleDepth > 0)
            {
                // allow terminator checks after reducing depth below.
            }
            if (isTerminator(token.kind, terminators, angleDepth))
            {
                break;
            }

            const Token& consumed = advance();

            if (consumed.kind == TokenKind::LessThan)
            {
                ++angleDepth;
            }
            else if (consumed.kind == TokenKind::GreaterThan && angleDepth > 0)
            {
                --angleDepth;
            }

            if (consumed.text.empty())
            {
                continue;
            }

            const bool punctuation = consumed.kind == TokenKind::LessThan
                                     || consumed.kind == TokenKind::GreaterThan
                                     || consumed.kind == TokenKind::Dot
                                     || consumed.kind == TokenKind::Ampersand
                                     || consumed.kind == TokenKind::Asterisk
                                     || consumed.kind == TokenKind::LeftBracket
                                     || consumed.kind == TokenKind::RightBracket
                                     || consumed.kind == TokenKind::Comma
                                     || consumed.kind == TokenKind::Colon;

            if (!capture.valid)
            {
                capture.span.begin = consumed.span.begin;
            }

            if (!capture.text.empty() && !punctuation && !lastWasPunctuation)
            {
                capture.text.push_back(' ');
            }

            capture.text.append(consumed.text);
            capture.span.end = consumed.span.end;
            capture.valid = true;
            lastWasPunctuation = punctuation;
        }

        return capture;
    }
} // namespace bolt::frontend


