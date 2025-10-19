#pragma once

#include "module.hpp"
#include "../hir/module.hpp"

namespace bolt::mir
{
    Module lowerFromHir(const hir::Module& hirModule);
}
