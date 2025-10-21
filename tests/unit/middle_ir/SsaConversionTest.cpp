#include <gtest/gtest.h>

#include <unordered_set>

#include "module.hpp"
#include "passes/ssa_conversion.hpp"

namespace bolt::mir::passes
{
namespace
{
    Function buildDiamondFunction()
    {
        Function function;
        function.name = "diamond";
        function.nextBlockId = 4;
        function.nextValueId = 1;

        BasicBlock entry;
        entry.id = 0;
        entry.name = "entry";

        Instruction entryAssign;
        entryAssign.kind = InstructionKind::Binary;
        entryAssign.result = Value{ValueKind::Temporary, 0, "x"};
        entryAssign.detail = "entry-assign";
        entry.instructions.push_back(entryAssign);

        Instruction cond;
        cond.kind = InstructionKind::CondBranch;
        cond.detail = "branch";
        cond.successors = {1, 2};
        Operand condOperand;
        condOperand.value = Value{ValueKind::Temporary, 0, "x"};
        cond.operands.push_back(condOperand);
        entry.instructions.push_back(cond);

        BasicBlock thenBlock;
        thenBlock.id = 1;
        thenBlock.name = "then";

        Instruction thenAssign;
        thenAssign.kind = InstructionKind::Binary;
        thenAssign.result = Value{ValueKind::Temporary, 0, "x"};
        Operand thenOperand;
        thenOperand.value = Value{ValueKind::Temporary, 0, "x"};
        thenAssign.operands.push_back(thenOperand);
        thenAssign.detail = "then-assign";
        thenBlock.instructions.push_back(thenAssign);

        Instruction thenBranch;
        thenBranch.kind = InstructionKind::Branch;
        thenBranch.successors = {3};
        thenBranch.detail = "jump merge";
        thenBlock.instructions.push_back(thenBranch);

        BasicBlock elseBlock;
        elseBlock.id = 2;
        elseBlock.name = "else";

        Instruction elseAssign;
        elseAssign.kind = InstructionKind::Binary;
        elseAssign.result = Value{ValueKind::Temporary, 0, "x"};
        Operand elseOperand;
        elseOperand.value = Value{ValueKind::Temporary, 0, "x"};
        elseAssign.operands.push_back(elseOperand);
        elseAssign.detail = "else-assign";
        elseBlock.instructions.push_back(elseAssign);

        Instruction elseBranch;
        elseBranch.kind = InstructionKind::Branch;
        elseBranch.successors = {3};
        elseBranch.detail = "jump merge";
        elseBlock.instructions.push_back(elseBranch);

        BasicBlock mergeBlock;
        mergeBlock.id = 3;
        mergeBlock.name = "merge";

        Instruction mergeReturn;
        mergeReturn.kind = InstructionKind::Return;
        mergeReturn.detail = "return";
        Operand mergeOperand;
        mergeOperand.value = Value{ValueKind::Temporary, 0, "x"};
        mergeReturn.operands.push_back(mergeOperand);
        mergeBlock.instructions.push_back(mergeReturn);

        function.blocks.push_back(entry);
        function.blocks.push_back(thenBlock);
        function.blocks.push_back(elseBlock);
        function.blocks.push_back(mergeBlock);

        return function;
    }
} // namespace

    TEST(SsaConversionTest, InsertsPhiAndRenamesDiamond)
    {
        auto function = buildDiamondFunction();
        std::vector<SsaDiagnostic> diagnostics;

        EXPECT_TRUE(convertToSsa(function, diagnostics));
        EXPECT_TRUE(diagnostics.empty());

        ASSERT_EQ(function.blocks.size(), 4u);
        const auto& mergeBlock = function.blocks[3];
        ASSERT_FALSE(mergeBlock.instructions.empty());

        const auto& phi = mergeBlock.instructions.front();
        ASSERT_EQ(phi.kind, InstructionKind::Phi);
        ASSERT_TRUE(phi.result.has_value());
        EXPECT_EQ(phi.operands.size(), 2u);

        std::unordered_set<std::uint32_t> predecessors;
        for (const auto& operand : phi.operands)
        {
            ASSERT_TRUE(operand.predecessorBlockId.has_value());
            predecessors.insert(*operand.predecessorBlockId);
        }
        EXPECT_EQ(predecessors.size(), 2u);
        EXPECT_TRUE(predecessors.count(1));
        EXPECT_TRUE(predecessors.count(2));

        const auto& returnInst = mergeBlock.instructions.back();
        ASSERT_EQ(returnInst.kind, InstructionKind::Return);
        ASSERT_FALSE(returnInst.operands.empty());
        ASSERT_TRUE(phi.result.has_value());
        EXPECT_EQ(returnInst.operands.front().value.id, phi.result->id);
        EXPECT_EQ(returnInst.operands.front().value.name, phi.result->name);

        const auto& entryAssign = function.blocks[0].instructions.front();
        ASSERT_TRUE(entryAssign.result.has_value());
        const auto& thenAssign = function.blocks[1].instructions.front();
        ASSERT_TRUE(thenAssign.result.has_value());
        const auto& elseAssign = function.blocks[2].instructions.front();
        ASSERT_TRUE(elseAssign.result.has_value());

        EXPECT_NE(entryAssign.result->id, thenAssign.result->id);
        EXPECT_NE(entryAssign.result->id, elseAssign.result->id);
        EXPECT_NE(thenAssign.result->id, elseAssign.result->id);
    }

    TEST(SsaConversionTest, ReportsUseBeforeDefinition)
    {
        Function function;
        function.name = "use-before-def";

        BasicBlock block;
        block.id = 0;
        block.name = "only";

        Instruction inst;
        inst.kind = InstructionKind::Unary;
        inst.detail = "use";
        Operand operand;
        operand.value = Value{ValueKind::Temporary, 42, "temp"};
        inst.operands.push_back(operand);
        block.instructions.push_back(inst);

        function.blocks.push_back(block);

        std::vector<SsaDiagnostic> diagnostics;
        EXPECT_FALSE(convertToSsa(function, diagnostics));
        ASSERT_FALSE(diagnostics.empty());
        EXPECT_EQ(diagnostics.front().code, "BOLT-E4301");
    }
} // namespace bolt::mir::passes

