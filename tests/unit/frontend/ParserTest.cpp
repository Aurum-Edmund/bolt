#include <gtest/gtest.h>

#include "lexer.hpp"
#include "parser.hpp"

namespace bolt::frontend
{
namespace
{
    CompilationUnit parseSource(const std::string& source, std::vector<Diagnostic>& outDiagnostics)
    {
        Lexer lexer{source, "test"};
        lexer.lex();

        Parser parser{lexer.tokens(), "test"};
        CompilationUnit unit = parser.parse();
        outDiagnostics = parser.diagnostics();
        return unit;
    }

    TEST(ParserTest, ParsesFunctionSignature)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public function sample(integer32 value) -> integer32 {
    return;
}
)";

        std::vector<Diagnostic> diagnostics;
        CompilationUnit unit = parseSource(source, diagnostics);

        EXPECT_TRUE(diagnostics.empty());
        ASSERT_EQ(unit.functions.size(), 1u);
        const auto& fn = unit.functions.front();
        EXPECT_EQ(fn.name, "sample");
        ASSERT_EQ(fn.parameters.size(), 1u);
        EXPECT_EQ(fn.parameters.front().name, "value");
        EXPECT_TRUE(fn.returnType.has_value());
        EXPECT_EQ(*fn.returnType, "integer32");
    }

    TEST(ParserTest, ParsesImportStatement)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

import demo.utils.core;

public function dummy() {
    return;
}
)";

        std::vector<Diagnostic> diagnostics;
        CompilationUnit unit = parseSource(source, diagnostics);

        EXPECT_TRUE(diagnostics.empty());
        ASSERT_EQ(unit.imports.size(), 1u);
        EXPECT_EQ(unit.imports.front().modulePath, "demo.utils.core");
    }

    TEST(ParserTest, ReportsAttributesOnImport)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

[packed]
import demo.alpha;
)";

        std::vector<Diagnostic> diagnostics;
        CompilationUnit unit = parseSource(source, diagnostics);

        EXPECT_FALSE(diagnostics.empty());
        EXPECT_EQ(diagnostics.front().code, "BOLT-E2108");
        ASSERT_EQ(unit.imports.size(), 1u);
        EXPECT_EQ(unit.imports.front().modulePath, "demo.alpha");
    }
}
} // namespace bolt::frontend

