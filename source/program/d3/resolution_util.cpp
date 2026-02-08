#include "program/d3/resolution_util.hpp"

#include "program/d3/_util.hpp"

namespace d3 {

    auto MaxResolutionHackOutputTarget() -> u32 {
        constexpr u32 kMaxDefault = 1440u;
        constexpr u32 kMaxDevMem  = 2160u;
        if (g_ptDevMemMode != nullptr && *g_ptDevMemMode != 0u) {
            return kMaxDevMem;
        }
        return kMaxDefault;
    }

}  // namespace d3
