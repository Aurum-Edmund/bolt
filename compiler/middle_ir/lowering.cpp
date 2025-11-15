#include "lowering.hpp"

#include "builder.hpp"

#include <sstream>
#include <string>
#include <utility>

namespace bolt::mir
{
    namespace
    {
        void appendDetail(Builder& builder, BasicBlock& block, std::string text)
        {
            auto& inst = builder.appendInstruction(block, InstructionKind::Unary);
            inst.detail = std::move(text);
        }

        std::string joinList(const std::vector<std::string>& list, std::string_view label)
        {
            if (list.empty())
            {
                return {};
            }

            std::ostringstream stream;
            stream << label;
            for (const auto& entry : list)
            {
                stream << ' ' << entry;
            }
            return stream.str();
        }

        const std::string& canonicalTypeText(const bolt::common::TypeReference& type)
        {
            if (!type.normalizedText.empty())
            {
                return type.normalizedText;
            }
            return type.text;
        }
    } // namespace

    Module lowerFromHir(const hir::Module& hirModule)
    {
        Module module;
        module.packageName = hirModule.packageName;
        module.moduleName = hirModule.moduleName;
        if (!hirModule.packageName.empty())
        {
            if (hirModule.packageName == hirModule.moduleName)
            {
                module.canonicalModulePath = hirModule.moduleName;
            }
            else
            {
                module.canonicalModulePath = hirModule.packageName + "::" + hirModule.moduleName;
            }
        }
        else
        {
            module.canonicalModulePath = hirModule.moduleName;
        }
        module.imports.reserve(hirModule.imports.size());
        for (const auto& importDecl : hirModule.imports)
        {
            module.imports.emplace_back(importDecl.modulePath);
        }

        Builder builder{module};

        for (const auto& hirFunction : hirModule.functions)
        {
            auto& mirFunction = builder.createFunction(hirFunction.name);
            mirFunction.isBlueprintConstructor = hirFunction.isBlueprintConstructor;
            mirFunction.isBlueprintDestructor = hirFunction.isBlueprintDestructor;
            mirFunction.blueprintName = hirFunction.blueprintName;
            auto& entryBlock = builder.appendBlock(mirFunction, "entry");

            if (!hirFunction.modifiers.empty())
            {
                appendDetail(builder, entryBlock, joinList(hirFunction.modifiers, "modifiers:"));
            }

            if (hirFunction.isInterruptHandler)
            {
                appendDetail(builder, entryBlock, "attr interruptHandler");
            }
            if (hirFunction.isBareFunction)
            {
                appendDetail(builder, entryBlock, "attr bareFunction");
            }
            if (hirFunction.isPageAligned)
            {
                appendDetail(builder, entryBlock, "attr pageAligned");
            }
            if (hirFunction.sectionName.has_value())
            {
                appendDetail(builder, entryBlock, "section " + *hirFunction.sectionName);
            }
            if (hirFunction.alignmentBytes.has_value())
            {
                appendDetail(builder, entryBlock, "aligned " + std::to_string(*hirFunction.alignmentBytes));
            }
            if (hirFunction.systemRequestId.has_value())
            {
                appendDetail(builder, entryBlock, "systemRequest " + std::to_string(*hirFunction.systemRequestId));
            }
            if (hirFunction.intrinsicName.has_value())
            {
                appendDetail(builder, entryBlock, "intrinsic " + *hirFunction.intrinsicName);
            }
            if (!hirFunction.kernelMarkers.empty())
            {
                appendDetail(builder, entryBlock, joinList(hirFunction.kernelMarkers, "kernelMarkers:"));
            }

            if (hirFunction.hasReturnType)
            {
                mirFunction.hasReturnType = true;
                mirFunction.returnType = hirFunction.returnType;
                mirFunction.returnIsLive = hirFunction.returnIsLive;
                std::string detail = "return ";
                detail += canonicalTypeText(mirFunction.returnType);
                if (hirFunction.returnIsLive)
                {
                    detail += " [live]";
                }
                appendDetail(builder, entryBlock, std::move(detail));
            }

            for (const auto& parameter : hirFunction.parameters)
            {
                Function::Parameter mirParameter{};
                mirParameter.type = parameter.type;
                mirParameter.name = parameter.name;
                mirParameter.isLive = parameter.isLive;
                mirParameter.hasDefaultValue = parameter.hasDefaultValue;
                mirParameter.requiresExplicitValue = parameter.requiresExplicitValue;
                mirParameter.defaultValue = parameter.defaultValue;
                mirFunction.parameters.emplace_back(std::move(mirParameter));
                std::string detail = "param ";
                detail += canonicalTypeText(parameter.type);
                detail += ' ';
                detail += parameter.name;
                if (parameter.isLive)
                {
                    detail += " [live]";
                }
                if (parameter.hasDefaultValue)
                {
                    detail += " default=" + parameter.defaultValue;
                }
                if (parameter.requiresExplicitValue)
                {
                    detail += " required";
                }
                appendDetail(builder, entryBlock, std::move(detail));
            }

            builder.appendInstruction(entryBlock, InstructionKind::Return).detail = "function";
        }

        for (const auto& blueprint : hirModule.blueprints)
        {
            auto& mirBlueprint = builder.createBlueprint(blueprint.name);
            mirBlueprint.modifiers = blueprint.modifiers;
            mirBlueprint.isPacked = blueprint.isPacked;
            mirBlueprint.alignmentBytes = blueprint.alignmentBytes;
            mirBlueprint.fields.reserve(blueprint.fields.size());
            for (const auto& field : blueprint.fields)
            {
                BlueprintField mirField{};
                mirField.name = field.name;
                mirField.type = field.type;
                mirField.isLive = field.isLive;
                mirField.bitWidth = field.bitWidth;
                mirField.alignmentBytes = field.alignmentBytes;
                mirBlueprint.fields.emplace_back(std::move(mirField));
            }

            auto& mirFunction = builder.createFunction("blueprint." + blueprint.name);
            auto& entryBlock = builder.appendBlock(mirFunction, "entry");

            if (!blueprint.modifiers.empty())
            {
                appendDetail(builder, entryBlock, joinList(blueprint.modifiers, "modifiers:"));
            }
            if (blueprint.isPacked)
            {
                appendDetail(builder, entryBlock, "attr packed");
            }
            if (blueprint.alignmentBytes.has_value())
            {
                appendDetail(builder, entryBlock, "aligned " + std::to_string(*blueprint.alignmentBytes));
            }

            for (const auto& field : blueprint.fields)
            {
                std::ostringstream stream;
                stream << "field " << canonicalTypeText(field.type) << ' ' << field.name;
                if (field.isLive)
                {
                    stream << " [live]";
                }
                if (field.bitWidth.has_value())
                {
                    stream << " bits=" << *field.bitWidth;
                }
                if (field.alignmentBytes.has_value())
                {
                    stream << " align=" << *field.alignmentBytes;
                }
                appendDetail(builder, entryBlock, stream.str());
            }

            builder.appendInstruction(entryBlock, InstructionKind::Return).detail = "blueprint";
        }

        return module;
    }
} // namespace bolt::mir

