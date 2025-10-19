#pragma once

#include "module.hpp"

#include <cstdint>
#include <string>

namespace bolt::mir
{
    std::string canonicalPrint(const Module& module);
    std::uint64_t canonicalHash(const Module& module);
}
