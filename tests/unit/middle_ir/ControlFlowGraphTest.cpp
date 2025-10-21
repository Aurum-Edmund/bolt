#include <gtest/gtest.h>

#include "builder.hpp"
#include "passes/control_flow_graph.hpp"

namespace bolt::mir::passes
{
    TEST(ControlFlowGraphTest, LinearSuccessors)
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

        builder.appendInstruction(exit, InstructionKind::Return).detail = "return";

        const auto graph = buildControlFlowGraph(function);
        ASSERT_EQ(graph.nodes.size(), 3u);

        const auto* entryNode = graph.findNode(entry.id);
        ASSERT_NE(entryNode, nullptr);
        ASSERT_EQ(entryNode->successors.size(), 1u);
        EXPECT_EQ(entryNode->successors.front()->id, body.id);
        EXPECT_TRUE(entryNode->predecessors.empty());

        const auto* bodyNode = graph.findNode(body.id);
        ASSERT_NE(bodyNode, nullptr);
        ASSERT_EQ(bodyNode->successors.size(), 1u);
        EXPECT_EQ(bodyNode->successors.front()->id, exit.id);
        ASSERT_EQ(bodyNode->predecessors.size(), 1u);
        EXPECT_EQ(bodyNode->predecessors.front()->id, entry.id);

        const auto* exitNode = graph.findNode(exit.id);
        ASSERT_NE(exitNode, nullptr);
        EXPECT_TRUE(exitNode->successors.empty());
        ASSERT_EQ(exitNode->predecessors.size(), 1u);
        EXPECT_EQ(exitNode->predecessors.front()->id, body.id);
    }

    TEST(ControlFlowGraphTest, ConditionalSuccessorsAreSorted)
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

        auto& condBranch = builder.appendInstruction(entry, InstructionKind::CondBranch);
        condBranch.successors.push_back(merge.id);
        condBranch.successors.push_back(trueBlock.id);
        condBranch.successors.push_back(falseBlock.id);

        auto& trueBranch = builder.appendInstruction(trueBlock, InstructionKind::Branch);
        trueBranch.successors.push_back(merge.id);

        auto& falseBranch = builder.appendInstruction(falseBlock, InstructionKind::Branch);
        falseBranch.successors.push_back(merge.id);

        builder.appendInstruction(merge, InstructionKind::Return).detail = "return";

        const auto graph = buildControlFlowGraph(function);
        ASSERT_EQ(graph.nodes.size(), 4u);

        const auto* entryNode = graph.findNode(entry.id);
        ASSERT_NE(entryNode, nullptr);
        ASSERT_EQ(entryNode->successors.size(), 3u);
        EXPECT_EQ(entryNode->successors[0]->id, trueBlock.id);
        EXPECT_EQ(entryNode->successors[1]->id, falseBlock.id);
        EXPECT_EQ(entryNode->successors[2]->id, merge.id);

        const auto* mergeNode = graph.findNode(merge.id);
        ASSERT_NE(mergeNode, nullptr);
        ASSERT_EQ(mergeNode->predecessors.size(), 3u);
        EXPECT_EQ(mergeNode->predecessors[0]->id, entry.id);
        EXPECT_EQ(mergeNode->predecessors[1]->id, trueBlock.id);
        EXPECT_EQ(mergeNode->predecessors[2]->id, falseBlock.id);
    }
} // namespace bolt::mir::passes
