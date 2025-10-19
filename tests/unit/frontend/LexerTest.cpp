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
}
} // namespace bolt::frontend
