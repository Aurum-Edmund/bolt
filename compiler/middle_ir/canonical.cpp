#include "canonical.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

namespace bolt::mir
{
    namespace
    {
        std::string canonicalDetail(const Instruction& inst)
        {
            std::ostringstream stream;
            stream << static_cast<int>(inst.kind);
            stream << ' ';
            stream << inst.detail;
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
        stream << "module " << module.name << '\n';

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
