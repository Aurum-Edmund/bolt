#include "verifier.hpp"

namespace bolt::mir
{
    namespace
    {
        bool isTerminator(InstructionKind kind)
        {
            switch (kind)
            {
            case InstructionKind::Return:
            case InstructionKind::Branch:
            case InstructionKind::CondBranch:
                return true;
            default:
                return false;
            }
        }
    } // namespace

    bool verify(const Module& module)
    {
        for (const auto& function : module.functions)
        {
            if (function.blocks.empty())
            {
                return false;
            }

            for (const auto& block : function.blocks)
            {
                if (block.instructions.empty())
                {
                    return false;
                }

                if (!isTerminator(block.instructions.back().kind))
                {
                    return false;
                }
            }

            if (function.blocks.front().name != "entry")
            {
                return false;
            }
        }

        return true;
    }
} // namespace bolt::mir
