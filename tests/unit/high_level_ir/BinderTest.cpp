#include <gtest/gtest.h>

#include "lexer.hpp"
#include "parser.hpp"
#include "binder.hpp"

namespace
{
    bolt::frontend::CompilationUnit parseCompilationUnit(const std::string& source,
                                                          std::vector<bolt::frontend::Diagnostic>& diagnostics)
    {
        bolt::frontend::Lexer lexer{source, "binder-test"};
        lexer.lex();

        bolt::frontend::Parser parser{lexer.tokens(), "binder-test"};
        bolt::frontend::CompilationUnit unit = parser.parse();
        diagnostics = parser.diagnostics();
        return unit;
    }
}

namespace bolt::hir
{
namespace
{
    TEST(BinderTest, CapturesFunctionMetadata)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

[aligned(16)]
[systemRequest(identifier=2)]
public function request(param: integer32) -> integer32 {
    return;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty()) << "Parser diagnostics detected";

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty()) << "Binder diagnostics detected";

        ASSERT_EQ(module.functions.size(), 1u);
        const auto& fn = module.functions.front();
        EXPECT_EQ(fn.name, "request");
        EXPECT_EQ(fn.modifiers.size(), 1u);
        EXPECT_EQ(fn.modifiers.front(), "public");
        ASSERT_TRUE(fn.alignmentBytes.has_value());
        EXPECT_EQ(*fn.alignmentBytes, 16u);
        ASSERT_TRUE(fn.systemRequestId.has_value());
        EXPECT_EQ(*fn.systemRequestId, 2u);
        ASSERT_EQ(fn.parameters.size(), 1u);
        EXPECT_EQ(fn.parameters.front().name, "param");
        EXPECT_EQ(fn.parameters.front().type.text, "integer32");
    }

    TEST(BinderTest, DuplicateFunctionAttributeEmitsDiagnostic)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

[aligned(16)]
[aligned(8)]
function badAlign() {
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        (void)binder.bind();
        const auto& diags = binder.diagnostics();
        ASSERT_FALSE(diags.empty());
        EXPECT_EQ(diags.front().code, "BOLT-E2200");
    }
}
} // namespace bolt::hir
