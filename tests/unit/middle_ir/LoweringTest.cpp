#include <gtest/gtest.h>

#include <algorithm>
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
public live integer32 function demoFunc(live integer32 value) {
    return value;
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
        EXPECT_EQ(block.instructions[3].detail, "return integer [live]");
        EXPECT_EQ(block.instructions[4].detail, "param integer value [live]");

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
    live integer32 start;
    [bits(8)] integer32 mode;
    [aligned(16)] [bits(4)] integer32 priority;
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
        EXPECT_EQ(block.instructions[3].detail, "field integer start [live]");
        EXPECT_EQ(block.instructions[4].detail, "field integer mode bits=8");
        EXPECT_EQ(block.instructions[5].detail, "field integer priority bits=4 align=16");

        const auto& terminator = block.instructions.back();
        EXPECT_EQ(terminator.kind, InstructionKind::Return);
        EXPECT_EQ(terminator.detail, "blueprint");
    }

    TEST(LoweringTest, EmitsLinkFunctionAndBlueprints)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint FirstBlueprint {
    integer firstField;
}

public blueprint SecondBlueprint {
    integer secondField;
}

public link integer function staticFunctionTest(integer value) {
    return value;
}
)";

        auto hirModule = buildHir(source);
        Module mirModule = lowerFromHir(hirModule);
        ASSERT_TRUE(verify(mirModule));

        ASSERT_EQ(mirModule.functions.size(), 3u);

        const auto& function = mirModule.functions[0];
        EXPECT_EQ(function.name, "staticFunctionTest");
        ASSERT_EQ(function.blocks.size(), 1u);
        const auto& functionBlock = function.blocks.front();
        ASSERT_EQ(functionBlock.instructions.size(), 4u);
        EXPECT_EQ(functionBlock.instructions[0].detail, "modifiers: public link");
        EXPECT_EQ(functionBlock.instructions[1].detail, "return integer");
        EXPECT_EQ(functionBlock.instructions[2].detail, "param integer value");
        EXPECT_EQ(functionBlock.instructions.back().detail, "function");
        EXPECT_EQ(functionBlock.instructions.back().kind, InstructionKind::Return);

        const auto& firstBlueprint = mirModule.functions[1];
        EXPECT_EQ(firstBlueprint.name, "blueprint.FirstBlueprint");
        ASSERT_EQ(firstBlueprint.blocks.size(), 1u);
        const auto& firstBlock = firstBlueprint.blocks.front();
        ASSERT_GE(firstBlock.instructions.size(), 3u);
        EXPECT_EQ(firstBlock.instructions[0].detail, "modifiers: public");
        EXPECT_EQ(firstBlock.instructions[1].detail, "field integer firstField");
        EXPECT_EQ(firstBlock.instructions.back().kind, InstructionKind::Return);

        const auto& secondBlueprint = mirModule.functions[2];
        EXPECT_EQ(secondBlueprint.name, "blueprint.SecondBlueprint");
        ASSERT_EQ(secondBlueprint.blocks.size(), 1u);
        const auto& secondBlock = secondBlueprint.blocks.front();
        ASSERT_GE(secondBlock.instructions.size(), 3u);
        EXPECT_EQ(secondBlock.instructions[0].detail, "modifiers: public");
        EXPECT_EQ(secondBlock.instructions[1].detail, "field integer secondField");
        EXPECT_EQ(secondBlock.instructions.back().kind, InstructionKind::Return);
    }

    TEST(LoweringTest, EmitsConstructorParameterDefaults)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint Sample {
    integer value;
}

public void function Sample(integer value) {}
)";

        auto hirModule = buildHir(source);
        Module mirModule = lowerFromHir(hirModule);
        ASSERT_TRUE(verify(mirModule));

        const auto constructorIt = std::find_if(
            mirModule.functions.begin(),
            mirModule.functions.end(),
            [](const Function& fn) { return fn.name == "Sample"; });
        ASSERT_NE(constructorIt, mirModule.functions.end());

        const auto& constructor = *constructorIt;
        EXPECT_TRUE(constructor.isBlueprintConstructor);
        ASSERT_TRUE(constructor.blueprintName.has_value());
        EXPECT_EQ(*constructor.blueprintName, "Sample");
        ASSERT_EQ(constructor.parameters.size(), 1u);
        EXPECT_TRUE(constructor.parameters.front().hasDefaultValue);
        EXPECT_EQ(constructor.parameters.front().defaultValue, "0");

        ASSERT_EQ(constructor.blocks.size(), 1u);
        const auto& block = constructor.blocks.front();
        const auto paramDetail = std::find_if(
            block.instructions.begin(),
            block.instructions.end(),
            [](const Instruction& inst) {
                return inst.detail.find("param integer value") != std::string::npos;
            });
        ASSERT_NE(paramDetail, block.instructions.end());
        EXPECT_NE(paramDetail->detail.find("default=0"), std::string::npos);
    }
}
} // namespace bolt::mir




