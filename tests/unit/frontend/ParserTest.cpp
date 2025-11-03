#include <gtest/gtest.h>

#include "lexer.hpp"
#include "parser.hpp"

#include <algorithm>

namespace bolt::frontend
{
namespace
{
    CompilationUnit parseSource(const std::string& source, std::vector<Diagnostic>& outDiagnostics)
    {
        Lexer lexer{source, "test"};
        lexer.lex();

        Parser parser{lexer.tokens(), "test"};
        CompilationUnit unit = parser.parse();
        outDiagnostics = parser.diagnostics();
        return unit;
    }

    TEST(ParserTest, ParsesFunctionSignature)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public integer function sample(integer value) {
    return 0;
}
)";

        std::vector<Diagnostic> diagnostics;
        CompilationUnit unit = parseSource(source, diagnostics);

        ASSERT_TRUE(diagnostics.empty())
            << "First diagnostic: " << diagnostics.front().code << " - " << diagnostics.front().message;
        ASSERT_EQ(unit.functions.size(), 1u);
        const auto& fn = unit.functions.front();
        EXPECT_EQ(fn.name, "sample");
        ASSERT_EQ(fn.parameters.size(), 1u);
        EXPECT_EQ(fn.parameters.front().name, "value");
        EXPECT_TRUE(fn.returnType.has_value());
        EXPECT_EQ(*fn.returnType, "integer");
    }

    TEST(ParserTest, ParsesBlueprintConstructorAndDestructorNames)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public void function Widget() {}
public void function ~Widget() {}
)";

        std::vector<Diagnostic> diagnostics;
        CompilationUnit unit = parseSource(source, diagnostics);

        ASSERT_TRUE(diagnostics.empty())
            << "First diagnostic: " << diagnostics.front().code << " - " << diagnostics.front().message;
        ASSERT_EQ(unit.functions.size(), 2u);
        EXPECT_EQ(unit.functions[0].name, "Widget");
        EXPECT_EQ(unit.functions[1].name, "~Widget");
    }

    TEST(ParserTest, ParsesImportStatement)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

import demo.utils.core;

integer function dummy() {
    return 0;
}
)";

        std::vector<Diagnostic> diagnostics;
        CompilationUnit unit = parseSource(source, diagnostics);

        EXPECT_TRUE(diagnostics.empty());
        ASSERT_EQ(unit.imports.size(), 1u);
        EXPECT_EQ(unit.imports.front().modulePath, "demo.utils.core");
    }

    TEST(ParserTest, ReportsAttributesOnImport)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

[packed]
import demo.alpha;
)";

        std::vector<Diagnostic> diagnostics;
        CompilationUnit unit = parseSource(source, diagnostics);

        EXPECT_FALSE(diagnostics.empty());
        EXPECT_EQ(diagnostics.front().code, "BOLT-E2108");
        ASSERT_EQ(unit.imports.size(), 1u);
        EXPECT_EQ(unit.imports.front().modulePath, "demo.alpha");
    }

    TEST(ParserTest, ReportsMissingImportSemicolonWithFixIt)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

import demo.alpha
)";

        std::vector<Diagnostic> diagnostics;
        (void)parseSource(source, diagnostics);

        const auto missingSemicolon = std::find_if(
            diagnostics.begin(),
            diagnostics.end(),
            [](const Diagnostic& diag) { return diag.code == "BOLT-E2123"; });

        ASSERT_NE(missingSemicolon, diagnostics.end());
        ASSERT_TRUE(missingSemicolon->fixItHint.has_value());
        EXPECT_NE(missingSemicolon->fixItHint->find("Insert ';'"), std::string::npos);
    }

    TEST(ParserTest, ReportsModuleHeaderSemicolonFixIts)
    {
        const std::string source = R"(package demo.tests
module demo.tests

integer function demo() {
    return 0;
}
)";

        std::vector<Diagnostic> diagnostics;
        (void)parseSource(source, diagnostics);

        const auto packageFixIt = std::find_if(
            diagnostics.begin(),
            diagnostics.end(),
            [](const Diagnostic& diag) { return diag.code == "BOLT-E2104"; });
        ASSERT_NE(packageFixIt, diagnostics.end());
        ASSERT_TRUE(packageFixIt->fixItHint.has_value());
        EXPECT_NE(packageFixIt->fixItHint->find("Insert ';'"), std::string::npos);

        const auto moduleFixIt = std::find_if(
            diagnostics.begin(),
            diagnostics.end(),
            [](const Diagnostic& diag) { return diag.code == "BOLT-E2106"; });
        ASSERT_NE(moduleFixIt, diagnostics.end());
        ASSERT_TRUE(moduleFixIt->fixItHint.has_value());
        EXPECT_NE(moduleFixIt->fixItHint->find("Insert ';'"), std::string::npos);
    }

    TEST(ParserTest, RejectsLegacyParameterSyntax)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public function legacy(value: integer) -> integer {
    return 0;
}
)";

        std::vector<Diagnostic> diagnostics;
        (void)parseSource(source, diagnostics);

        ASSERT_FALSE(diagnostics.empty());
        const bool containsLegacyDiagnostic = std::any_of(
            diagnostics.begin(),
            diagnostics.end(),
            [](const Diagnostic& diag) { return diag.code == "BOLT-E2115"; });
        EXPECT_TRUE(containsLegacyDiagnostic);
    }

    TEST(ParserTest, RejectsLegacyFieldSyntax)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint Timer {
    start: integer32;
}
)";

        std::vector<Diagnostic> diagnostics;
        (void)parseSource(source, diagnostics);

        ASSERT_FALSE(diagnostics.empty());
        const bool containsFieldError = std::any_of(
            diagnostics.begin(),
            diagnostics.end(),
            [](const Diagnostic& diag) { return diag.code == "BOLT-E2153"; });
        EXPECT_TRUE(containsFieldError);
    }

    TEST(ParserTest, ParsesLinkFunctionAlongsideMultipleBlueprints)
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

        std::vector<Diagnostic> diagnostics;
        CompilationUnit unit = parseSource(source, diagnostics);

        EXPECT_TRUE(diagnostics.empty());
        ASSERT_EQ(unit.blueprints.size(), 2u);
        EXPECT_EQ(unit.blueprints[0].name, "FirstBlueprint");
        EXPECT_EQ(unit.blueprints[1].name, "SecondBlueprint");

        ASSERT_EQ(unit.blueprints[0].modifiers.size(), 1u);
        EXPECT_EQ(unit.blueprints[0].modifiers.front(), "public");
        ASSERT_EQ(unit.blueprints[0].fields.size(), 1u);
        EXPECT_EQ(unit.blueprints[0].fields.front().typeName, "integer");
        EXPECT_EQ(unit.blueprints[0].fields.front().name, "firstField");

        ASSERT_EQ(unit.blueprints[1].modifiers.size(), 1u);
        EXPECT_EQ(unit.blueprints[1].modifiers.front(), "public");
        ASSERT_EQ(unit.blueprints[1].fields.size(), 1u);
        EXPECT_EQ(unit.blueprints[1].fields.front().typeName, "integer");
        EXPECT_EQ(unit.blueprints[1].fields.front().name, "secondField");

        ASSERT_EQ(unit.functions.size(), 1u);
        const auto& fn = unit.functions.front();
        EXPECT_EQ(fn.name, "staticFunctionTest");
        ASSERT_EQ(fn.modifiers.size(), 2u);
        EXPECT_EQ(fn.modifiers[0], "public");
        EXPECT_EQ(fn.modifiers[1], "link");
        ASSERT_TRUE(fn.returnType.has_value());
        EXPECT_EQ(*fn.returnType, "integer");
        ASSERT_EQ(fn.parameters.size(), 1u);
        EXPECT_EQ(fn.parameters.front().typeName, "integer");
        EXPECT_EQ(fn.parameters.front().name, "value");
    }

    TEST(ParserTest, ParsesPointerAndReferenceUsage)
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

        std::vector<Diagnostic> diagnostics;
        CompilationUnit unit = parseSource(source, diagnostics);

        ASSERT_TRUE(diagnostics.empty())
            << "First diagnostic: " << diagnostics.front().code << " - " << diagnostics.front().message;
        ASSERT_EQ(unit.blueprints.size(), 1u);
        const auto& blueprint = unit.blueprints.front();
        EXPECT_EQ(blueprint.name, "PointerCarrier");
        ASSERT_EQ(blueprint.fields.size(), 2u);
        EXPECT_EQ(blueprint.fields[0].typeName, "pointer<byte>");
        EXPECT_EQ(blueprint.fields[0].name, "payload");
        EXPECT_EQ(blueprint.fields[1].typeName, "reference<byte>");
        EXPECT_EQ(blueprint.fields[1].name, "mirrorRef");

        ASSERT_EQ(unit.functions.size(), 1u);
        const auto& function = unit.functions.front();
        EXPECT_EQ(function.name, "example");
        ASSERT_TRUE(function.returnType.has_value());
        EXPECT_EQ(function.returnType.value(), "live integer");
        ASSERT_EQ(function.parameters.size(), 3u);
        EXPECT_EQ(function.parameters[0].typeName, "integer");
        EXPECT_EQ(function.parameters[0].name, "value");
        EXPECT_EQ(function.parameters[1].typeName, "pointer<byte>");
        EXPECT_EQ(function.parameters[1].name, "buffer");
        EXPECT_EQ(function.parameters[2].typeName, "reference<byte>");
        EXPECT_EQ(function.parameters[2].name, "mirrorRef");
    }

    TEST(ParserTest, CapturesStarPointerAndReferenceSyntax)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint SyntaxCarrier {
    integer* smartPointer;
    integer & smartReference;
    integer*& refToPointer;
}

public integer function build(integer* instance) {
    return 0;
}
)";

        std::vector<Diagnostic> diagnostics;
        CompilationUnit unit = parseSource(source, diagnostics);

        ASSERT_TRUE(diagnostics.empty())
            << "First diagnostic: " << diagnostics.front().code << " - " << diagnostics.front().message;

        ASSERT_EQ(unit.blueprints.size(), 1u);
        const auto& blueprint = unit.blueprints.front();
        ASSERT_EQ(blueprint.fields.size(), 3u);
        EXPECT_EQ(blueprint.fields[0].typeName, "integer*");
        EXPECT_EQ(blueprint.fields[1].typeName, "integer&");
        EXPECT_EQ(blueprint.fields[2].typeName, "integer*&");

        ASSERT_EQ(unit.functions.size(), 1u);
        const auto& function = unit.functions.front();
        ASSERT_EQ(function.parameters.size(), 1u);
        EXPECT_EQ(function.parameters[0].typeName, "integer*");
    }

    TEST(ParserTest, CapturesConstantQualifiedTypes)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint ConstantCarrier {
    pointer<constant byte> payload;
    constant byte terminator;
}

public pointer<constant byte> function toView(pointer<constant byte> input, constant byte sentinel) {
    return input;
}
)";

        std::vector<Diagnostic> diagnostics;
        CompilationUnit unit = parseSource(source, diagnostics);

        ASSERT_TRUE(diagnostics.empty())
            << "First diagnostic: " << diagnostics.front().code << " - " << diagnostics.front().message;

        ASSERT_EQ(unit.blueprints.size(), 1u);
        const auto& blueprint = unit.blueprints.front();
        ASSERT_EQ(blueprint.fields.size(), 2u);
        EXPECT_EQ(blueprint.fields[0].typeName, "pointer<constant byte>");
        EXPECT_EQ(blueprint.fields[0].name, "payload");
        EXPECT_EQ(blueprint.fields[1].typeName, "constant byte");
        EXPECT_EQ(blueprint.fields[1].name, "terminator");

        ASSERT_EQ(unit.functions.size(), 1u);
        const auto& function = unit.functions.front();
        ASSERT_TRUE(function.returnType.has_value());
        EXPECT_EQ(function.returnType.value(), "pointer<constant byte>");
        ASSERT_EQ(function.parameters.size(), 2u);
        EXPECT_EQ(function.parameters[0].typeName, "pointer<constant byte>");
        EXPECT_EQ(function.parameters[0].name, "input");
        EXPECT_EQ(function.parameters[1].typeName, "constant byte");
        EXPECT_EQ(function.parameters[1].name, "sentinel");
    }
}
} // namespace bolt::frontend




