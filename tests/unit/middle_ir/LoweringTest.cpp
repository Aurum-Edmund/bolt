#include <gtest/gtest.h>

#include <string>

#include "lexer.hpp"
#include "parser.hpp"
#include "binder.hpp"
#include "lowering.hpp"
#include "verifier.hpp"

namespace
{
    bolt::hir::Module buildHir(const std::string& source)
    {
        bolt::frontend::Lexer lexer{source, "lowering-test"};
        lexer.lex();

        bolt::frontend::Parser parser{lexer.tokens(), "lowering-test"};
        auto unit = parser.parse();
        EXPECT_TRUE(parser.diagnostics().empty()) << "Parser diagnostics present";

        bolt::hir::Binder binder{unit, "lowering-test"};
        auto module = binder.bind();
        EXPECT_TRUE(binder.diagnostics().empty()) << "Binder diagnostics present";
        return module;
    }
}

namespace bolt::mir
{
namespace
{
    TEST(LoweringTest, EmitsFunctionDetails)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

[aligned(32)]
[systemRequest(identifier=3)]
public function demoFunc(integer32 value) -> Live integer32 {
    return;
}
)";

        auto hirModule = buildHir(source);
        Module mirModule = lowerFromHir(hirModule);
        ASSERT_TRUE(verify(mirModule));

        ASSERT_EQ(mirModule.functions.size(), 1u);
        const auto& fn = mirModule.functions.front();
        ASSERT_EQ(fn.blocks.size(), 1u);
        const auto& block = fn.blocks.front();
        ASSERT_EQ(block.instructions.size(), 6u);

        EXPECT_EQ(block.instructions[0].kind, InstructionKind::Unary);
        EXPECT_EQ(block.instructions[0].detail, "modifiers: public");

        EXPECT_EQ(block.instructions[1].detail, "aligned 32");
        EXPECT_EQ(block.instructions[2].detail, "systemRequest 3");
        EXPECT_EQ(block.instructions[3].detail, "return integer32 [Live]");
        EXPECT_EQ(block.instructions[4].detail, "param value: integer32 [Live]");

        const auto& terminator = block.instructions.back();
        EXPECT_EQ(terminator.kind, InstructionKind::Return);
        EXPECT_EQ(terminator.detail, "function");
    }

    TEST(LoweringTest, EmitsBlueprintDetails)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

[packed]
[aligned(64)]
public blueprint Timer {
    start: Live integer32;
    [bits(8)] mode: integer32;
    [aligned(16)] [bits(4)] priority: integer32;
}
)";

        auto hirModule = buildHir(source);
        Module mirModule = lowerFromHir(hirModule);
        ASSERT_TRUE(verify(mirModule));

        ASSERT_EQ(mirModule.functions.size(), 1u);
        const auto& fn = mirModule.functions.front();
        EXPECT_EQ(fn.name, "blueprint.Timer");
        ASSERT_EQ(fn.blocks.size(), 1u);
        const auto& block = fn.blocks.front();
        ASSERT_EQ(block.instructions.size(), 7u);

        EXPECT_EQ(block.instructions[0].detail, "modifiers: public");
        EXPECT_EQ(block.instructions[1].detail, "attr packed");
        EXPECT_EQ(block.instructions[2].detail, "aligned 64");
        EXPECT_EQ(block.instructions[3].detail, "field start: integer32 [Live]");
        EXPECT_EQ(block.instructions[4].detail, "field mode: integer32 bits=8");
        EXPECT_EQ(block.instructions[5].detail, "field priority: integer32 bits=4 align=16");

        const auto& terminator = block.instructions.back();
        EXPECT_EQ(terminator.kind, InstructionKind::Return);
        EXPECT_EQ(terminator.detail, "blueprint");
    }
}
} // namespace bolt::mir



