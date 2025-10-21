#include "control_flow_graph.hpp"

#include <algorithm>
#include <unordered_map>

namespace bolt::mir::passes
{
    namespace
    {
        void appendUnique(std::vector<const BasicBlock*>& list, const BasicBlock* block)
        {
            if (block == nullptr)
            {
                return;
            }

            if (std::find(list.begin(), list.end(), block) == list.end())
            {
                list.push_back(block);
            }
        }

        void sortById(std::vector<const BasicBlock*>& blocks)
        {
            std::sort(blocks.begin(), blocks.end(), [](const BasicBlock* left, const BasicBlock* right) {
                return left->id < right->id;
            });
        }
    } // namespace

    const ControlFlowGraphNode* ControlFlowGraph::findNode(std::uint32_t blockId) const
    {
        for (const auto& node : nodes)
        {
            if (node.block != nullptr && node.block->id == blockId)
            {
                return &node;
            }
        }
        return nullptr;
    }

    ControlFlowGraph buildControlFlowGraph(const Function& function)
    {
        ControlFlowGraph graph;
        graph.function = &function;
        graph.nodes.reserve(function.blocks.size());

        std::unordered_map<std::uint32_t, std::size_t> indexById;
        for (const auto& block : function.blocks)
        {
            ControlFlowGraphNode node;
            node.block = &block;
            graph.nodes.emplace_back(std::move(node));
            indexById.emplace(block.id, graph.nodes.size() - 1);
        }

        for (auto& node : graph.nodes)
        {
            if (node.block == nullptr || node.block->instructions.empty())
            {
                continue;
            }

            const Instruction& terminator = node.block->instructions.back();
            for (std::uint32_t successorId : terminator.successors)
            {
                auto successorIt = indexById.find(successorId);
                if (successorIt == indexById.end())
                {
                    continue;
                }

                const auto successorIndex = successorIt->second;
                appendUnique(node.successors, graph.nodes[successorIndex].block);
            }

            sortById(node.successors);
        }

        for (auto& node : graph.nodes)
        {
            for (const auto* successor : node.successors)
            {
                if (successor == nullptr)
                {
                    continue;
                }

                auto successorIt = indexById.find(successor->id);
                if (successorIt == indexById.end())
                {
                    continue;
                }

                auto& successorNode = graph.nodes[successorIt->second];
                appendUnique(successorNode.predecessors, node.block);
            }
        }

        for (auto& node : graph.nodes)
        {
            sortById(node.predecessors);
        }

        return graph;
    }
} // namespace bolt::mir::passes
