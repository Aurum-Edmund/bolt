#include "lowering.hpp"

#include "builder.hpp"

namespace bolt::mir
{
    Module lowerFromHir(const hir::Module& hirModule)
    {
        Module module;
        module.name = hirModule.moduleName;

        Builder builder{module};

        for (const auto& hirFunction : hirModule.functions)
        {
            auto& mirFunction = builder.createFunction(hirFunction.name);
            auto& entryBlock = builder.appendBlock(mirFunction, "entry");

            if (!hirFunction.modifiers.empty())
            {
                std::string detail = "modifiers:";
                for (const auto& modifier : hirFunction.modifiers)
                {
                    detail += " " + modifier;
                }
                auto& inst = builder.appendInstruction(entryBlock, InstructionKind::Unary);
                inst.detail = detail;
            }

            builder.appendInstruction(entryBlock, InstructionKind::Return).detail = "stub";
        }

        return module;
    }
} // namespace bolt::mir

