#include <gtest/gtest.h>

#include "builder.hpp"
#include "passes/dominance_frontier.hpp"

namespace bolt::mir::passes
{
    TEST(DominanceFrontierTest, DiamondFrontierAssignedToBranchEdges)
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
        cond.successors.push_back(merge.id);
        cond.successors.push_back(trueBlock.id);
        cond.successors.push_back(falseBlock.id);

        auto& trueBranch = builder.appendInstruction(trueBlock, InstructionKind::Branch);
        trueBranch.successors.push_back(merge.id);

        auto& falseBranch = builder.appendInstruction(falseBlock, InstructionKind::Branch);
        falseBranch.successors.push_back(merge.id);

        builder.appendInstruction(merge, InstructionKind::Return);

        const auto tree = buildDominatorTree(function);
        const auto frontier = buildDominanceFrontier(function, tree);

        const auto* entryNode = frontier.findNode(entry.id);
        ASSERT_NE(entryNode, nullptr);
        EXPECT_TRUE(entryNode->frontier.empty());

        const auto* trueNode = frontier.findNode(trueBlock.id);
        ASSERT_NE(trueNode, nullptr);
        ASSERT_EQ(trueNode->frontier.size(), 1u);
        EXPECT_EQ(trueNode->frontier.front()->id, merge.id);

        const auto* falseNode = frontier.findNode(falseBlock.id);
        ASSERT_NE(falseNode, nullptr);
        ASSERT_EQ(falseNode->frontier.size(), 1u);
        EXPECT_EQ(falseNode->frontier.front()->id, merge.id);

        const auto* mergeNode = frontier.findNode(merge.id);
        ASSERT_NE(mergeNode, nullptr);
        EXPECT_TRUE(mergeNode->frontier.empty());
    }

    TEST(DominanceFrontierTest, LoopBackEdgeProducesSelfFrontier)
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

        auto& headerBranch = builder.appendInstruction(header, InstructionKind::CondBranch);
        headerBranch.successors.push_back(exit.id);
        headerBranch.successors.push_back(body.id);

        auto& bodyBranch = builder.appendInstruction(body, InstructionKind::Branch);
        bodyBranch.successors.push_back(header.id);

        builder.appendInstruction(exit, InstructionKind::Return);

        const auto tree = buildDominatorTree(function);
        const auto frontier = buildDominanceFrontier(function, tree);

        const auto* headerNode = frontier.findNode(header.id);
        ASSERT_NE(headerNode, nullptr);
        ASSERT_EQ(headerNode->frontier.size(), 1u);
        EXPECT_EQ(headerNode->frontier.front()->id, header.id);

        const auto* bodyNode = frontier.findNode(body.id);
        ASSERT_NE(bodyNode, nullptr);
        ASSERT_EQ(bodyNode->frontier.size(), 1u);
        EXPECT_EQ(bodyNode->frontier.front()->id, header.id);

        const auto* exitNode = frontier.findNode(exit.id);
        ASSERT_NE(exitNode, nullptr);
        EXPECT_TRUE(exitNode->frontier.empty());
    }
} // namespace bolt::mir::passes
