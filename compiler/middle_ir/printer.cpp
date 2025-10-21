#include "printer.hpp"

#include <ostream>

namespace bolt::mir
{
    namespace
    {
        void printIndent(std::ostream& stream, int level)
        {
            for (int i = 0; i < level; ++i)
            {
                stream << "  ";
            }
        }
    } // namespace

    void print(const Module& module, std::ostream& stream)
    {
        stream << "module " << module.moduleName << "\n";
        if (!module.packageName.empty())
        {
            stream << "package " << module.packageName << "\n";
        }
        if (!module.canonicalModulePath.empty())
        {
            stream << "canonical " << module.canonicalModulePath << "\n";
        }
        if (!module.imports.empty())
        {
            printIndent(stream, 1);
            stream << "imports (" << module.imports.size() << ")\n";
            for (const auto& importName : module.imports)
            {
                printIndent(stream, 2);
                stream << importName << "\n";
            }
        }
        if (!module.resolvedImports.empty())
        {
            printIndent(stream, 1);
            stream << "resolvedImports (" << module.resolvedImports.size() << ")\n";
            for (const auto& entry : module.resolvedImports)
            {
                printIndent(stream, 2);
                stream << entry.modulePath;
                if (entry.canonicalModulePath.has_value())
                {
                    stream << " [" << *entry.canonicalModulePath << "]";
                }
                if (entry.filePath.has_value())
                {
                    stream << " -> " << *entry.filePath;
                }
                stream << "\n";
            }
        }

        for (const auto& function : module.functions)
        {
            printIndent(stream, 1);
            stream << "function " << function.name << " {\n";

            for (const auto& block : function.blocks)
            {
                printIndent(stream, 2);
                stream << block.name << " (#" << block.id << ")" << " {\n";

                for (const auto& instruction : block.instructions)
                {
                    printIndent(stream, 3);
                    stream << static_cast<int>(instruction.kind);
                    if (!instruction.detail.empty())
                    {
                        stream << " // " << instruction.detail;
                    }
                    if (!instruction.successors.empty())
                    {
                        stream << " ->";
                        for (std::size_t index = 0; index < instruction.successors.size(); ++index)
                        {
                            stream << (index == 0 ? ' ' : ',');
                            stream << instruction.successors[index];
                        }
                    }
                    stream << "\n";
                }

                printIndent(stream, 2);
                stream << "}\n";
            }

            printIndent(stream, 1);
            stream << "}\n";
        }
    }
} // namespace bolt::mir
