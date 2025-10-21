#include "ssa_conversion.hpp"

#include "control_flow_graph.hpp"
#include "dominance_frontier.hpp"
#include "dominator_tree.hpp"
#include "ssa_placement.hpp"

#include <algorithm>
#include <functional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace bolt::mir::passes
{
    namespace
    {
        struct VariableInfo
        {
            std::string baseName;
        };

        struct PhiRecord
        {
            std::uint32_t variableId{0};
            std::size_t instructionIndex{0};
        };

        std::string inferBaseName(const Value& value)
        {
            if (!value.name.empty())
            {
                return value.name;
            }

            return "t" + std::to_string(value.id);
        }

        void reportDiagnostic(const Function& function,
                               std::string code,
                               std::string detail,
                               std::vector<SsaDiagnostic>& diagnostics)
        {
            SsaDiagnostic diagnostic;
            diagnostic.code = std::move(code);
            diagnostic.functionName = function.name;
            diagnostic.detail = std::move(detail);
            diagnostics.emplace_back(std::move(diagnostic));
        }

        Value makeVersion(Function& function,
                          const VariableInfo& info,
                          std::uint32_t originalId,
                          std::unordered_map<std::uint32_t, std::uint32_t>& counters)
        {
            auto& nextCounter = counters[originalId];
            const auto version = nextCounter++;

            Value value;
            value.kind = ValueKind::Temporary;
            value.id = function.nextValueId++;
            value.name = info.baseName;
            if (version > 0)
            {
                value.name += "." + std::to_string(version);
            }
            return value;
        }

        void insertPhiNodes(Function& function,
                            const DominanceFrontier& frontiers,
                            const std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>& definitionBlocks,
                            const std::unordered_map<std::uint32_t, VariableInfo>& variables,
                            std::unordered_map<std::uint32_t, std::vector<PhiRecord>>& phiByBlock)
        {
            std::unordered_map<std::uint32_t, BasicBlock*> blockById;
            for (auto& block : function.blocks)
            {
                blockById.emplace(block.id, &block);
            }

            for (const auto& [variableId, blocks] : definitionBlocks)
            {
                auto variableIt = variables.find(variableId);
                if (variableIt == variables.end())
                {
                    continue;
                }

                const auto phiBlocks = computePhiPlacement(frontiers, blocks);
                for (const auto* block : phiBlocks)
                {
                    if (block == nullptr)
                    {
                        continue;
                    }

                    auto blockIt = blockById.find(block->id);
                    if (blockIt == blockById.end())
                    {
                        continue;
                    }

                    phiByBlock[block->id].push_back(PhiRecord{variableId, 0});
                }
            }

            for (auto& [blockId, records] : phiByBlock)
            {
                auto blockIt = blockById.find(blockId);
                if (blockIt == blockById.end())
                {
                    continue;
                }

                auto* block = blockIt->second;
                std::sort(records.begin(), records.end(), [](const PhiRecord& left, const PhiRecord& right) {
                    return left.variableId < right.variableId;
                });

                std::vector<Instruction> phiInstructions;
                phiInstructions.reserve(records.size());

                for (auto& record : records)
                {
                    Instruction phi;
                    phi.kind = InstructionKind::Phi;
                    phi.originalTemporaryId = record.variableId;
                    auto variableIt = variables.find(record.variableId);
                    if (variableIt != variables.end())
                    {
                        Value placeholder;
                        placeholder.kind = ValueKind::Temporary;
                        placeholder.id = record.variableId;
                        placeholder.name = variableIt->second.baseName;
                        phi.result = placeholder;
                        phi.detail = "phi " + variableIt->second.baseName;
                    }
                    record.instructionIndex = phiInstructions.size();
                    phiInstructions.emplace_back(std::move(phi));
                }

                block->instructions.insert(block->instructions.begin(), phiInstructions.begin(), phiInstructions.end());
            }
        }

        struct RenameContext
        {
            Function& function;
            const ControlFlowGraph& cfg;
            const DominatorTree& domTree;
            const std::unordered_map<std::uint32_t, VariableInfo>& variables;
            std::unordered_map<std::uint32_t, std::vector<PhiRecord>>& phiByBlock;
            std::unordered_map<std::uint32_t, std::vector<Value>> stacks;
            std::unordered_map<std::uint32_t, std::uint32_t> counters;
            std::unordered_map<std::uint32_t, BasicBlock*> blockById;
            std::vector<SsaDiagnostic>& diagnostics;
            std::unordered_set<std::uint32_t> visitedBlocks;
            bool success{true};

            RenameContext(Function& function,
                          const ControlFlowGraph& cfg,
                          const DominatorTree& domTree,
                          const std::unordered_map<std::uint32_t, VariableInfo>& variables,
                          std::unordered_map<std::uint32_t, std::vector<PhiRecord>>& phiByBlock,
                          std::vector<SsaDiagnostic>& diagnostics)
                : function(function)
                , cfg(cfg)
                , domTree(domTree)
                , variables(variables)
                , phiByBlock(phiByBlock)
                , diagnostics(diagnostics)
            {
                for (auto& block : function.blocks)
                {
                    blockById.emplace(block.id, &block);
                }
            }

            void pushVersion(std::uint32_t variableId, const Value& value)
            {
                stacks[variableId].push_back(value);
            }

            void popVersion(std::uint32_t variableId)
            {
                auto stackIt = stacks.find(variableId);
                if (stackIt == stacks.end() || stackIt->second.empty())
                {
                    return;
                }
                stackIt->second.pop_back();
            }

            [[nodiscard]] const Value* peekVersion(std::uint32_t variableId) const
            {
                auto stackIt = stacks.find(variableId);
                if (stackIt == stacks.end() || stackIt->second.empty())
                {
                    return nullptr;
                }
                return &stackIt->second.back();
            }

            Value createVersion(std::uint32_t variableId)
            {
                auto variableIt = variables.find(variableId);
                if (variableIt == variables.end())
                {
                    Value fallback;
                    fallback.kind = ValueKind::Temporary;
                    fallback.id = function.nextValueId++;
                    fallback.name = "t" + std::to_string(fallback.id);
                    return fallback;
                }

                return makeVersion(function, variableIt->second, variableId, counters);
            }

            void renameBlock(const DominatorTreeNode& node)
            {
                if (node.block == nullptr)
                {
                    return;
                }

                auto blockIt = blockById.find(node.block->id);
                if (blockIt == blockById.end())
                {
                    return;
                }

                if (!visitedBlocks.insert(node.block->id).second)
                {
                    return;
                }

                auto* block = blockIt->second;
                std::vector<std::uint32_t> pushedVariables;

                auto phiRecordsIt = phiByBlock.find(block->id);
                if (phiRecordsIt != phiByBlock.end())
                {
                    for (auto& record : phiRecordsIt->second)
                    {
                        auto& phiInst = block->instructions[record.instructionIndex];
                        const auto variableId = record.variableId;
                        const auto value = createVersion(variableId);
                        phiInst.result = value;
                        if (phiInst.detail.empty())
                        {
                            phiInst.detail = "phi";
                        }
                        else
                        {
                            phiInst.detail += " -> " + value.name;
                        }
                        pushVersion(variableId, value);
                        pushedVariables.push_back(variableId);
                    }
                }

                for (auto& instruction : block->instructions)
                {
                    if (instruction.kind == InstructionKind::Phi)
                    {
                        continue;
                    }

                    for (auto& operand : instruction.operands)
                    {
                        if (operand.value.kind != ValueKind::Temporary)
                        {
                            continue;
                        }

                        const auto originalId = operand.value.id;
                        const auto* value = peekVersion(originalId);
                        if (value == nullptr)
                        {
                            reportDiagnostic(function,
                                             "BOLT-E4301",
                                             "temporary value '" + std::to_string(originalId)
                                                 + "' used before definition in block " + block->name,
                                             diagnostics);
                            success = false;
                            continue;
                        }

                        operand.value = *value;
                    }

                    if (instruction.result.has_value()
                        && instruction.result->kind == ValueKind::Temporary)
                    {
                        const auto originalId = instruction.result->id;
                        auto newValue = createVersion(originalId);
                        instruction.originalTemporaryId = originalId;
                        instruction.result = newValue;
                        pushVersion(originalId, newValue);
                        pushedVariables.push_back(originalId);
                    }
                }

                const auto* cfgNode = cfg.findNode(block->id);
                if (cfgNode != nullptr)
                {
                    for (const auto* successor : cfgNode->successors)
                    {
                        if (successor == nullptr)
                        {
                            continue;
                        }

                        auto successorIt = blockById.find(successor->id);
                        if (successorIt == blockById.end())
                        {
                            continue;
                        }

                        auto successorPhiIt = phiByBlock.find(successor->id);
                        if (successorPhiIt == phiByBlock.end())
                        {
                            continue;
                        }

                        auto* successorBlock = successorIt->second;
                        for (const auto& record : successorPhiIt->second)
                        {
                            const auto* incomingValue = peekVersion(record.variableId);
                            if (incomingValue == nullptr)
                            {
                                reportDiagnostic(function,
                                                 "BOLT-E4302",
                                                 "missing definition for phi input of temporary '"
                                                     + std::to_string(record.variableId) + "' on edge from block "
                                                     + block->name + " to block " + successorBlock->name,
                                                 diagnostics);
                                success = false;
                                continue;
                            }

                            auto& phiInst = successorBlock->instructions[record.instructionIndex];

                            const auto alreadyPresent = std::any_of(
                                phiInst.operands.begin(),
                                phiInst.operands.end(),
                                [block](const Operand& operand) {
                                    return operand.predecessorBlockId.has_value()
                                           && operand.predecessorBlockId.value() == block->id;
                                });

                            if (alreadyPresent)
                            {
                                continue;
                            }

                            Operand operand;
                            operand.value = *incomingValue;
                            operand.predecessorBlockId = block->id;
                            phiInst.operands.push_back(std::move(operand));
                        }
                    }
                }

                for (const auto* child : node.children)
                {
                    if (child == nullptr)
                    {
                        continue;
                    }

                    const auto* childNode = domTree.findNode(child->id);
                    if (childNode != nullptr)
                    {
                        renameBlock(*childNode);
                    }
                }

                for (auto it = pushedVariables.rbegin(); it != pushedVariables.rend(); ++it)
                {
                    popVersion(*it);
                }
            }
        };
    } // namespace

    bool convertToSsa(Function& function, std::vector<SsaDiagnostic>& diagnostics)
    {
        if (function.blocks.empty())
        {
            return true;
        }

        auto cfg = buildControlFlowGraph(function);
        auto dominatorTree = buildDominatorTree(function);
        auto frontiers = buildDominanceFrontier(function, dominatorTree);

        std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> definitionBlocks;
        std::unordered_map<std::uint32_t, VariableInfo> variables;

        for (auto& block : function.blocks)
        {
            for (auto& instruction : block.instructions)
            {
                if (instruction.result.has_value()
                    && instruction.result->kind == ValueKind::Temporary)
                {
                    const auto variableId = instruction.result->id;
                    instruction.originalTemporaryId = variableId;
                    auto& blocks = definitionBlocks[variableId];
                    if (std::find(blocks.begin(), blocks.end(), block.id) == blocks.end())
                    {
                        blocks.push_back(block.id);
                    }

                    variables.emplace(variableId, VariableInfo{inferBaseName(*instruction.result)});
                }
            }
        }

        std::unordered_map<std::uint32_t, std::vector<PhiRecord>> phiByBlock;
        insertPhiNodes(function, frontiers, definitionBlocks, variables, phiByBlock);

        RenameContext context{function, cfg, dominatorTree, variables, phiByBlock, diagnostics};

        std::vector<const DominatorTreeNode*> roots;
        roots.reserve(dominatorTree.nodes.size());
        for (const auto& node : dominatorTree.nodes)
        {
            if (node.block == nullptr)
            {
                continue;
            }

            if (node.immediateDominator == nullptr)
            {
                roots.push_back(&node);
            }
        }

        std::sort(roots.begin(), roots.end(), [](const DominatorTreeNode* left, const DominatorTreeNode* right) {
            if (left == nullptr || right == nullptr)
            {
                return left < right;
            }

            return left->block->id < right->block->id;
        });

        for (const auto* root : roots)
        {
            if (root != nullptr)
            {
                context.renameBlock(*root);
            }
        }

        return context.success;
    }

    bool convertToSsa(Module& module, std::vector<SsaDiagnostic>& diagnostics)
    {
        bool success = true;
        for (auto& function : module.functions)
        {
            if (!convertToSsa(function, diagnostics))
            {
                success = false;
            }
        }
        return success;
    }
}

