#pragma once

#include <d3/types/gfx.hpp>
#include <d3/_util.hpp>
#include <hook/trampoline.hpp>
#include "../../config.hpp"
#include "d3/setting.hpp"
#include <symbols/common.hpp>
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
            const auto &extra = global_config.resolution_hack.extra;
            if (extra.window_left >= 0)
                tMode->nWinLeft = extra.window_left;
            if (extra.window_top >= 0)
                tMode->nWinTop = extra.window_top;
            if (extra.window_width > 0)
                tMode->nWinWidth = extra.window_width;
            if (extra.window_height > 0)
                tMode->nWinHeight = extra.window_height;
            if (extra.ui_opt_width > 0)
                tMode->dwUIOptWidth = static_cast<u32>(extra.ui_opt_width);
            if (extra.ui_opt_height > 0)
                tMode->dwUIOptHeight = static_cast<u32>(extra.ui_opt_height);
            bool refresh_aspect = false;
            if (extra.render_width > 0) {
                tMode->dwWidth = static_cast<u32>(extra.render_width);
                refresh_aspect = true;
            }
            if (extra.render_height > 0) {
                tMode->dwHeight = static_cast<u32>(extra.render_height);
                refresh_aspect  = true;
            }
            if (extra.refresh_rate > 0)
                tMode->nRefreshRate = extra.refresh_rate;
            if (extra.bit_depth > 0)
                tMode->dwBitDepth = static_cast<u32>(extra.bit_depth);
            if (extra.msaa_level >= 0)
                tMode->dwMSAALevel = static_cast<u32>(extra.msaa_level);
            if (refresh_aspect && tMode->dwHeight != 0u)
                tMode->flAspectRatio = static_cast<float>(tMode->dwWidth) / static_cast<float>(tMode->dwHeight);

            // force prefs-changed/reset by setting bit 6 (GfxHasPrefsChanged)
            // PRINT_EXPR("PRE : %u %u %u %u", g_ptGfxData->tCurrentMode.dwWidth, g_ptGfxData->tCurrentMode.dwHeight, g_ptGfxData->tCurrentMode.dwMSAALevel, g_ptGfxData->dwFlags);
            // PRINT_EXPR("%u %u %u %u", tMode->dwWidth, tMode->dwHeight, tMode->dwMSAALevel, tMode->dwFlags);

            Orig(tMode);

            if (g_ptGfxData != nullptr) {
                // g_ptGfxData->dwFlags |= 0x40u;
                // g_ptGfxData->tCurrentMode = *tMode;
                // PRINT_EXPR("POST: %u %u %u %u", g_ptGfxData->tCurrentMode.dwWidth, g_ptGfxData->tCurrentMode.dwHeight, g_ptGfxData->tCurrentMode.dwMSAALevel, g_ptGfxData->dwFlags);
            }

            PRINT_EXPR("%d", g_ptGfxNVNGlobals->bIsDocked);
            Orig(tMode);
            PRINT_EXPR("%d", scheck_gfx_ready(true));
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
