#include "dominance_frontier.hpp"

#include "control_flow_graph.hpp"

#include <algorithm>
#include <unordered_map>

namespace bolt::mir::passes
{
    namespace
    {
        void appendUnique(std::vector<const BasicBlock*>& blocks, const BasicBlock* block)
        {
            if (block == nullptr)
            {
                return;
            }

            if (std::find(blocks.begin(), blocks.end(), block) == blocks.end())
            {
                blocks.push_back(block);
            }
        }

        void sortById(std::vector<const BasicBlock*>& blocks)
        {
            std::sort(blocks.begin(), blocks.end(), [](const BasicBlock* left, const BasicBlock* right) {
                return left->id < right->id;
            });
        }
    } // namespace

    const DominanceFrontierNode* DominanceFrontier::findNode(std::uint32_t blockId) const
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

    DominanceFrontier buildDominanceFrontier(const Function& function, const DominatorTree& tree)
    {
        DominanceFrontier frontiers;
        frontiers.function = &function;
        frontiers.nodes.reserve(function.blocks.size());

        std::unordered_map<std::uint32_t, std::size_t> indexById;
        indexById.reserve(function.blocks.size());

        for (const auto& block : function.blocks)
        {
            DominanceFrontierNode node;
            node.block = &block;
            frontiers.nodes.emplace_back(std::move(node));
            indexById.emplace(block.id, frontiers.nodes.size() - 1);
        }

        if (function.blocks.empty())
        {
            return frontiers;
        }

        const auto cfg = buildControlFlowGraph(function);

        for (const auto& block : function.blocks)
        {
            const auto* cfgNode = cfg.findNode(block.id);
            if (cfgNode == nullptr || cfgNode->predecessors.size() < 2)
            {
                continue;
            }

            const auto* dominatorNode = tree.findNode(block.id);
            const BasicBlock* immediateDominator = nullptr;
            if (dominatorNode != nullptr)
            {
                immediateDominator = dominatorNode->immediateDominator;
            }

            for (const auto* predecessor : cfgNode->predecessors)
            {
                const auto* runner = predecessor;
                while (runner != nullptr && runner != immediateDominator)
                {
                    auto indexIt = indexById.find(runner->id);
                    if (indexIt == indexById.end())
                    {
                        break;
                    }

                    auto& frontier = frontiers.nodes[indexIt->second].frontier;
                    appendUnique(frontier, &block);

                    const auto* runnerNode = tree.findNode(runner->id);
                    if (runnerNode == nullptr)
                    {
                        break;
                    }

                    runner = runnerNode->immediateDominator;
                }
            }
        }

        for (auto& node : frontiers.nodes)
        {
            sortById(node.frontier);
        }

        return frontiers;
    }
} // namespace bolt::mir::passes
