#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "lexer.hpp"
#include "parser.hpp"
#include "binder.hpp"
#include "lowering.hpp"
#include "passes/live_enforcement.hpp"

namespace
{
    bolt::hir::Module buildHir(const std::string& source)
    {
        bolt::frontend::Lexer lexer{source, "live-enforcement-test"};
        lexer.lex();

        bolt::frontend::Parser parser{lexer.tokens(), "live-enforcement-test"};
        auto unit = parser.parse();
        EXPECT_TRUE(parser.diagnostics().empty());

        bolt::hir::Binder binder{unit, "live-enforcement-test"};
        auto module = binder.bind();
        EXPECT_TRUE(binder.diagnostics().empty());
        return module;
    }
}

namespace bolt::mir
{
namespace
{
    TEST(LiveEnforcementTest, AcceptsSimpleModule)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public integer function demo() {
    return 0;
}
)";

        auto hirModule = buildHir(source);
        Module mirModule = lowerFromHir(hirModule);
        std::vector<LiveDiagnostic> diagnostics;
        EXPECT_TRUE(enforceLive(mirModule, diagnostics));
        EXPECT_TRUE(diagnostics.empty());
    }

    TEST(LiveEnforcementTest, RejectsLiveFunctionWithoutReturn)
    {
        Module module;
        Function function;
        function.name = "demo";
        Function::Parameter parameter{};
        parameter.typeName = "integer";
        parameter.name = "value";
        parameter.isLive = true;
        function.parameters.push_back(parameter);
        function.blocks.emplace_back();
        module.functions.push_back(function);

        std::vector<LiveDiagnostic> diagnostics;
        EXPECT_FALSE(enforceLive(module, diagnostics));
        ASSERT_EQ(diagnostics.size(), 1u);
        EXPECT_EQ(diagnostics.front().code, "BOLT-E4101");
        EXPECT_EQ(diagnostics.front().functionName, "demo");
        EXPECT_NE(diagnostics.front().detail.find("missing a return instruction"), std::string::npos);
    }

    TEST(LiveEnforcementTest, RejectsLiveReturnWithoutType)
    {
        Module module;
        Function function;
        function.name = "requiresType";
        function.returnIsLive = true;
        function.blocks.emplace_back();
        function.blocks.back().instructions.push_back({InstructionKind::Return, {}, {}});
        module.functions.push_back(function);

        std::vector<LiveDiagnostic> diagnostics;
        EXPECT_FALSE(enforceLive(module, diagnostics));
        ASSERT_EQ(diagnostics.size(), 1u);
        EXPECT_EQ(diagnostics.front().code, "BOLT-E4101");
        EXPECT_EQ(diagnostics.front().functionName, "requiresType");
        EXPECT_NE(diagnostics.front().detail.find("return declared without a concrete return type"), std::string::npos);
    }
}
} // namespace bolt::mir

