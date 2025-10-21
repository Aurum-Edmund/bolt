#include "canonical.hpp"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

namespace bolt::mir
{
    namespace
    {
        std::string describeValue(const Value& value)
        {
            std::ostringstream stream;
            stream << static_cast<int>(value.kind) << ':' << value.id;
            if (!value.name.empty())
            {
                stream << ':' << value.name;
            }
            return stream.str();
        }

        std::string canonicalDetail(const Instruction& inst)
        {
            std::ostringstream stream;
            stream << static_cast<int>(inst.kind);
            stream << ' ';
            stream << inst.detail;
            if (inst.result.has_value())
            {
                stream << " res " << describeValue(*inst.result);
            }
            if (!inst.operands.empty())
            {
                stream << " ops";
                for (std::size_t index = 0; index < inst.operands.size(); ++index)
                {
                    const auto& operand = inst.operands[index];
                    stream << (index == 0 ? ' ' : ',');
                    stream << describeValue(operand.value);
                    if (operand.predecessorBlockId.has_value())
                    {
                        stream << "@" << *operand.predecessorBlockId;
                    }
                }
            }
            if (!inst.successors.empty())
            {
                stream << " succ";
                for (std::size_t index = 0; index < inst.successors.size(); ++index)
                {
                    stream << (index == 0 ? ' ' : ',');
                    stream << inst.successors[index];
                }
            }
            return stream.str();
        }

        std::vector<const Function*> sortedFunctions(const Module& module)
        {
            std::vector<const Function*> result;
            result.reserve(module.functions.size());
            for (const auto& fn : module.functions)
            {
                result.push_back(&fn);
            }
            std::sort(result.begin(), result.end(), [](const Function* a, const Function* b) {
                return a->name < b->name;
            });
            return result;
        }

        std::vector<const BasicBlock*> sortedBlocks(const Function& function)
        {
            std::vector<const BasicBlock*> result;
            result.reserve(function.blocks.size());
            for (const auto& block : function.blocks)
            {
                result.push_back(&block);
            }
            std::sort(result.begin(), result.end(), [](const BasicBlock* a, const BasicBlock* b) {
                if (a->id == b->id)
                {
                    return a->name < b->name;
                }
                return a->id < b->id;
            });
            return result;
        }
    } // namespace

    std::string canonicalPrint(const Module& module)
    {
        std::ostringstream stream;
        stream << "module " << module.moduleName << '\n';
        stream << "package " << module.packageName << '\n';
        stream << "canonical " << module.canonicalModulePath << '\n';

        std::vector<std::string> imports = module.imports;
        std::sort(imports.begin(), imports.end());
        for (const auto& importName : imports)
        {
            stream << "import " << importName << '\n';
        }

        std::vector<std::pair<std::string, std::string>> resolved;
        resolved.reserve(module.resolvedImports.size());
        for (const auto& entry : module.resolvedImports)
        {
            std::string detail = entry.modulePath;
            if (entry.canonicalModulePath.has_value())
            {
                detail += " [" + *entry.canonicalModulePath + "]";
            }
            if (entry.filePath.has_value())
            {
                detail += " -> ";
                detail += *entry.filePath;
            }
            resolved.emplace_back(entry.modulePath, std::move(detail));
        }
        std::sort(resolved.begin(), resolved.end(), [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
        for (const auto& pair : resolved)
        {
            stream << "resolved " << pair.second << '\n';
        }

        for (const auto* function : sortedFunctions(module))
        {
            stream << "function " << function->name << '\n';

            for (const auto* block : sortedBlocks(*function))
            {
                stream << "  block " << block->id << ' ' << block->name << '\n';

                for (std::size_t index = 0; index < block->instructions.size(); ++index)
                {
                    stream << "    inst " << index << ' ' << canonicalDetail(block->instructions[index]) << '\n';
                }
            }
        }

        return stream.str();
    }

    std::uint64_t canonicalHash(const Module& module)
    {
        const std::string canonical = canonicalPrint(module);
        constexpr std::uint64_t offset = 1469598103934665603ull;
        constexpr std::uint64_t prime = 1099511628211ull;

        std::uint64_t hash = offset;
        for (unsigned char c : canonical)
        {
            hash ^= static_cast<std::uint64_t>(c);
            hash *= prime;
        }
        return hash;
    }
} // namespace bolt::mir
