#pragma once

#include <d3/patches.hpp>
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
    HOOK_DEFINE_TRAMPOLINE(GfxGetDesiredDisplayModeHook) {
        // @ DisplayMode *__struct_ptr GfxGetDesiredDisplayMode()
        static auto Callback() -> DisplayMode {
            PRINT(
                "GFXNVN globals: dev=%ux%u reduced=%ux%u",
                g_ptGfxNVNGlobals->dwDeviceWidth,
                g_ptGfxNVNGlobals->dwDeviceHeight,
                g_ptGfxNVNGlobals->dwReducedDeviceWidth,
                g_ptGfxNVNGlobals->dwReducedDeviceHeight
            );
            if (!global_config.resolution_hack.active)
                return Orig();

            if (g_ptGfxNVNGlobals != nullptr) {
                const u32 outW      = global_config.resolution_hack.OutputWidthPx();
                const u32 outH      = global_config.resolution_hack.OutputHeightPx();
                const u32 handheldH = global_config.resolution_hack.OutputHandheldHeightPx();
                const u32 fallbackW = handheldH != 0 ? global_config.resolution_hack.WidthForHeight(handheldH) : 2048u;
                const u32 fallbackH = handheldH != 0 ? handheldH : 1152u;

                g_ptGfxNVNGlobals->dwDeviceWidth         = outW;
                g_ptGfxNVNGlobals->dwDeviceHeight        = outH;
                g_ptGfxNVNGlobals->dwReducedDeviceWidth  = fallbackW;
                g_ptGfxNVNGlobals->dwReducedDeviceHeight = fallbackH;
            }
            PRINT(
                "POST GFXNVN globals: dev=%ux%u reduced=%ux%u",
                g_ptGfxNVNGlobals->dwDeviceWidth,
                g_ptGfxNVNGlobals->dwDeviceHeight,
                g_ptGfxNVNGlobals->dwReducedDeviceWidth,
                g_ptGfxNVNGlobals->dwReducedDeviceHeight
            );

            auto result = Orig();

            const auto &extra = global_config.resolution_hack.extra;
            if (extra.window_left >= 0)
                result.nWinLeft = extra.window_left;
            if (extra.window_top >= 0)
                result.nWinTop = extra.window_top;
            if (extra.window_width > 0)
                result.nWinWidth = extra.window_width;
            if (extra.window_height > 0)
                result.nWinHeight = extra.window_height;
            if (extra.ui_opt_width > 0)
                result.dwUIOptWidth = static_cast<u32>(extra.ui_opt_width);
            if (extra.ui_opt_height > 0)
                result.dwUIOptHeight = static_cast<u32>(extra.ui_opt_height);
            bool refresh_aspect = false;
            if (extra.render_width > 0) {
                result.dwWidth = static_cast<u32>(extra.render_width);
                refresh_aspect = true;
            }
            if (extra.render_height > 0) {
                result.dwHeight = static_cast<u32>(extra.render_height);
                refresh_aspect  = true;
            }
            if (extra.refresh_rate > 0)
                result.nRefreshRate = extra.refresh_rate;
            if (extra.bit_depth > 0)
                result.dwBitDepth = static_cast<u32>(extra.bit_depth);
            if (extra.msaa_level >= 0)
                result.dwMSAALevel = static_cast<u32>(extra.msaa_level);
            if (refresh_aspect && result.dwHeight != 0u)
                result.flAspectRatio = static_cast<float>(result.dwWidth) / static_cast<float>(result.dwHeight);

            PatchRenderTargetCurrentResolutionScale(
                global_config.resolution_hack.active ? global_config.resolution_hack.output_handheld_scale : 0.0f
            );

            return result;
        }
    };

    HOOK_DEFINE_TRAMPOLINE(GfxWindowChangeDisplayModeHook) {
        // @ void __fastcall GfxWindowChangeDisplayMode(const DisplayMode *tMode)
        static void Callback(DisplayMode *tMode) { Orig(tMode); }
    };

    inline void SetupResolutionHooks() {
        if (!global_config.resolution_hack.active)
            return;

        GfxGetDesiredDisplayModeHook::InstallAtFuncPtr(GfxGetDesiredDisplayMode);
        // GfxWindowChangeDisplayModeHook::InstallAtOffset(0x29B0E0);

        nvn::NVNTexInfoCreateHook::InstallAtOffset(0xE6B20);

        if (ClampTexturesEnabled())
            nvn::TexDefCreateCallHook::InstallAtOffset(0x28365C);
    }
}  // namespace d3
