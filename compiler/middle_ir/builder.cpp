#include "builder.hpp"

namespace bolt::mir
{
    Builder::Builder(Module& module)
        : m_module(module)
    {
    }

    Function& Builder::createFunction(std::string_view name)
    {
        m_module.functions.emplace_back();
        Function& fn = m_module.functions.back();
        fn.name = std::string{name};
        fn.parameters.clear();
        fn.hasReturnType = false;
        fn.returnType.clear();
        fn.returnIsLive = false;
        fn.blocks.clear();
        fn.nextBlockId = 0;
        fn.nextValueId = 0;
        fn.isBlueprintConstructor = false;
        fn.isBlueprintDestructor = false;
        fn.blueprintName.reset();
        return fn;
    }

    BasicBlock& Builder::appendBlock(Function& function, std::string_view name)
    {
        function.blocks.emplace_back();
        BasicBlock& block = function.blocks.back();
        block.id = function.nextBlockId++;
        block.name = name.empty() ? ("block" + std::to_string(block.id)) : std::string{name};
        return block;
    }

    Instruction& Builder::appendInstruction(BasicBlock& block, InstructionKind kind)
    {
        block.instructions.emplace_back();
        Instruction& inst = block.instructions.back();
        inst.kind = kind;
        return inst;
    }

    Value Builder::makeTemporary(Function& function, std::string_view name)
    {
        Value value;
        value.kind = ValueKind::Temporary;
        value.id = function.nextValueId++;
        value.name = name.empty() ? ("t" + std::to_string(value.id)) : std::string{name};
        return value;
    }
} // namespace bolt::mir
