#include <gtest/gtest.h>

#include "lexer.hpp"

namespace bolt::frontend
{
namespace
{
    TEST(LexerTest, ParsesModuleHeader)
    {
        const std::string source = "package demo; module demo;\n";

        Lexer lexer{source, "test"};
        lexer.lex();

        const auto& tokens = lexer.tokens();
        ASSERT_GE(tokens.size(), 4u);
        EXPECT_EQ(tokens[0].kind, TokenKind::KeywordPackage);
        EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
        EXPECT_EQ(tokens[1].text, "demo");
        EXPECT_EQ(tokens[2].kind, TokenKind::Semicolon);
        EXPECT_EQ(tokens[3].kind, TokenKind::KeywordModule);
    }

    TEST(LexerTest, RecognizesCompoundOperators)
    {
        const std::string source = "value += 1; other--; ++value; if (value && other) { value -= 2; }";

        Lexer lexer{source, "operators"};
        lexer.lex();

        const auto& tokens = lexer.tokens();
        std::size_t plusEqualsCount = 0;
        std::size_t minusMinusCount = 0;
        std::size_t plusPlusCount = 0;
        std::size_t ampersandAmpersandCount = 0;
        std::size_t minusEqualsCount = 0;

        for (const auto& token : tokens)
        {
            switch (token.kind)
            {
            case TokenKind::PlusEquals: ++plusEqualsCount; break;
            case TokenKind::MinusMinus: ++minusMinusCount; break;
            case TokenKind::PlusPlus: ++plusPlusCount; break;
            case TokenKind::AmpersandAmpersand: ++ampersandAmpersandCount; break;
            case TokenKind::MinusEquals: ++minusEqualsCount; break;
            default: break;
            }
        }

        EXPECT_EQ(1u, plusEqualsCount);
        EXPECT_EQ(1u, minusMinusCount);
        EXPECT_EQ(1u, plusPlusCount);
        EXPECT_EQ(1u, ampersandAmpersandCount);
        EXPECT_EQ(1u, minusEqualsCount);
    }

    TEST(LexerTest, RecognizesLifecycleKeywords)
    {
        const std::string source = "public constructor function build() {} public destructor function tearDown() {} new delete";

        Lexer lexer{source, "keywords"};
        lexer.lex();

        const auto& tokens = lexer.tokens();
        bool sawConstructor = false;
        bool sawDestructor = false;
        bool sawNew = false;
        bool sawDelete = false;

        for (const auto& token : tokens)
        {
            if (token.kind == TokenKind::KeywordConstructor)
            {
                sawConstructor = true;
            }
            else if (token.kind == TokenKind::KeywordDestructor)
            {
                sawDestructor = true;
            }
            else if (token.kind == TokenKind::KeywordNew)
            {
                sawNew = true;
            }
            else if (token.kind == TokenKind::KeywordDelete)
            {
                sawDelete = true;
            }
        }

        EXPECT_TRUE(sawConstructor);
        EXPECT_TRUE(sawDestructor);
        EXPECT_TRUE(sawNew);
        EXPECT_TRUE(sawDelete);
    }
}
} // namespace bolt::frontend
