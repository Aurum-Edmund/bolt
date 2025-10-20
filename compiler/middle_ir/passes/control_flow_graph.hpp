#pragma once

#include "../module.hpp"

#include <vector>

namespace bolt::mir::passes
{
    struct ControlFlowGraphNode
    {
        const BasicBlock* block{nullptr};
        std::vector<const BasicBlock*> predecessors;
        std::vector<const BasicBlock*> successors;
    };

    struct ControlFlowGraph
    {
        const Function* function{nullptr};
        std::vector<ControlFlowGraphNode> nodes;

        [[nodiscard]] const ControlFlowGraphNode* findNode(std::uint32_t blockId) const;
    };

    [[nodiscard]] ControlFlowGraph buildControlFlowGraph(const Function& function);
}
