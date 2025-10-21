#include <gtest/gtest.h>

#include <string>

#include "lexer.hpp"
#include "parser.hpp"
#include "binder.hpp"

namespace
{
    bolt::frontend::CompilationUnit parseCompilationUnit(const std::string& source,
                                                          std::vector<bolt::frontend::Diagnostic>& diagnostics)
    {
        bolt::frontend::Lexer lexer{source, "binder-test"};
        lexer.lex();

        bolt::frontend::Parser parser{lexer.tokens(), "binder-test"};
        bolt::frontend::CompilationUnit unit = parser.parse();
        diagnostics = parser.diagnostics();
        return unit;
    }
}

namespace bolt::hir
{
namespace
{
    TEST(BinderTest, CapturesFunctionMetadata)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

[aligned(16)]
[systemRequest(identifier=2)]
public live integer32 function request(live integer32 param) {
    return param;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty())
            << "First diagnostic: " << parseDiagnostics.front().code << " - " << parseDiagnostics.front().message;

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty()) << "Binder diagnostics detected";

        ASSERT_EQ(module.functions.size(), 1u);
        const auto& fn = module.functions.front();
        EXPECT_EQ(fn.name, "request");
        ASSERT_EQ(fn.modifiers.size(), 1u);
        EXPECT_EQ(fn.modifiers.front(), "public");
        ASSERT_TRUE(fn.alignmentBytes.has_value());
        EXPECT_EQ(*fn.alignmentBytes, 16u);
        ASSERT_TRUE(fn.systemRequestId.has_value());
        EXPECT_EQ(*fn.systemRequestId, 2u);
        EXPECT_TRUE(fn.kernelMarkers.empty());
        EXPECT_TRUE(fn.returnIsLive);
        EXPECT_EQ(fn.returnType.text, "integer");
        ASSERT_EQ(fn.parameters.size(), 1u);
        EXPECT_EQ(fn.parameters.front().name, "param");
        EXPECT_EQ(fn.parameters.front().type.text, "integer");
        EXPECT_TRUE(fn.parameters.front().isLive);
    }

    TEST(BinderTest, DuplicateFunctionAttributeEmitsDiagnostic)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

[aligned(16)]
[aligned(8)]
integer function badAlign() {
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        (void)binder.bind();
        const auto& diags = binder.diagnostics();
        ASSERT_FALSE(diags.empty());
        EXPECT_EQ(diags.front().code, "BOLT-E2200");
    }

    TEST(BinderTest, CapturesBlueprintMetadata)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

[packed]
[aligned(64)]
public blueprint Timer {
    live integer32 start;
    [bits(8)] integer32 mode;
    [aligned(16)] [bits(4)] integer32 priority;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty());

        ASSERT_EQ(module.blueprints.size(), 1u);
        const auto& bp = module.blueprints.front();
        EXPECT_EQ(bp.name, "Timer");
        ASSERT_EQ(bp.modifiers.size(), 1u);
        EXPECT_EQ(bp.modifiers.front(), "public");
        EXPECT_TRUE(bp.isPacked);
        ASSERT_TRUE(bp.alignmentBytes.has_value());
        EXPECT_EQ(*bp.alignmentBytes, 64u);
        ASSERT_EQ(bp.fields.size(), 3u);

        const auto& startField = bp.fields[0];
        EXPECT_EQ(startField.name, "start");
        EXPECT_EQ(startField.type.text, "integer");
        EXPECT_TRUE(startField.isLive);
        EXPECT_FALSE(startField.bitWidth.has_value());

        const auto& modeField = bp.fields[1];
        EXPECT_EQ(modeField.name, "mode");
        ASSERT_TRUE(modeField.bitWidth.has_value());
        EXPECT_EQ(*modeField.bitWidth, 8u);

        const auto& prioField = bp.fields[2];
        ASSERT_TRUE(prioField.bitWidth.has_value());
        EXPECT_EQ(*prioField.bitWidth, 4u);
        ASSERT_TRUE(prioField.alignmentBytes.has_value());
        EXPECT_EQ(*prioField.alignmentBytes, 16u);
    }

    TEST(BinderTest, CapturesImports)
    {
        const std::string source = R"(package demo.tests; module demo.tests;
import demo.alpha;
import demo.beta.gamma;

public integer function sample() {
    return 0;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty());

        ASSERT_EQ(module.imports.size(), 2u);
        EXPECT_EQ(module.imports[0].modulePath, "demo.alpha");
        EXPECT_EQ(module.imports[1].modulePath, "demo.beta.gamma");
    }

    TEST(BinderTest, DuplicateImportsEmitDiagnostic)
    {
        const std::string source = R"(package demo.tests; module demo.tests;
import demo.alpha;
import demo.beta;
import demo.alpha;
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        const auto& diags = binder.diagnostics();
        ASSERT_FALSE(diags.empty());
        EXPECT_EQ(diags.front().code, "BOLT-E2218");
        ASSERT_EQ(module.imports.size(), 2u);
        EXPECT_EQ(module.imports[0].modulePath, "demo.alpha");
        EXPECT_EQ(module.imports[1].modulePath, "demo.beta");
    }

    TEST(BinderTest, RecordsLinkFunctionAcrossMultipleBlueprints)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint FirstBlueprint {
    integer firstField;
}

public blueprint SecondBlueprint {
    integer secondField;
}

public link integer function staticFunctionTest(integer value) {
    return value;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty())
            << "First diagnostic: " << parseDiagnostics.front().code << " - " << parseDiagnostics.front().message;

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty()) << "Binder diagnostics detected";

        ASSERT_EQ(module.blueprints.size(), 2u);
        EXPECT_EQ(module.blueprints[0].name, "FirstBlueprint");
        EXPECT_EQ(module.blueprints[1].name, "SecondBlueprint");

        ASSERT_EQ(module.blueprints[0].modifiers.size(), 1u);
        EXPECT_EQ(module.blueprints[0].modifiers.front(), "public");
        ASSERT_EQ(module.blueprints[0].fields.size(), 1u);
        EXPECT_EQ(module.blueprints[0].fields.front().type.text, "integer");
        EXPECT_EQ(module.blueprints[0].fields.front().name, "firstField");

        ASSERT_EQ(module.blueprints[1].modifiers.size(), 1u);
        EXPECT_EQ(module.blueprints[1].modifiers.front(), "public");
        ASSERT_EQ(module.blueprints[1].fields.size(), 1u);
        EXPECT_EQ(module.blueprints[1].fields.front().type.text, "integer");
        EXPECT_EQ(module.blueprints[1].fields.front().name, "secondField");

        ASSERT_EQ(module.functions.size(), 1u);
        const auto& fn = module.functions.front();
        EXPECT_EQ(fn.name, "staticFunctionTest");
        ASSERT_EQ(fn.modifiers.size(), 2u);
        EXPECT_EQ(fn.modifiers[0], "public");
        EXPECT_EQ(fn.modifiers[1], "link");
        EXPECT_TRUE(fn.hasReturnType);
        EXPECT_EQ(fn.returnType.text, "integer");
        ASSERT_EQ(fn.parameters.size(), 1u);
        EXPECT_EQ(fn.parameters.front().type.text, "integer");
        EXPECT_EQ(fn.parameters.front().name, "value");
    }

    TEST(BinderTest, CapturesPointerAndReferenceTypes)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint PointerCarrier {
    pointer<byte> payload;
    reference<byte> mirrorRef;
}

public live integer function example(integer value, pointer<byte> buffer, reference<byte> mirrorRef) {
    return value + 1;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty())
            << "First diagnostic: " << parseDiagnostics.front().code << " - " << parseDiagnostics.front().message;

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty()) << "Binder diagnostics detected";

        ASSERT_EQ(module.blueprints.size(), 1u);
        const auto& blueprint = module.blueprints.front();
        ASSERT_EQ(blueprint.fields.size(), 2u);
        EXPECT_EQ(blueprint.fields[0].type.text, "pointer<byte>");
        EXPECT_EQ(blueprint.fields[0].name, "payload");
        EXPECT_EQ(blueprint.fields[1].type.text, "reference<byte>");
        EXPECT_EQ(blueprint.fields[1].name, "mirrorRef");

        ASSERT_EQ(module.functions.size(), 1u);
        const auto& function = module.functions.front();
        EXPECT_EQ(function.name, "example");
        ASSERT_EQ(function.parameters.size(), 3u);
        EXPECT_EQ(function.parameters[0].type.text, "integer");
        EXPECT_EQ(function.parameters[0].name, "value");
        EXPECT_EQ(function.parameters[1].type.text, "pointer<byte>");
        EXPECT_EQ(function.parameters[1].name, "buffer");
        EXPECT_EQ(function.parameters[2].type.text, "reference<byte>");
        EXPECT_EQ(function.parameters[2].name, "mirrorRef");
        EXPECT_TRUE(function.hasReturnType);
        EXPECT_EQ(function.returnType.text, "integer");
        EXPECT_TRUE(function.returnIsLive);
    }
}
} // namespace bolt::hir





