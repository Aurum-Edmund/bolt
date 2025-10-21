#include "live_enforcement.hpp"

#include <algorithm>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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

        void reportError(const Function& function,
                         std::string_view code,
                         std::string_view detail,
                         std::vector<LiveDiagnostic>& diagnostics)
        {
            LiveDiagnostic diagnostic;
            diagnostic.code = std::string{code};
            diagnostic.functionName = function.name;
            diagnostic.detail = std::string{detail};
            diagnostics.emplace_back(std::move(diagnostic));
        }

        std::unordered_map<std::uint32_t, const BasicBlock*> buildBlockMap(const Function& function)
        {
            std::unordered_map<std::uint32_t, const BasicBlock*> blockMap;
            for (const auto& block : function.blocks)
            {
                blockMap.emplace(block.id, &block);
            }
            return blockMap;
        }

        std::unordered_set<std::uint32_t> computeReachableBlocks(const Function& function)
        {
            std::unordered_set<std::uint32_t> reachable;
            if (function.blocks.empty())
            {
                return reachable;
            }

            const auto blockMap = buildBlockMap(function);

            std::queue<const BasicBlock*> worklist;
            const auto* entry = &function.blocks.front();
            worklist.push(entry);
            reachable.insert(entry->id);

            while (!worklist.empty())
            {
                const auto* current = worklist.front();
                worklist.pop();

                if (current->instructions.empty())
                {
                    continue;
                }

                const auto& terminator = current->instructions.back();
                for (auto successorId : terminator.successors)
                {
                    if (reachable.find(successorId) != reachable.end())
                    {
                        continue;
                    }

                    auto nextIt = blockMap.find(successorId);
                    if (nextIt == blockMap.end())
                    {
                        continue;
                    }

                    reachable.insert(successorId);
                    worklist.push(nextIt->second);
                }
            }

            return reachable;
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
                reportError(function,
                    "BOLT-E4101",
                    "live return declared without a concrete return type.",
                    diagnostics);
                success = false;
            }

            if (function.blocks.empty())
            {
                reportError(function,
                    "BOLT-E4102",
                    "live-qualified function has no basic blocks.",
                    diagnostics);
                success = false;
                continue;
            }

            if (!functionHasReturnInstruction(function))
            {
                reportError(function,
                    "BOLT-E4103",
                    "live-qualified function is missing a return instruction.",
                    diagnostics);
                success = false;
            }

            for (const auto& block : function.blocks)
            {
                if (block.instructions.empty())
                {
                    reportError(function,
                                "BOLT-E4104",
                                "live-qualified function contains an empty basic block '" + describeBlock(block) + "'.",
                                diagnostics);
                    success = false;
                    continue;
                }

                if (!isTerminator(block.instructions.back()))
                {
                    reportError(function,
                                "BOLT-E4105",
                                "Basic block '" + describeBlock(block)
                                    + "' must terminate with return or branch for live-qualified function.",
                                diagnostics);
                    success = false;
                }
            }

            const auto reachableBlocks = computeReachableBlocks(function);
            if (reachableBlocks.size() < function.blocks.size())
            {
                for (const auto& block : function.blocks)
                {
                    if (reachableBlocks.find(block.id) != reachableBlocks.end())
                    {
                        continue;
                    }

                    reportError(function,
                                "BOLT-E4106",
                                "Basic block '" + describeBlock(block)
                                    + "' is unreachable in live-qualified function.",
                                diagnostics);
                    success = false;
                }
            }
        }

        return success;
    }
}
