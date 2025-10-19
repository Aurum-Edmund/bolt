#pragma once

#include "module.hpp"

#include <iosfwd>

namespace bolt::mir
{
    void print(const Module& module, std::ostream& stream);
}
