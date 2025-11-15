#pragma once

#include "module.hpp"

namespace bolt::mir
{
    class Builder
    {
    public:
        explicit Builder(Module& module);

        Function& createFunction(std::string_view name);
        Blueprint& createBlueprint(std::string_view name);
        BasicBlock& appendBlock(Function& function, std::string_view name);
        Instruction& appendInstruction(BasicBlock& block, InstructionKind kind);
        Value makeTemporary(Function& function, std::string_view name = {});

    private:
        Module& m_module;
    };
} // namespace bolt::mir
