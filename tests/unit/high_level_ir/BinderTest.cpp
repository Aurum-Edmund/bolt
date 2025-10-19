#include <gtest/gtest.h>

#include <string>

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
public function request(param: LiveValue integer32) -> LiveValue integer32 {
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
        ASSERT_EQ(fn.modifiers.size(), 1u);
        EXPECT_EQ(fn.modifiers.front(), "public");
        ASSERT_TRUE(fn.alignmentBytes.has_value());
        EXPECT_EQ(*fn.alignmentBytes, 16u);
        ASSERT_TRUE(fn.systemRequestId.has_value());
        EXPECT_EQ(*fn.systemRequestId, 2u);
        EXPECT_TRUE(fn.kernelMarkers.empty());
        EXPECT_TRUE(fn.returnIsLiveValue);
        EXPECT_EQ(fn.returnType.text, "integer32");
        ASSERT_EQ(fn.parameters.size(), 1u);
        EXPECT_EQ(fn.parameters.front().name, "param");
        EXPECT_EQ(fn.parameters.front().type.text, "integer32");
        EXPECT_TRUE(fn.parameters.front().isLiveValue);
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

    TEST(BinderTest, CapturesBlueprintMetadata)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

[packed]
[aligned(64)]
public blueprint Timer {
    start: LiveValue integer32;
    [bits(8)] mode: integer32;
    [aligned(16)] [bits(4)] priority: integer32;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty());

        ASSERT_EQ(module.blueprints.size(), 1u);
        const auto& bp = module.blueprints.front();
        EXPECT_EQ(bp.name, "Timer");
        ASSERT_EQ(bp.modifiers.size(), 1u);
        EXPECT_EQ(bp.modifiers.front(), "public");
        EXPECT_TRUE(bp.isPacked);
        ASSERT_TRUE(bp.alignmentBytes.has_value());
        EXPECT_EQ(*bp.alignmentBytes, 64u);
        ASSERT_EQ(bp.fields.size(), 3u);

        const auto& startField = bp.fields[0];
        EXPECT_EQ(startField.name, "start");
        EXPECT_EQ(startField.type.text, "integer32");
        EXPECT_TRUE(startField.isLiveValue);
        EXPECT_FALSE(startField.bitWidth.has_value());

        const auto& modeField = bp.fields[1];
        EXPECT_EQ(modeField.name, "mode");
        ASSERT_TRUE(modeField.bitWidth.has_value());
        EXPECT_EQ(*modeField.bitWidth, 8u);

        const auto& prioField = bp.fields[2];
        ASSERT_TRUE(prioField.bitWidth.has_value());
        EXPECT_EQ(*prioField.bitWidth, 4u);
        ASSERT_TRUE(prioField.alignmentBytes.has_value());
        EXPECT_EQ(*prioField.alignmentBytes, 16u);
    }
}
} // namespace bolt::hir
