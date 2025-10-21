#pragma once

#include "dominance_frontier.hpp"

#include <cstdint>
#include <vector>

namespace bolt::mir::passes
{
    [[nodiscard]] std::vector<const BasicBlock*> computePhiPlacement(
        const DominanceFrontier& frontiers,
        const std::vector<std::uint32_t>& definitionBlocks);
}
