#include "verifier.hpp"

#include <gtest/gtest.h>
#include <utility>

namespace bolt::mir
{
    namespace
    {
        TypeReference makeBuiltinType(const std::string& name)
        {
        TypeReference type{};
        type.kind = bolt::common::TypeKind::Named;
        type.name.components.emplace_back(name);
        type.isBuiltin = true;
        type.text = name;
        type.originalText = name;
        type.normalizedText = name;
        return type;
    }

        Module makeModule(Function function)
        {
            Module module;
            module.functions.push_back(std::move(function));
            return module;
        }

        Function makeBaseFunction()
        {
            Function function;
            function.name = "sample";

            BasicBlock entry;
            entry.id = 0;
            entry.name = "entry";

            function.blocks.push_back(std::move(entry));
            return function;
        }
    } // namespace

    TEST(VerifierTest, LiveFunctionWithReturnPasses)
    {
        Function function = makeBaseFunction();
        function.returnIsLive = true;
        function.hasReturnType = true;
        function.returnType = makeBuiltinType("integer");

        Instruction ret;
        ret.kind = InstructionKind::Return;
        function.blocks.front().instructions.push_back(ret);

        Module module = makeModule(function);
        EXPECT_TRUE(verify(module));
    }

    TEST(VerifierTest, LiveFunctionMissingReturnFails)
    {
        Function function = makeBaseFunction();
        Function::Parameter parameter;
        parameter.name = "value";
        parameter.type = makeBuiltinType("integer");
        parameter.isLive = true;
        function.parameters.push_back(parameter);

        Instruction branch;
        branch.kind = InstructionKind::Branch;
        branch.successors.push_back(function.blocks.front().id);
        function.blocks.front().instructions.push_back(branch);

        Module module = makeModule(function);
        EXPECT_FALSE(verify(module));
    }

    TEST(VerifierTest, LiveReturnMissingTypeFails)
    {
        Function function = makeBaseFunction();
        function.returnIsLive = true;
        function.hasReturnType = false;

        Instruction ret;
        ret.kind = InstructionKind::Return;
        function.blocks.front().instructions.push_back(ret);

        Module module = makeModule(function);
        EXPECT_FALSE(verify(module));
    }
} // namespace bolt::mir
