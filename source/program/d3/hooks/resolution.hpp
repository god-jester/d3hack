#pragma once

#include <hook/trampoline.hpp>
#include "../../config.hpp"
#include "../../setting.hpp"
#include "nvn.hpp"

namespace d3 {
    namespace {
        inline auto ClampTexturesEnabled() -> bool {
            return global_config.resolution_hack.active && global_config.resolution_hack.ClampTexturesEnabled();
        }
    }  // namespace
    HOOK_DEFINE_TRAMPOLINE(GfxWindowChangeDisplayModeHook) {
        // @ void __fastcall GfxWindowChangeDisplayMode(const DisplayMode *tMode)
        static void Callback(DisplayMode *tMode) {
            if (tMode == nullptr) {
                PRINT_LINE("GfxWindowChangeDisplayMode X0=null");
                Orig(tMode);
                return;
            }
            PRINT("GfxWindowChangeDisplayMode X0=%p", tMode)
            PRINT(
                "  dwFlags=0x%x dwWindowMode=%d nWinLeft=%d nWinTop=%d nWinWidth=%d nWinHeight=%d",
                tMode->dwFlags, tMode->dwWindowMode, tMode->nWinLeft, tMode->nWinTop, tMode->nWinWidth, tMode->nWinHeight
            )
            PRINT(
                "  dwUIOptWidth=%u dwUIOptHeight=%u dwWidth=%u dwHeight=%u nRefreshRate=%d dwBitDepth=%u dwMSAALevel=%u",
                tMode->dwUIOptWidth, tMode->dwUIOptHeight, tMode->dwWidth, tMode->dwHeight, tMode->nRefreshRate, tMode->dwBitDepth, tMode->dwMSAALevel
            )
            PRINT("  flAspectRatio=%f", tMode->flAspectRatio)
            tMode->dwUIOptWidth = 1920, tMode->dwUIOptHeight = 1080,
            tMode->dwMSAALevel = 4;

            PRINT("POST: %u %u %u", tMode->dwUIOptWidth, tMode->dwUIOptHeight, tMode->dwMSAALevel);
            Orig(tMode);
        }
    };

    inline void SetupResolutionHooks() {
        if (!global_config.resolution_hack.active)
            return;

        GfxWindowChangeDisplayModeHook::InstallAtOffset(0x29B0E0);

        nvn::NVNTexInfoCreateHook::InstallAtOffset(0xE6B20);

        if (ClampTexturesEnabled())
            nvn::TexDefCreateCallHook::InstallAtOffset(0x28365C);
    }
}  // namespace d3
