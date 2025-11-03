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

    TEST(BinderTest, RecordsBlueprintLifecycleFunctions)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint Widget {
    integer value;
}

public void function Widget(integer value) {}
public void function ~Widget() {}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty())
            << "First diagnostic: " << parseDiagnostics.front().code << " - " << parseDiagnostics.front().message;

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty());

        ASSERT_EQ(module.functions.size(), 2u);
        const auto& constructor = module.functions[0];
        const auto& destructor = module.functions[1];
        EXPECT_EQ(constructor.name, "Widget");
        EXPECT_TRUE(constructor.isBlueprintConstructor);
        EXPECT_FALSE(constructor.isBlueprintDestructor);
        ASSERT_TRUE(constructor.blueprintName.has_value());
        EXPECT_EQ(*constructor.blueprintName, "Widget");
        ASSERT_EQ(constructor.parameters.size(), 1u);
        EXPECT_TRUE(constructor.parameters.front().hasDefaultValue);
        EXPECT_EQ(constructor.parameters.front().defaultValue, "0");
        EXPECT_FALSE(constructor.parameters.front().requiresExplicitValue);

        EXPECT_EQ(destructor.name, "~Widget");
        EXPECT_TRUE(destructor.isBlueprintDestructor);
        EXPECT_FALSE(destructor.isBlueprintConstructor);
        ASSERT_TRUE(destructor.blueprintName.has_value());
        EXPECT_EQ(*destructor.blueprintName, "Widget");
    }

    TEST(BinderTest, ConstructorParametersCaptureSaneDefaults)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint Sample {
    integer value;
}

public void function Sample(integer value, float amount, integer* pointerValue) {}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty());

        ASSERT_EQ(module.functions.size(), 1u);
        const auto& constructor = module.functions.front();
        ASSERT_EQ(constructor.parameters.size(), 3u);
        EXPECT_TRUE(constructor.parameters[0].hasDefaultValue);
        EXPECT_EQ(constructor.parameters[0].defaultValue, "0");
        EXPECT_TRUE(constructor.parameters[1].hasDefaultValue);
        EXPECT_EQ(constructor.parameters[1].defaultValue, "0.0");
        EXPECT_TRUE(constructor.parameters[2].hasDefaultValue);
        EXPECT_EQ(constructor.parameters[2].defaultValue, "null");
        EXPECT_FALSE(constructor.parameters[0].requiresExplicitValue);
        EXPECT_FALSE(constructor.parameters[1].requiresExplicitValue);
        EXPECT_FALSE(constructor.parameters[2].requiresExplicitValue);
    }

    TEST(BinderTest, ConstructorReferenceParameterRequiresExplicitValue)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint Holder {
    integer value;
}

public void function Holder(integer& value) {}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        const auto& diags = binder.diagnostics();
        ASSERT_EQ(diags.size(), 1u);
        EXPECT_EQ(diags.front().code, "BOLT-W2210");
        EXPECT_TRUE(diags.front().isWarning);

        ASSERT_EQ(module.functions.size(), 1u);
        const auto& constructor = module.functions.front();
        ASSERT_EQ(constructor.parameters.size(), 1u);
        EXPECT_FALSE(constructor.parameters.front().hasDefaultValue);
        EXPECT_TRUE(constructor.parameters.front().requiresExplicitValue);
    }

    TEST(BinderTest, DestructorRejectsParameters)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint Widget {
    integer value;
}

public void function ~Widget(integer value) {}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        const auto& diags = binder.diagnostics();
        ASSERT_EQ(diags.size(), 1u);
        EXPECT_EQ(diags.front().code, "BOLT-E2230");
        EXPECT_FALSE(diags.front().isWarning);

        ASSERT_EQ(module.functions.size(), 1u);
        EXPECT_TRUE(module.functions.front().isBlueprintDestructor);
    }

    TEST(BinderTest, CapturesTypeReferenceMetadata)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public std.core.result.Result<void, WriteError> function process(
    pointer<byte> buffer,
    pointer<constant byte> readonlyBuffer,
    reference<std.core.result.Result<void, WriteError>> state) {
    return state;
}

public blueprint Holder {
    pointer<byte> data;
    pointer<constant byte> readonly;
    reference<pointer<byte>> nested;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty());

        ASSERT_EQ(module.functions.size(), 1u);
        const auto& fn = module.functions.front();
        ASSERT_TRUE(fn.hasReturnType);
        EXPECT_EQ(fn.returnType.kind, TypeKind::Named);
        EXPECT_TRUE(fn.returnType.isGeneric());
        ASSERT_EQ(fn.returnType.name.components.size(), 4u);
        EXPECT_EQ(fn.returnType.name.components[0], "std");
        EXPECT_EQ(fn.returnType.name.components[3], "Result");
        ASSERT_EQ(fn.returnType.genericArguments.size(), 2u);
        EXPECT_EQ(fn.returnType.genericArguments[0].text, "void");
        EXPECT_EQ(fn.returnType.genericArguments[1].text, "WriteError");

        ASSERT_EQ(fn.parameters.size(), 3u);
        const auto& bufferParam = fn.parameters[0];
        EXPECT_EQ(bufferParam.type.kind, TypeKind::Pointer);
        ASSERT_EQ(bufferParam.type.genericArguments.size(), 1u);
        EXPECT_EQ(bufferParam.type.genericArguments[0].text, "byte");

        const auto& readonlyParam = fn.parameters[1];
        EXPECT_EQ(readonlyParam.type.kind, TypeKind::Pointer);
        ASSERT_EQ(readonlyParam.type.genericArguments.size(), 1u);
        const auto& readonlyInner = readonlyParam.type.genericArguments[0];
        EXPECT_EQ(readonlyInner.kind, TypeKind::Named);
        ASSERT_EQ(readonlyInner.qualifiers.size(), 1u);
        EXPECT_EQ(readonlyInner.qualifiers.front(), "constant");
        EXPECT_TRUE(readonlyInner.hasQualifier("constant"));
        EXPECT_EQ(readonlyInner.text, "constant byte");
        ASSERT_EQ(readonlyInner.name.components.size(), 1u);
        EXPECT_EQ(readonlyInner.name.components.front(), "byte");

        const auto& stateParam = fn.parameters[2];
        EXPECT_EQ(stateParam.type.kind, TypeKind::Reference);
        ASSERT_EQ(stateParam.type.genericArguments.size(), 1u);
        const auto& stateInner = stateParam.type.genericArguments[0];
        EXPECT_EQ(stateInner.kind, TypeKind::Named);
        EXPECT_TRUE(stateInner.isGeneric());
        ASSERT_EQ(stateInner.genericArguments.size(), 2u);
        EXPECT_EQ(stateInner.genericArguments[0].text, "void");
        EXPECT_EQ(stateInner.genericArguments[1].text, "WriteError");

        ASSERT_EQ(module.blueprints.size(), 1u);
        const auto& blueprint = module.blueprints.front();
        ASSERT_EQ(blueprint.fields.size(), 3u);
        EXPECT_EQ(blueprint.fields[0].type.kind, TypeKind::Pointer);
        ASSERT_EQ(blueprint.fields[0].type.genericArguments.size(), 1u);
        EXPECT_EQ(blueprint.fields[0].type.genericArguments[0].text, "byte");
        ASSERT_EQ(blueprint.fields[1].type.kind, TypeKind::Pointer);
        ASSERT_EQ(blueprint.fields[1].type.genericArguments.size(), 1u);
        const auto& readonlyFieldInner = blueprint.fields[1].type.genericArguments[0];
        EXPECT_EQ(readonlyFieldInner.kind, TypeKind::Named);
        ASSERT_EQ(readonlyFieldInner.qualifiers.size(), 1u);
        EXPECT_EQ(readonlyFieldInner.qualifiers.front(), "constant");
        EXPECT_TRUE(readonlyFieldInner.hasQualifier("constant"));
        EXPECT_EQ(readonlyFieldInner.text, "constant byte");
        ASSERT_EQ(readonlyFieldInner.name.components.size(), 1u);
        EXPECT_EQ(readonlyFieldInner.name.components.front(), "byte");
        EXPECT_EQ(blueprint.fields[2].type.kind, TypeKind::Reference);
        ASSERT_EQ(blueprint.fields[2].type.genericArguments.size(), 1u);
        EXPECT_EQ(blueprint.fields[2].type.genericArguments[0].kind, TypeKind::Pointer);
        ASSERT_EQ(blueprint.fields[2].type.genericArguments[0].genericArguments.size(), 1u);
        EXPECT_EQ(blueprint.fields[2].type.genericArguments[0].genericArguments[0].text, "byte");
    }

    TEST(BinderTest, CapturesArrayTypeMetadata)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public void function reshape(pointer<byte>[4][2] blocks, integer[] dynamicValues) {
    return;
}

public blueprint Matrix {
    integer[8][3] data;
    reference<pointer<byte>[4]> nested;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty());

        ASSERT_EQ(module.functions.size(), 1u);
        const auto& fn = module.functions.front();
        ASSERT_EQ(fn.parameters.size(), 2u);

        const auto& blocksParam = fn.parameters[0];
        EXPECT_EQ(blocksParam.type.text, "pointer<byte>[4][2]");
        EXPECT_EQ(blocksParam.type.kind, TypeKind::Array);
        ASSERT_TRUE(blocksParam.type.arrayLength.has_value());
        EXPECT_EQ(*blocksParam.type.arrayLength, 2u);
        ASSERT_EQ(blocksParam.type.genericArguments.size(), 1u);
        const auto& blocksInnerArray = blocksParam.type.genericArguments[0];
        EXPECT_EQ(blocksInnerArray.text, "pointer<byte>[4]");
        EXPECT_EQ(blocksInnerArray.kind, TypeKind::Array);
        ASSERT_TRUE(blocksInnerArray.arrayLength.has_value());
        EXPECT_EQ(*blocksInnerArray.arrayLength, 4u);
        ASSERT_EQ(blocksInnerArray.genericArguments.size(), 1u);
        const auto& blocksElement = blocksInnerArray.genericArguments[0];
        EXPECT_EQ(blocksElement.text, "pointer<byte>");
        EXPECT_EQ(blocksElement.kind, TypeKind::Pointer);
        ASSERT_EQ(blocksElement.genericArguments.size(), 1u);
        EXPECT_EQ(blocksElement.genericArguments[0].text, "byte");

        const auto& dynamicParam = fn.parameters[1];
        EXPECT_EQ(dynamicParam.type.text, "integer[]");
        EXPECT_EQ(dynamicParam.type.kind, TypeKind::Array);
        EXPECT_FALSE(dynamicParam.type.arrayLength.has_value());
        ASSERT_EQ(dynamicParam.type.genericArguments.size(), 1u);
        EXPECT_EQ(dynamicParam.type.genericArguments[0].text, "integer");

        ASSERT_EQ(module.blueprints.size(), 1u);
        const auto& blueprint = module.blueprints.front();
        ASSERT_EQ(blueprint.fields.size(), 2u);

        const auto& dataField = blueprint.fields[0];
        EXPECT_EQ(dataField.type.text, "integer[8][3]");
        EXPECT_EQ(dataField.type.kind, TypeKind::Array);
        ASSERT_TRUE(dataField.type.arrayLength.has_value());
        EXPECT_EQ(*dataField.type.arrayLength, 3u);
        ASSERT_EQ(dataField.type.genericArguments.size(), 1u);
        const auto& dataInner = dataField.type.genericArguments[0];
        EXPECT_EQ(dataInner.text, "integer[8]");
        EXPECT_EQ(dataInner.kind, TypeKind::Array);
        ASSERT_TRUE(dataInner.arrayLength.has_value());
        EXPECT_EQ(*dataInner.arrayLength, 8u);
        ASSERT_EQ(dataInner.genericArguments.size(), 1u);
        EXPECT_EQ(dataInner.genericArguments[0].text, "integer");

        const auto& nestedField = blueprint.fields[1];
        EXPECT_EQ(nestedField.type.kind, TypeKind::Reference);
        ASSERT_EQ(nestedField.type.genericArguments.size(), 1u);
        const auto& nestedInner = nestedField.type.genericArguments[0];
        EXPECT_EQ(nestedInner.kind, TypeKind::Array);
        EXPECT_EQ(nestedInner.text, "pointer<byte>[4]");
        ASSERT_TRUE(nestedInner.arrayLength.has_value());
        EXPECT_EQ(*nestedInner.arrayLength, 4u);
        ASSERT_EQ(nestedInner.genericArguments.size(), 1u);
        const auto& nestedElement = nestedInner.genericArguments[0];
        EXPECT_EQ(nestedElement.kind, TypeKind::Pointer);
        EXPECT_EQ(nestedElement.text, "pointer<byte>");
        ASSERT_EQ(nestedElement.genericArguments.size(), 1u);
        EXPECT_EQ(nestedElement.genericArguments[0].text, "byte");
    }

    TEST(BinderTest, CapturesConstantArrayMetadata)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public void function checksum(constant byte[16] payload) {
    return;
}

public blueprint Packet {
    constant byte[32] digest;
    pointer<constant byte[8]> view;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty());

        ASSERT_EQ(module.functions.size(), 1u);
        const auto& fn = module.functions.front();
        ASSERT_EQ(fn.parameters.size(), 1u);

        const auto& payloadParam = fn.parameters[0];
        EXPECT_EQ(payloadParam.type.kind, TypeKind::Array);
        EXPECT_EQ(payloadParam.type.text, "constant byte[16]");
        ASSERT_EQ(payloadParam.type.genericArguments.size(), 1u);
        const auto& payloadElement = payloadParam.type.genericArguments[0];
        EXPECT_EQ(payloadElement.kind, TypeKind::Named);
        ASSERT_EQ(payloadElement.qualifiers.size(), 1u);
        EXPECT_EQ(payloadElement.qualifiers.front(), "constant");
        EXPECT_EQ(payloadElement.text, "constant byte");

        ASSERT_EQ(module.blueprints.size(), 1u);
        const auto& blueprint = module.blueprints.front();
        ASSERT_EQ(blueprint.fields.size(), 2u);

        const auto& digestField = blueprint.fields[0];
        EXPECT_EQ(digestField.type.kind, TypeKind::Array);
        EXPECT_EQ(digestField.type.text, "constant byte[32]");
        ASSERT_EQ(digestField.type.genericArguments.size(), 1u);
        const auto& digestElement = digestField.type.genericArguments[0];
        EXPECT_EQ(digestElement.kind, TypeKind::Named);
        ASSERT_EQ(digestElement.qualifiers.size(), 1u);
        EXPECT_EQ(digestElement.qualifiers.front(), "constant");
        EXPECT_EQ(digestElement.text, "constant byte");

        const auto& viewField = blueprint.fields[1];
        EXPECT_EQ(viewField.type.kind, TypeKind::Pointer);
        ASSERT_EQ(viewField.type.genericArguments.size(), 1u);
        const auto& viewInner = viewField.type.genericArguments[0];
        EXPECT_EQ(viewInner.kind, TypeKind::Array);
        EXPECT_EQ(viewInner.text, "constant byte[8]");
        ASSERT_EQ(viewInner.genericArguments.size(), 1u);
        const auto& viewElement = viewInner.genericArguments[0];
        EXPECT_EQ(viewElement.kind, TypeKind::Named);
        ASSERT_EQ(viewElement.qualifiers.size(), 1u);
        EXPECT_EQ(viewElement.qualifiers.front(), "constant");
        EXPECT_EQ(viewElement.text, "constant byte");
    }

    TEST(BinderTest, DuplicateConstantQualifierEmitsDiagnostic)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public void function duplicate(constant constant byte value) {
    return;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        (void)binder.bind();

        const auto& diagnostics = binder.diagnostics();
        ASSERT_FALSE(diagnostics.empty());
        EXPECT_EQ(diagnostics.front().code, "BOLT-E2301");
        EXPECT_NE(diagnostics.front().message.find("Duplicate 'constant' qualifier"), std::string::npos);
    }

    TEST(BinderTest, UnknownQualifierEmitsDiagnostic)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public void function misspelt(const byte value) {
    return;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        (void)binder.bind();

        const auto& diagnostics = binder.diagnostics();
        ASSERT_FALSE(diagnostics.empty());
        EXPECT_EQ(diagnostics.front().code, "BOLT-E2302");
        EXPECT_NE(diagnostics.front().message.find("Legacy 'const' qualifier"), std::string::npos);
        EXPECT_NE(diagnostics.front().message.find("use 'constant'"), std::string::npos);
    }

    TEST(BinderTest, TypeNamesStartingWithConstAreAccepted)
    {
        const std::string source = R"(package demo.tests; module demo.tests;

public blueprint Constellation {
    integer32 magnitude;
}

public void function observe(Constellation target) {
    return;
}
)";

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty());

        Binder binder{unit, "binder-test"};
        (void)binder.bind();

        EXPECT_TRUE(binder.diagnostics().empty());
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

    TEST(BinderTest, NormalizesStarPointerAndReferenceSyntax)
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

        std::vector<frontend::Diagnostic> parseDiagnostics;
        auto unit = parseCompilationUnit(source, parseDiagnostics);
        ASSERT_TRUE(parseDiagnostics.empty())
            << "First diagnostic: " << parseDiagnostics.front().code << " - " << parseDiagnostics.front().message;

        Binder binder{unit, "binder-star"};
        Module module = binder.bind();
        ASSERT_TRUE(binder.diagnostics().empty()) << "Binder diagnostics detected";

        ASSERT_EQ(module.blueprints.size(), 1u);
        const auto& blueprint = module.blueprints.front();
        ASSERT_EQ(blueprint.fields.size(), 3u);
        EXPECT_EQ(blueprint.fields[0].type.text, "pointer<integer>");
        EXPECT_EQ(blueprint.fields[1].type.text, "reference<integer>");
        EXPECT_EQ(blueprint.fields[2].type.text, "reference<pointer<integer>>");

        ASSERT_EQ(module.functions.size(), 1u);
        const auto& function = module.functions.front();
        ASSERT_EQ(function.parameters.size(), 1u);
        EXPECT_EQ(function.parameters[0].type.text, "pointer<integer>");
        EXPECT_EQ(function.returnType.text, "integer");
    }
}
} // namespace bolt::hir





