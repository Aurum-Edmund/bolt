#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "lexer.hpp"
#include "parser.hpp"
#include "binder.hpp"
#include "lowering.hpp"
#include "passes/live_enforcement.hpp"

namespace
{
    bolt::hir::Module buildHir(const std::string& source)
    {
        bolt::frontend::Lexer lexer{source, "live-enforcement-test"};
        lexer.lex();

        bolt::frontend::Parser parser{lexer.tokens(), "live-enforcement-test"};
        auto unit = parser.parse();
        EXPECT_TRUE(parser.diagnostics().empty());

        bolt::hir::Binder binder{unit, "live-enforcement-test"};
        auto module = binder.bind();
        EXPECT_TRUE(binder.diagnostics().empty());
        return module;
    }
}

namespace bolt::mir
{
namespace
{
    TypeReference makeBuiltinType(const std::string& name)
    {
        TypeReference type{};
        type.kind = bolt::common::TypeKind::Named;
        type.name.components.emplace_back(name);
        type.isBuiltin = true;
        type.text = name;
        type.originalText = name;
        type.normalizedText = name;
        return type;
    }

    TEST(LiveEnforcementTest, AcceptsSimpleModule)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public integer function demo() {
    return 0;
}
)";

        auto hirModule = buildHir(source);
        Module mirModule = lowerFromHir(hirModule);
        std::vector<LiveDiagnostic> diagnostics;
        EXPECT_TRUE(enforceLive(mirModule, diagnostics));
        EXPECT_TRUE(diagnostics.empty());
    }

    TEST(LiveEnforcementTest, RejectsLiveFunctionWithoutReturn)
    {
        Module module;
        Function function;
        function.name = "demo";
        Function::Parameter parameter{};
        parameter.type = makeBuiltinType("integer");
        parameter.name = "value";
        parameter.isLive = true;
        function.parameters.push_back(parameter);
        function.blocks.emplace_back();
        module.functions.push_back(function);

        std::vector<LiveDiagnostic> diagnostics;
        EXPECT_FALSE(enforceLive(module, diagnostics));
        ASSERT_EQ(diagnostics.size(), 2u);
        EXPECT_EQ(diagnostics[0].code, "BOLT-E4103");
        EXPECT_EQ(diagnostics[0].functionName, "demo");
        EXPECT_NE(diagnostics[0].detail.find("missing a return instruction"), std::string::npos);
        EXPECT_EQ(diagnostics[1].code, "BOLT-E4104");
        EXPECT_EQ(diagnostics[1].functionName, "demo");
        EXPECT_NE(diagnostics[1].detail.find("empty basic block"), std::string::npos);
    }

    TEST(LiveEnforcementTest, RejectsLiveReturnWithoutType)
    {
        Module module;
        Function function;
        function.name = "requiresType";
        function.returnIsLive = true;
        function.blocks.emplace_back();
        function.blocks.back().instructions.push_back({InstructionKind::Return, {}, {}});
        module.functions.push_back(function);

        std::vector<LiveDiagnostic> diagnostics;
        EXPECT_FALSE(enforceLive(module, diagnostics));
        ASSERT_EQ(diagnostics.size(), 1u);
        EXPECT_EQ(diagnostics.front().code, "BOLT-E4101");
        EXPECT_EQ(diagnostics.front().functionName, "requiresType");
        EXPECT_NE(diagnostics.front().detail.find("return declared without a concrete return type"), std::string::npos);
    }

    TEST(LiveEnforcementTest, RejectsLiveFunctionWithoutBlocks)
    {
        Module module;
        Function function;
        function.name = "noBlocks";
        function.returnIsLive = true;
        function.hasReturnType = true;
        function.returnType = makeBuiltinType("integer");
        module.functions.push_back(function);

        std::vector<LiveDiagnostic> diagnostics;
        EXPECT_FALSE(enforceLive(module, diagnostics));
        ASSERT_EQ(diagnostics.size(), 1u);
        EXPECT_EQ(diagnostics.front().code, "BOLT-E4102");
        EXPECT_EQ(diagnostics.front().functionName, "noBlocks");
        EXPECT_NE(diagnostics.front().detail.find("no basic blocks"), std::string::npos);
    }

    TEST(LiveEnforcementTest, RejectsLiveBlockMissingTerminator)
    {
        Module module;
        Function function;
        function.name = "misordered";
        Function::Parameter parameter{};
        parameter.type = makeBuiltinType("integer");
        parameter.name = "value";
        parameter.isLive = true;
        function.parameters.push_back(parameter);

        BasicBlock block;
        block.name = "entry";
        Instruction returnInst;
        returnInst.kind = InstructionKind::Return;
        block.instructions.push_back(returnInst);
        Instruction trailing;
        trailing.kind = InstructionKind::Nop;
        block.instructions.push_back(trailing);
        function.blocks.push_back(block);
        module.functions.push_back(function);

        std::vector<LiveDiagnostic> diagnostics;
        EXPECT_FALSE(enforceLive(module, diagnostics));
        ASSERT_EQ(diagnostics.size(), 1u);
        EXPECT_EQ(diagnostics.front().code, "BOLT-E4105");
        EXPECT_EQ(diagnostics.front().functionName, "misordered");
        EXPECT_NE(diagnostics.front().detail.find("must terminate with return or branch"), std::string::npos);
    }

    TEST(LiveEnforcementTest, RejectsUnreachableBlocksInLiveFunction)
    {
        Module module;
        Function function;
        function.name = "unreachable";
        function.returnIsLive = true;
        function.hasReturnType = true;
        function.returnType = makeBuiltinType("integer");

        BasicBlock entry;
        entry.id = 0;
        entry.name = "entry";
        Instruction branch;
        branch.kind = InstructionKind::Branch;
        branch.successors.push_back(1);
        entry.instructions.push_back(branch);

        BasicBlock reachable;
        reachable.id = 1;
        reachable.name = "reachable";
        Instruction ret;
        ret.kind = InstructionKind::Return;
        reachable.instructions.push_back(ret);

        BasicBlock unreachable;
        unreachable.id = 2;
        unreachable.name = "dead";
        Instruction deadReturn;
        deadReturn.kind = InstructionKind::Return;
        unreachable.instructions.push_back(deadReturn);

        function.blocks.push_back(entry);
        function.blocks.push_back(reachable);
        function.blocks.push_back(unreachable);
        module.functions.push_back(function);

        std::vector<LiveDiagnostic> diagnostics;
        EXPECT_FALSE(enforceLive(module, diagnostics));
        ASSERT_FALSE(diagnostics.empty());
        const auto unreachableIt = std::find_if(diagnostics.begin(), diagnostics.end(), [](const LiveDiagnostic& diagnostic) {
            return diagnostic.code == "BOLT-E4106";
        });
        ASSERT_NE(unreachableIt, diagnostics.end());
        EXPECT_EQ(unreachableIt->functionName, "unreachable");
        EXPECT_NE(unreachableIt->detail.find("dead"), std::string::npos);
    }
}
} // namespace bolt::mir

