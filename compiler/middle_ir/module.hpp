#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bolt::mir
{
    enum class InstructionKind : std::uint16_t
    {
        Nop,
        Return,
        Call,
        Branch,
        CondBranch,
        Load,
        Store,
        Unary,
        Binary
    };

    enum class ValueKind : std::uint16_t
    {
        Temporary,
        Parameter,
        Constant,
        Global
    };

    struct Value
    {
        ValueKind kind{ValueKind::Temporary};
        std::uint32_t id{0};
        std::string name;
    };

    struct Operand
    {
        Value value;
    };

    struct Instruction
    {
        InstructionKind kind{InstructionKind::Nop};
        std::vector<Operand> operands;
        std::string detail;
        std::vector<std::uint32_t> successors;
    };

    struct BasicBlock
    {
        std::uint32_t id{0};
        std::string name;
        std::vector<Instruction> instructions;
    };

    struct Function
    {
        std::string name;
        struct Parameter
        {
            std::string typeName;
            std::string name;
            bool isLive{false};
        };
        std::vector<Parameter> parameters;
        bool hasReturnType{false};
        std::string returnType;
        bool returnIsLive{false};
        std::vector<BasicBlock> blocks;
        std::uint32_t nextBlockId{0};
        std::uint32_t nextValueId{0};
    };

    struct Module
    {
        std::string packageName;
        std::string moduleName;
        std::string canonicalModulePath;
        std::vector<std::string> imports;
        struct ResolvedImport
        {
            std::string modulePath;
            std::optional<std::string> canonicalModulePath;
            std::optional<std::string> filePath;
        };
        std::vector<ResolvedImport> resolvedImports;
        std::vector<Function> functions;
    };
} // namespace bolt::mir
