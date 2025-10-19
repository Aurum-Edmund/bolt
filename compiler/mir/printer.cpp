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
        stream << "module " << module.name << "\n";

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
