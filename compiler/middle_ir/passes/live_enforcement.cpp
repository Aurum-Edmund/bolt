#include "live_enforcement.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace bolt::mir
{
    namespace
    {
        bool isTerminator(const Instruction& instruction)
        {
            switch (instruction.kind)
            {
                case InstructionKind::Return:
                case InstructionKind::Branch:
                case InstructionKind::CondBranch:
                    return true;
                default:
                    return false;
            }
        }

        bool functionHasReturnInstruction(const Function& function)
        {
            for (const auto& block : function.blocks)
            {
                for (const auto& inst : block.instructions)
                {
                    if (inst.kind == InstructionKind::Return)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        bool hasLiveParameters(const Function& function)
        {
            return std::any_of(function.parameters.begin(), function.parameters.end(), [](const Function::Parameter& parameter) {
                return parameter.isLive;
            });
        }

        std::string describeBlock(const BasicBlock& block)
        {
            if (!block.name.empty())
            {
                return block.name;
            }

            return "#" + std::to_string(block.id);
        }

        void reportError(const Function& function, std::string_view detail, std::vector<LiveDiagnostic>& diagnostics)
        {
            LiveDiagnostic diagnostic;
            diagnostic.code = "BOLT-E4101";
            diagnostic.functionName = function.name;
            diagnostic.detail = std::string{detail};
            diagnostics.emplace_back(std::move(diagnostic));
        }
    } // namespace

    bool enforceLive(Module& module, std::vector<LiveDiagnostic>& diagnostics)
    {
        bool success = true;

        for (const auto& function : module.functions)
        {
            const bool liveReturn = function.returnIsLive;
            const bool liveParameters = hasLiveParameters(function);

            if (!liveReturn && !liveParameters)
            {
                continue;
            }

            if (liveReturn && !function.hasReturnType)
            {
                reportError(function, "Live return declared without a concrete return type.", diagnostics);
                success = false;
            }

            if (function.blocks.empty())
            {
                reportError(function, "Live-qualified function has no basic blocks.", diagnostics);
                success = false;
                continue;
            }

            if (!functionHasReturnInstruction(function))
            {
                reportError(function, "Live-qualified function is missing a return instruction.", diagnostics);
                success = false;
            }

            for (const auto& block : function.blocks)
            {
                if (block.instructions.empty())
                {
                    reportError(function,
                                "Live-qualified function contains an empty basic block '" + describeBlock(block) + "'.",
                                diagnostics);
                    success = false;
                    continue;
                }

                if (!isTerminator(block.instructions.back()))
                {
                    reportError(function,
                                "Basic block '" + describeBlock(block)
                                    + "' must terminate with return or branch for Live-qualified function.",
                                diagnostics);
                    success = false;
                }
            }
        }

        return success;
    }
}
