#include <gtest/gtest.h>

#include "builder.hpp"
#include "passes/dominator_tree.hpp"

namespace bolt::mir::passes
{
    TEST(DominatorTreeTest, LinearChain)
    {
        mir::Module module;
        mir::Builder builder{module};
        auto& function = builder.createFunction("linear");
        builder.appendBlock(function, "entry");
        builder.appendBlock(function, "body");
        builder.appendBlock(function, "exit");

        auto& entry = function.blocks[0];
        auto& body = function.blocks[1];
        auto& exit = function.blocks[2];

        auto& entryBranch = builder.appendInstruction(entry, InstructionKind::Branch);
        entryBranch.successors.push_back(body.id);

        auto& bodyBranch = builder.appendInstruction(body, InstructionKind::Branch);
        bodyBranch.successors.push_back(exit.id);

        builder.appendInstruction(exit, InstructionKind::Return);

        const auto tree = buildDominatorTree(function);
        ASSERT_EQ(tree.nodes.size(), 3u);

        const auto* entryNode = tree.findNode(entry.id);
        ASSERT_NE(entryNode, nullptr);
        EXPECT_EQ(entryNode->immediateDominator, nullptr);
        EXPECT_TRUE(tree.dominates(entry.id, entry.id));
        EXPECT_TRUE(tree.dominates(entry.id, body.id));
        EXPECT_TRUE(tree.dominates(entry.id, exit.id));

        const auto* bodyNode = tree.findNode(body.id);
        ASSERT_NE(bodyNode, nullptr);
        ASSERT_NE(bodyNode->immediateDominator, nullptr);
        EXPECT_EQ(bodyNode->immediateDominator->id, entry.id);
        EXPECT_TRUE(tree.dominates(body.id, body.id));
        EXPECT_TRUE(tree.dominates(body.id, exit.id));
        EXPECT_FALSE(tree.dominates(body.id, entry.id));

        const auto* exitNode = tree.findNode(exit.id);
        ASSERT_NE(exitNode, nullptr);
        ASSERT_NE(exitNode->immediateDominator, nullptr);
        EXPECT_EQ(exitNode->immediateDominator->id, body.id);
    }

    TEST(DominatorTreeTest, DiamondMergeDominatedByEntry)
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
        ASSERT_EQ(tree.nodes.size(), 4u);

        const auto* trueNode = tree.findNode(trueBlock.id);
        ASSERT_NE(trueNode, nullptr);
        ASSERT_NE(trueNode->immediateDominator, nullptr);
        EXPECT_EQ(trueNode->immediateDominator->id, entry.id);

        const auto* falseNode = tree.findNode(falseBlock.id);
        ASSERT_NE(falseNode, nullptr);
        ASSERT_NE(falseNode->immediateDominator, nullptr);
        EXPECT_EQ(falseNode->immediateDominator->id, entry.id);

        const auto* mergeNode = tree.findNode(merge.id);
        ASSERT_NE(mergeNode, nullptr);
        ASSERT_NE(mergeNode->immediateDominator, nullptr);
        EXPECT_EQ(mergeNode->immediateDominator->id, entry.id);
        EXPECT_TRUE(tree.dominates(entry.id, merge.id));
        EXPECT_FALSE(tree.dominates(trueBlock.id, entry.id));
    }

    TEST(DominatorTreeTest, UnreachableBlockOnlyDominatedByItself)
    {
        mir::Module module;
        mir::Builder builder{module};
        auto& function = builder.createFunction("unreachable");
        builder.appendBlock(function, "entry");
        builder.appendBlock(function, "reachable");
        builder.appendBlock(function, "orphan");

        auto& entry = function.blocks[0];
        auto& reachable = function.blocks[1];
        auto& orphan = function.blocks[2];

        auto& branch = builder.appendInstruction(entry, InstructionKind::Branch);
        branch.successors.push_back(reachable.id);
        builder.appendInstruction(reachable, InstructionKind::Return);

        const auto tree = buildDominatorTree(function);
        ASSERT_EQ(tree.nodes.size(), 3u);

        const auto* orphanNode = tree.findNode(orphan.id);
        ASSERT_NE(orphanNode, nullptr);
        EXPECT_EQ(orphanNode->immediateDominator, nullptr);
        EXPECT_TRUE(tree.dominates(orphan.id, orphan.id));
        EXPECT_FALSE(tree.dominates(entry.id, orphan.id));
    }
} // namespace bolt::mir::passes
