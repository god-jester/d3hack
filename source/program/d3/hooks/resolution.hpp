#pragma once

#include "nvn.hpp"

namespace d3 {
    namespace {
        inline auto ClampTexturesEnabled() -> bool {
            return global_config.resolution_hack.active && global_config.resolution_hack.ClampTexturesEnabled();
        }
    }  // namespace

    inline void SetupResolutionHooks() {
        if (!global_config.resolution_hack.active)
            return;

        nvn::NVNTexInfoCreateHook::InstallAtOffset(0xE6B20);

        if (ClampTexturesEnabled())
            nvn::TexDefCreateCallHook::InstallAtOffset(0x28365C);
    }
}  // namespace d3
