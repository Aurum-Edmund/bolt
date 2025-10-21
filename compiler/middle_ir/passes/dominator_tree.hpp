#pragma once

#include "../module.hpp"

#include <vector>

namespace bolt::mir::passes
{
    struct DominatorTreeNode
    {
        const BasicBlock* block{nullptr};
        const BasicBlock* immediateDominator{nullptr};
        std::vector<const BasicBlock*> dominators;
        std::vector<const BasicBlock*> children;
    };

    struct DominatorTree
    {
        const Function* function{nullptr};
        std::vector<DominatorTreeNode> nodes;

        [[nodiscard]] const DominatorTreeNode* findNode(std::uint32_t blockId) const;
        [[nodiscard]] bool dominates(std::uint32_t dominatorBlockId, std::uint32_t blockId) const;
    };

    [[nodiscard]] DominatorTree buildDominatorTree(const Function& function);
}
