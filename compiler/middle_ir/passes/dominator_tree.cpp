#include "dominator_tree.hpp"

#include "control_flow_graph.hpp"

#include <algorithm>
#include <unordered_map>

namespace bolt::mir::passes
{
    namespace
    {
        using DominatorMatrix = std::vector<std::vector<bool>>;

        std::unordered_map<std::uint32_t, std::size_t> buildIndexById(const Function& function)
        {
            std::unordered_map<std::uint32_t, std::size_t> indexById;
            indexById.reserve(function.blocks.size());
            for (std::size_t index = 0; index < function.blocks.size(); ++index)
            {
                indexById.emplace(function.blocks[index].id, index);
            }
            return indexById;
        }

        std::vector<std::size_t> gatherPredecessorIndices(
            const ControlFlowGraph& graph,
            const std::unordered_map<std::uint32_t, std::size_t>& indexById,
            std::uint32_t blockId)
        {
            std::vector<std::size_t> indices;
            if (const auto* node = graph.findNode(blockId))
            {
                indices.reserve(node->predecessors.size());
                for (const auto* predecessor : node->predecessors)
                {
                    if (predecessor == nullptr)
                    {
                        continue;
                    }

                    auto predecessorIt = indexById.find(predecessor->id);
                    if (predecessorIt != indexById.end())
                    {
                        indices.push_back(predecessorIt->second);
                    }
                }
            }
            return indices;
        }

        void initialiseDominatorMatrix(const Function& function,
            const ControlFlowGraph& graph,
            DominatorMatrix& matrix,
            const std::unordered_map<std::uint32_t, std::size_t>& indexById)
        {
            if (function.blocks.empty())
            {
                return;
            }

            const auto entryId = function.blocks.front().id;
            for (std::size_t index = 0; index < function.blocks.size(); ++index)
            {
                const auto& block = function.blocks[index];
                auto& dominators = matrix[index];

                if (block.id == entryId)
                {
                    dominators[index] = true;
                    continue;
                }

                const auto predecessors = gatherPredecessorIndices(graph, indexById, block.id);
                if (predecessors.empty())
                {
                    dominators[index] = true;
                    continue;
                }

                std::fill(dominators.begin(), dominators.end(), true);
            }
        }

        bool updateBlockDominators(std::size_t blockIndex,
            const std::vector<std::size_t>& predecessors,
            DominatorMatrix& matrix)
        {
            if (predecessors.empty())
            {
                std::vector<bool> newDominators(matrix.size(), false);
                newDominators[blockIndex] = true;
                if (newDominators != matrix[blockIndex])
                {
                    matrix[blockIndex] = std::move(newDominators);
                    return true;
                }
                return false;
            }

            std::vector<bool> newDominators(matrix.size(), true);
            for (auto predecessorIndex : predecessors)
            {
                for (std::size_t candidate = 0; candidate < newDominators.size(); ++candidate)
                {
                    newDominators[candidate]
                        = newDominators[candidate] && matrix[predecessorIndex][candidate];
                }
            }

            newDominators[blockIndex] = true;
            if (newDominators != matrix[blockIndex])
            {
                matrix[blockIndex] = std::move(newDominators);
                return true;
            }
            return false;
        }

        DominatorMatrix computeDominatorMatrix(const Function& function,
            const ControlFlowGraph& graph,
            const std::unordered_map<std::uint32_t, std::size_t>& indexById)
        {
            DominatorMatrix matrix(function.blocks.size(), std::vector<bool>(function.blocks.size(), false));
            initialiseDominatorMatrix(function, graph, matrix, indexById);

            if (function.blocks.empty())
            {
                return matrix;
            }

            const auto entryId = function.blocks.front().id;
            bool changed = true;
            while (changed)
            {
                changed = false;
                for (std::size_t index = 0; index < function.blocks.size(); ++index)
                {
                    const auto& block = function.blocks[index];
                    if (block.id == entryId)
                    {
                        continue;
                    }

                    const auto predecessors = gatherPredecessorIndices(graph, indexById, block.id);
                    if (updateBlockDominators(index, predecessors, matrix))
                    {
                        changed = true;
                    }
                }
            }

            return matrix;
        }

        bool dominates(const DominatorMatrix& matrix, std::size_t dominatorIndex, std::size_t blockIndex)
        {
            if (blockIndex >= matrix.size() || dominatorIndex >= matrix.size())
            {
                return false;
            }
            return matrix[blockIndex][dominatorIndex];
        }

        void populateDominatorSets(const Function& function,
            const DominatorMatrix& matrix,
            DominatorTree& tree)
        {
            for (std::size_t blockIndex = 0; blockIndex < function.blocks.size(); ++blockIndex)
            {
                auto& node = tree.nodes[blockIndex];
                node.dominators.clear();

                for (std::size_t candidate = 0; candidate < function.blocks.size(); ++candidate)
                {
                    if (matrix[blockIndex][candidate])
                    {
                        node.dominators.push_back(&function.blocks[candidate]);
                    }
                }

                std::sort(node.dominators.begin(), node.dominators.end(), [](const BasicBlock* left, const BasicBlock* right) {
                    return left->id < right->id;
                });
            }
        }

        const BasicBlock* computeImmediateDominator(const Function& function,
            const DominatorMatrix& matrix,
            std::size_t blockIndex)
        {
            const auto& block = function.blocks[blockIndex];
            if (&block == &function.blocks.front())
            {
                return nullptr;
            }

            const auto& dominatorRow = matrix[blockIndex];
            const BasicBlock* immediate{nullptr};

            for (std::size_t candidateIndex = 0; candidateIndex < dominatorRow.size(); ++candidateIndex)
            {
                if (!dominatorRow[candidateIndex])
                {
                    continue;
                }

                if (candidateIndex == blockIndex)
                {
                    continue;
                }

                const auto* candidate = &function.blocks[candidateIndex];
                bool dominatedByAllOthers = true;
                for (std::size_t otherIndex = 0; otherIndex < dominatorRow.size(); ++otherIndex)
                {
                    if (otherIndex == candidateIndex || otherIndex == blockIndex)
                    {
                        continue;
                    }

                    if (!dominatorRow[otherIndex])
                    {
                        continue;
                    }

                    if (!dominates(matrix, otherIndex, candidateIndex))
                    {
                        dominatedByAllOthers = false;
                        break;
                    }
                }

                if (dominatedByAllOthers)
                {
                    immediate = candidate;
                    break;
                }
            }

            return immediate;
        }

        void populateImmediateDominators(const Function& function,
            const DominatorMatrix& matrix,
            DominatorTree& tree,
            const std::unordered_map<std::uint32_t, std::size_t>& indexById)
        {
            for (std::size_t blockIndex = 0; blockIndex < function.blocks.size(); ++blockIndex)
            {
                auto& node = tree.nodes[blockIndex];
                node.immediateDominator = computeImmediateDominator(function, matrix, blockIndex);
                if (node.immediateDominator != nullptr)
                {
                    auto parentIt = indexById.find(node.immediateDominator->id);
                    if (parentIt != indexById.end())
                    {
                        tree.nodes[parentIt->second].children.push_back(node.block);
                    }
                }
            }

            for (auto& node : tree.nodes)
            {
                std::sort(node.children.begin(), node.children.end(), [](const BasicBlock* left, const BasicBlock* right) {
                    return left->id < right->id;
                });
            }
        }
    } // namespace

    const DominatorTreeNode* DominatorTree::findNode(std::uint32_t blockId) const
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

    bool DominatorTree::dominates(std::uint32_t dominatorBlockId, std::uint32_t blockId) const
    {
        const auto* node = findNode(blockId);
        if (node == nullptr)
        {
            return false;
        }

        for (const auto* dominator : node->dominators)
        {
            if (dominator != nullptr && dominator->id == dominatorBlockId)
            {
                return true;
            }
        }
        return false;
    }

    DominatorTree buildDominatorTree(const Function& function)
    {
        DominatorTree tree;
        tree.function = &function;
        tree.nodes.reserve(function.blocks.size());
        for (const auto& block : function.blocks)
        {
            DominatorTreeNode node;
            node.block = &block;
            tree.nodes.emplace_back(std::move(node));
        }

        if (function.blocks.empty())
        {
            return tree;
        }

        auto indexById = buildIndexById(function);
        const auto cfg = buildControlFlowGraph(function);
        const auto matrix = computeDominatorMatrix(function, cfg, indexById);

        populateDominatorSets(function, matrix, tree);
        populateImmediateDominators(function, matrix, tree, indexById);

        return tree;
    }
} // namespace bolt::mir::passes
