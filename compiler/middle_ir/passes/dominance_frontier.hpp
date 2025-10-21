#pragma once

#include "dominator_tree.hpp"

#include <vector>

namespace bolt::mir::passes
{
    struct DominanceFrontierNode
    {
        const BasicBlock* block{nullptr};
        std::vector<const BasicBlock*> frontier;
    };

    struct DominanceFrontier
    {
        const Function* function{nullptr};
        std::vector<DominanceFrontierNode> nodes;

        [[nodiscard]] const DominanceFrontierNode* findNode(std::uint32_t blockId) const;
    };

    [[nodiscard]] DominanceFrontier buildDominanceFrontier(const Function& function, const DominatorTree& tree);
}
