#include "verifier.hpp"

#include <algorithm>

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

        bool hasLiveParameters(const Function& function)
        {
            return std::any_of(function.parameters.begin(), function.parameters.end(), [](const Function::Parameter& parameter) {
                return parameter.isLive;
            });
        }

        bool functionHasReturnInstruction(const Function& function)
        {
            for (const auto& block : function.blocks)
            {
                for (const auto& instruction : block.instructions)
                {
                    if (instruction.kind == InstructionKind::Return)
                    {
                        return true;
                    }
                }
            }

            return false;
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

            const bool liveReturn = function.returnIsLive;
            const bool liveParameters = hasLiveParameters(function);

            if (liveReturn && !function.hasReturnType)
            {
                return false;
            }

            if ((liveReturn || liveParameters) && !functionHasReturnInstruction(function))
            {
                return false;
            }
        }

        return true;
    }
} // namespace bolt::mir
