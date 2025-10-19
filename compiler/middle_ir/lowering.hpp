#pragma once

#include "module.hpp"
#include "../high_level_ir/module.hpp"

namespace bolt::mir
{
    Module lowerFromHir(const hir::Module& hirModule);
}
