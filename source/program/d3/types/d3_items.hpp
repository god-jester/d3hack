#pragma once

#include "d3/types/hexrays.hpp"

#include <array>

namespace D3::Items {
    struct ALIGNED(8) Mail : std::array<_BYTE, 0x68> {};
    struct SavedItem : std::array<_BYTE, 0x50> {};
    struct Generator : std::array<_BYTE, 0xF0> {};
}  // namespace D3::Items
