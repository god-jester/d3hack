#pragma once

#include <d3/patches.hpp>
#include <d3/types/gfx.hpp>
#include <d3/_util.hpp>
#include "lib/hook/replace.hpp"
#include <hook/trampoline.hpp>
#include "../../config.hpp"
#include "d3/setting.hpp"
#include <symbols/common.hpp>
#include "nvn.hpp"

namespace d3 {
    extern float g_rt_scale;
    inline auto ClampTexturesEnabled() -> bool {
        return global_config.resolution_hack.active && global_config.resolution_hack.ClampTexturesEnabled();
    }
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
            // PRINT(
            //     "POST GFXNVN globals: dev=%ux%u reduced=%ux%u",
            //     g_ptGfxNVNGlobals->dwDeviceWidth,
            //     g_ptGfxNVNGlobals->dwDeviceHeight,
            //     g_ptGfxNVNGlobals->dwReducedDeviceWidth,
            //     g_ptGfxNVNGlobals->dwReducedDeviceHeight
            // );

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
            if (extra.refresh_rate > 0)
                result.nRefreshRate = extra.refresh_rate;
            if (extra.bit_depth > 0)
                result.dwBitDepth = static_cast<u32>(extra.bit_depth);
            if (extra.msaa_level >= 0)
                result.dwMSAALevel = static_cast<u32>(extra.msaa_level);
            bool refresh_aspect = false;
            if (extra.render_width > 0) {
                result.dwWidth = static_cast<u32>(extra.render_width);
                refresh_aspect = true;
            }
            if (extra.render_height > 0) {
                result.dwHeight = static_cast<u32>(extra.render_height);
                refresh_aspect  = true;
            }
            if (refresh_aspect && result.dwHeight != 0u)
                result.flAspectRatio = static_cast<float>(result.dwWidth) / static_cast<float>(result.dwHeight);

            return result;
        }
    };

    HOOK_DEFINE_TRAMPOLINE(GfxWindowChangeDisplayModeHook) {
        // @ void __fastcall GfxWindowChangeDisplayMode(const DisplayMode *tMode)
        static void Callback(DisplayMode *tMode) { Orig(tMode); }
    };

    HOOK_DEFINE_REPLACE(GetRenderTargetCurrentResolutionHook) {
        // @ 0x0EBAC0: void __fastcall GFXNX64NVN::GetRenderTargetCurrentResolution(GFXNX64NVN *this, NVNTexInfo *pTexInfo, uint32 *width, uint32 *height)
        static void Callback(void * /*self*/, NVNTexInfo *pTexInfo, u32 *width, u32 *height) {
            const u32  tex_w     = pTexInfo->dwPixelWidth;
            const u32  tex_h     = pTexInfo->dwPixelHeight;
            const u32  dev_w     = g_ptGfxNVNGlobals->dwDeviceWidth;
            const u32  dev_h     = g_ptGfxNVNGlobals->dwDeviceHeight;
            const bool is_docked = g_ptGfxNVNGlobals->bIsDocked != 0;

            if ((tex_w == dev_w && tex_h == dev_h) ||
                (tex_w == (dev_w >> 1) && tex_h == (dev_h >> 1))) {
                if (is_docked) {
                    *width  = tex_w;
                    *height = tex_h;
                    return;
                }
            } else if (tex_w == (dev_w >> 2) && tex_h == (dev_h >> 2) && !is_docked) {
                // fall through to scale path
            } else {
                *width  = tex_w;
                *height = tex_h;
                return;
            }

            const float scale = (g_rt_scale >= 0.4f) ? std::clamp(g_rt_scale, 0.4f, 1.0f) : 0.8f;

            *width       = static_cast<u32>(static_cast<float>(tex_w) * scale);
            *height      = static_cast<u32>(static_cast<float>(tex_h) * scale);
        }
    };

    inline void SetupResolutionHooks() {
        if (!global_config.resolution_hack.active)
            return;

        GfxGetDesiredDisplayModeHook::InstallAtFuncPtr(GfxGetDesiredDisplayMode);
        // GfxWindowChangeDisplayModeHook::InstallAtOffset(0x29B0E0);
        GetRenderTargetCurrentResolutionHook::InstallAtOffset(0x0EBAC0);

        nvn::NVNTexInfoCreateHook::InstallAtOffset(0xE6B20);

        if (ClampTexturesEnabled())
            nvn::TexDefCreateCallHook::InstallAtOffset(0x28365C);
    }
}  // namespace d3
