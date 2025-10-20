#include <gtest/gtest.h>

#include <string>

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
        EXPECT_TRUE(enforceLive(mirModule));
    }
}
} // namespace bolt::mir

