#include <gtest/gtest.h>

#include <vector>

#include "builder.hpp"
#include "passes/dominance_frontier.hpp"
#include "passes/dominator_tree.hpp"
#include "passes/ssa_placement.hpp"

namespace bolt::mir::passes
{
    namespace
    {
        DominanceFrontier buildFrontier(mir::Function& function)
        {
            const auto tree = buildDominatorTree(function);
            return buildDominanceFrontier(function, tree);
        }

        std::vector<const mir::BasicBlock*> placeFor(mir::Function& function,
                                                     const std::vector<std::uint32_t>& definitionBlocks)
        {
            const auto frontier = buildFrontier(function);
            return computePhiPlacement(frontier, definitionBlocks);
        }
    } // namespace

    TEST(SsaPlacementTest, InsertsPhiAtMergeForDiamond)
    {
        mir::Module module;
        mir::Builder builder{module};
        auto& function = builder.createFunction("diamond");
        builder.appendBlock(function, "entry");
        builder.appendBlock(function, "true");
        builder.appendBlock(function, "false");
        builder.appendBlock(function, "merge");

        auto& entry = function.blocks[0];
        auto& trueBlock = function.blocks[1];
        auto& falseBlock = function.blocks[2];
        auto& merge = function.blocks[3];

        auto& cond = builder.appendInstruction(entry, InstructionKind::CondBranch);
        cond.successors.push_back(trueBlock.id);
        cond.successors.push_back(falseBlock.id);
        cond.successors.push_back(merge.id);

        auto& trueBranch = builder.appendInstruction(trueBlock, InstructionKind::Branch);
        trueBranch.successors.push_back(merge.id);

        auto& falseBranch = builder.appendInstruction(falseBlock, InstructionKind::Branch);
        falseBranch.successors.push_back(merge.id);

        builder.appendInstruction(merge, InstructionKind::Return).detail = "return";

        const auto phiBlocks = placeFor(function, {entry.id, trueBlock.id});
        ASSERT_EQ(phiBlocks.size(), 1u);
        EXPECT_EQ(phiBlocks.front()->id, merge.id);
    }

    TEST(SsaPlacementTest, HandlesLoopHeaderPlacement)
    {
        mir::Module module;
        mir::Builder builder{module};
        auto& function = builder.createFunction("loop");
        builder.appendBlock(function, "entry");
        builder.appendBlock(function, "header");
        builder.appendBlock(function, "body");
        builder.appendBlock(function, "exit");

        auto& entry = function.blocks[0];
        auto& header = function.blocks[1];
        auto& body = function.blocks[2];
        auto& exit = function.blocks[3];

        auto& entryBranch = builder.appendInstruction(entry, InstructionKind::Branch);
        entryBranch.successors.push_back(header.id);

        auto& headerCond = builder.appendInstruction(header, InstructionKind::CondBranch);
        headerCond.successors.push_back(exit.id);
        headerCond.successors.push_back(body.id);

        auto& bodyBranch = builder.appendInstruction(body, InstructionKind::Branch);
        bodyBranch.successors.push_back(header.id);

        builder.appendInstruction(exit, InstructionKind::Return).detail = "return";

        const auto phiBlocks = placeFor(function, {entry.id, body.id});
        ASSERT_EQ(phiBlocks.size(), 1u);
        EXPECT_EQ(phiBlocks.front()->id, header.id);
    }

    TEST(SsaPlacementTest, AvoidsDuplicatePhiInsertion)
    {
        mir::Module module;
        mir::Builder builder{module};
        auto& function = builder.createFunction("fanout");
        builder.appendBlock(function, "entry");
        builder.appendBlock(function, "left");
        builder.appendBlock(function, "right");
        builder.appendBlock(function, "merge");

        auto& entry = function.blocks[0];
        auto& left = function.blocks[1];
        auto& right = function.blocks[2];
        auto& merge = function.blocks[3];

        auto& entryBranch = builder.appendInstruction(entry, InstructionKind::CondBranch);
        entryBranch.successors.push_back(left.id);
        entryBranch.successors.push_back(right.id);
        entryBranch.successors.push_back(merge.id);

        auto& leftBranch = builder.appendInstruction(left, InstructionKind::Branch);
        leftBranch.successors.push_back(merge.id);

        auto& rightBranch = builder.appendInstruction(right, InstructionKind::Branch);
        rightBranch.successors.push_back(merge.id);

        builder.appendInstruction(merge, InstructionKind::Return).detail = "return";

        const auto phiBlocks = placeFor(function, {entry.id, left.id, right.id});
        ASSERT_EQ(phiBlocks.size(), 1u);
        EXPECT_EQ(phiBlocks.front()->id, merge.id);
    }
}
