#include "ssa_placement.hpp"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace bolt::mir::passes
{
    namespace
    {
        void appendUniqueBlock(std::vector<const BasicBlock*>& blocks, const BasicBlock* block)
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

    std::vector<const BasicBlock*> computePhiPlacement(const DominanceFrontier& frontiers,
                                                        const std::vector<std::uint32_t>& definitionBlocks)
    {
        std::vector<const BasicBlock*> phiBlocks;

        if (frontiers.function == nullptr || frontiers.function->blocks.empty() || definitionBlocks.empty())
        {
            return phiBlocks;
        }

        std::unordered_set<std::uint32_t> definitionSet;
        std::unordered_set<std::uint32_t> queuedDefinitions;
        std::queue<std::uint32_t> worklist;

        for (std::uint32_t blockId : definitionBlocks)
        {
            if (definitionSet.insert(blockId).second)
            {
                queuedDefinitions.insert(blockId);
                worklist.push(blockId);
            }
        }

        std::unordered_set<std::uint32_t> phiSet;

        while (!worklist.empty())
        {
            const auto blockId = worklist.front();
            worklist.pop();

            const auto* frontierNode = frontiers.findNode(blockId);
            if (frontierNode == nullptr)
            {
                continue;
            }

            for (const auto* frontierBlock : frontierNode->frontier)
            {
                if (frontierBlock == nullptr)
                {
                    continue;
                }

                if (phiSet.insert(frontierBlock->id).second)
                {
                    appendUniqueBlock(phiBlocks, frontierBlock);

                    if (definitionSet.find(frontierBlock->id) == definitionSet.end()
                        && queuedDefinitions.insert(frontierBlock->id).second)
                    {
                        worklist.push(frontierBlock->id);
                    }
                }
            }
        }

        sortById(phiBlocks);
        return phiBlocks;
    }
} // namespace bolt::mir::passes
