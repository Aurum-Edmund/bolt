#include <gtest/gtest.h>

#include <string>

#include "lexer.hpp"
#include "parser.hpp"
#include "binder.hpp"
#include "lowering.hpp"
#include "canonical.hpp"

namespace
{
    bolt::hir::Module buildHir(const std::string& source)
    {
        bolt::frontend::Lexer lexer{source, "canonical-test"};
        lexer.lex();

        bolt::frontend::Parser parser{lexer.tokens(), "canonical-test"};
        auto unit = parser.parse();
        EXPECT_TRUE(parser.diagnostics().empty());

        bolt::hir::Binder binder{unit, "canonical-test"};
        auto module = binder.bind();
        EXPECT_TRUE(binder.diagnostics().empty());
        return module;
    }
}

namespace bolt::mir
{
namespace
{
    TEST(CanonicalTest, ProducesDeterministicOutput)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public integer function alpha() {
    return 0;
}

public integer function beta(integer value) {
    return value;
}
)";

        auto hirModule = buildHir(source);
        Module mirModule = lowerFromHir(hirModule);

        const std::string canonical = canonicalPrint(mirModule);
        const std::uint64_t hash = canonicalHash(mirModule);

        const std::string expected =
            "module demo.tests\n"
            "package demo.tests\n"
            "canonical demo.tests\n"
            "function alpha\n"
            "  block 0 entry\n"
            "    inst 0 7 modifiers: public\n"
            "    inst 1 7 return integer\n"
            "    inst 2 1 function\n"
            "function beta\n"
            "  block 0 entry\n"
            "    inst 0 7 modifiers: public\n"
            "    inst 1 7 return integer\n"
            "    inst 2 7 param integer value\n"
            "    inst 3 1 function\n";

        EXPECT_EQ(canonical, expected);
        EXPECT_EQ(hash, canonicalHash(mirModule)); // stability check
    }
}
} // namespace bolt::mir




