#pragma once

#include "lib/hook/inline.hpp"
#include "lib/hook/trampoline.hpp"
#include "d3/_util.hpp"
#include "d3/types/common.hpp"

namespace d3 {
    namespace {
        constexpr uint32 kInternalClampDim = 2048;

        inline float CalcRenderScale(uint32 width, uint32 height) {
            if (width == 0 || height == 0)
                return 1.0f;
            float scale = 1.0f;
            if (width > kInternalClampDim)
                scale = static_cast<float>(kInternalClampDim) / static_cast<float>(width);
            if (height > kInternalClampDim) {
                const float hscale = static_cast<float>(kInternalClampDim) / static_cast<float>(height);
                if (hscale < scale)
                    scale = hscale;
            }
            if (scale > 1.0f)
                scale = 1.0f;
            if (scale <= 0.0f)
                scale = 1.0f;
            return scale;
        }

        inline uint32 ScaleDim(uint32 dim, float scale) {
            auto scaled = static_cast<uint32>(dim * scale);
            scaled &= ~1u;  // keep even
            if (scaled < 2)
                scaled = 2;
            return scaled;
        }

        inline bool GetSwapchainDims(uint32 &out_w, uint32 &out_h) {
            if (!g_ptGfxData)
                return false;
            const int32 w = g_ptGfxData->tCurrentMode.nWinWidth;
            const int32 h = g_ptGfxData->tCurrentMode.nWinHeight;
            if (w <= 0 || h <= 0)
                return false;
            out_w = static_cast<uint32>(w);
            out_h = static_cast<uint32>(h);
            return true;
        }

        inline bool GetInternalDims(uint32 out_w, uint32 out_h, uint32 &int_w, uint32 &int_h, float &scale) {
            scale = CalcRenderScale(out_w, out_h);
            if (scale >= 1.0f)
                return false;
            int_w = ScaleDim(out_w, scale);
            int_h = ScaleDim(out_h, scale);
            return (int_w > 0 && int_h > 0);
        }
    }  // namespace

    // Resolution hack notes (1080p+ output):
    // - The 2048x1152 UI hard-clip only showed up once output exceeded the internal clamp
    //   (1152p+). It was caused by a smaller depth attachment clamping the swapchain render
    //   area, so we detach depth only when swapchain output is larger than internal.
    // - 1080p output does not hit this path; depth detachment stays inactive.
    // - sRenderDrawDefault builds rectUIViewport from GetPixelDimensions() (via g_ptGfxData),
    //   while GfxSetRenderTargets sets the swapchain viewport to g_ptGfxData->dwWidth/dwHeight.
    //   If those remain the output size, the UI basis itself is not the clip source; the
    //   call chain confirms the viewport comes from the current display mode.
    // - UI basis/NDC/UIC conversion hooks and stencil toggles can rescale UI but do not remove
    //   the hard clip, so they are not part of the shipping path here.

    HOOK_DEFINE_INLINE(PostFXInitClampDims) {
        // @ 0x12F218: after GetPixelDimensions, before GfxGetMSAALevel.
        // Clamp internal RT dimensions to <= 2048 when output > 2048 to avoid NVN/Metal crashes.
        static void Callback(exl::hook::InlineCtx *ctx) {
            if (!g_ptGfxData)
                return;
            if (!global_config.resolution_hack.active || !global_config.resolution_hack.clamp_textures_2048)
                return;

            // PostFXGfxInit packs dimensions into X22 (high32=height, low32=width) and also stores height in X23.
            // Preserve the packed value when clamping so downstream half/quarter-dimension calculations remain valid.
            const uint64 packed = ctx->X[22];
            const auto   width  = static_cast<uint32>(packed & 0xFFFFFFFFu);
            const auto   height = static_cast<uint32>(ctx->X[23]);
            const float  scale  = CalcRenderScale(width, height);

            if (scale >= 1.0f)
                return;

            const auto new_width  = ScaleDim(width, scale);
            const auto new_height = ScaleDim(height, scale);
            ctx->X[22]            = (static_cast<uint64>(new_height) << 32) | static_cast<uint64>(new_width);
            ctx->X[23]            = new_height;

            auto *rwin = reinterpret_cast<RWindow *>(ctx->X[19]);
            if (rwin && rwin->m_ptVariableResRWindowData) {
                auto *var = rwin->m_ptVariableResRWindowData;
                if (var->flMaxPercent > scale)
                    var->flMaxPercent = scale;
                if (var->flMinPercent > scale)
                    var->flMinPercent = scale;
                var->flPercent = scale;
            }

            if (global_config.debug.active) {
                PRINT("PostFXGfxInit clamp: %ux%u -> %ux%u (scale %.3f)", width, height, new_width, new_height, scale)
            }
        };
    };

    HOOK_DEFINE_TRAMPOLINE(GfxSetRenderAndDepthTargetSwapchainFix) {
        // @ 0x29C670: GfxSetRenderAndDepthTarget(SNO color, SNO depth, CubeMapFace face)
        // Depth attachment clamp (swapchain) fix for 1152p+ output.
        static void Callback(SNO snoColor, SNO snoDepth, CubeMapFace face) {
            if (global_config.resolution_hack.active && global_config.resolution_hack.clamp_textures_2048
                && snoColor == 0 && snoDepth != -1) {
                uint32 out_w = 0;
                uint32 out_h = 0;
                if (GetSwapchainDims(out_w, out_h)) {
                    uint32 int_w = 0;
                    uint32 int_h = 0;
                    float  scale = 1.0f;
                    if (GetInternalDims(out_w, out_h, int_w, int_h, scale)) {
                        if (global_config.debug.active) {
                            static uint32 s_logged = 0;
                            if (s_logged < 8) {
                                PRINT(
                                    "Detach depth on swapchain: depth=%d out=%ux%u int=%ux%u",
                                    snoDepth,
                                    out_w,
                                    out_h,
                                    int_w,
                                    int_h
                                )
                                ++s_logged;
                            }
                        }
                        Orig(snoColor, -1, face);
                        return;
                    }
                }
            }
            Orig(snoColor, snoDepth, face);
        }
    };

    HOOK_DEFINE_TRAMPOLINE(GfxSetDepthTargetSwapchainFix) {
        // @ 0x29C4E0: GfxSetDepthTarget(SNO depth)
        // Fallback: if depth is set while swapchain is bound, detach to prevent clamp.
        static void Callback(const SNO snoDepth) {
            if (global_config.resolution_hack.active && global_config.resolution_hack.clamp_textures_2048
                && snoDepth != -1 && g_ptGfxData) {
                struct RenderTargetView {
                    SNO   snoRTTex;
                    int32 eCubeMapFace;
                };

                RenderTargetView rt0 {};
                memcpy(&rt0, &g_ptGfxData->workerData[0].tCurrentRTs[0], sizeof(rt0));
                if (rt0.snoRTTex == 0) {
                    uint32 out_w = 0;
                    uint32 out_h = 0;
                    if (GetSwapchainDims(out_w, out_h)) {
                        uint32 int_w = 0;
                        uint32 int_h = 0;
                        float  scale = 1.0f;
                        if (GetInternalDims(out_w, out_h, int_w, int_h, scale)) {
                            if (global_config.debug.active) {
                                static uint32 s_logged = 0;
                                if (s_logged < 8) {
                                    PRINT(
                                        "Detach depth (set depth) on swapchain: depth=%d out=%ux%u int=%ux%u",
                                        snoDepth,
                                        out_w,
                                        out_h,
                                        int_w,
                                        int_h
                                    )
                                    ++s_logged;
                                }
                            }
                            Orig(-1);
                            return;
                        }
                    }
                }
            }
            Orig(snoDepth);
        }
    };

    HOOK_DEFINE_TRAMPOLINE(CGameVariableResInitializeForRWindowHook) {
        // @ 0x03CB90: CGameVariableResInitializeForRWindow()
        static VariableResRWindowData *Callback() {
            auto *result            = Orig();
            auto  orig_flMinPercent = result->flMinPercent;
            if (!global_config.initialized || global_config.defaults_only)
                return result;

            if (result && global_config.resolution_hack.active)
                result->flMinPercent = global_config.resolution_hack.min_res_scale * 0.01f;

            PRINT("Forced variable res min %f -> %f", orig_flMinPercent, result->flMinPercent)
            return result;
        }
    };

    inline void SetupResolutionHooks() {
        if (!global_config.resolution_hack.active)
            return;

        const uint32 out_w = global_config.resolution_hack.OutputWidthPx();
        const uint32 out_h = global_config.resolution_hack.OutputHeightPx();
        const bool   needs_internal_clamp = (out_w > kInternalClampDim) || (out_h > kInternalClampDim);
        // 1080p output stays <= 2048, so the 1152p+ clip path never triggers; no hooks needed.
        if (!needs_internal_clamp)
            return;

        if (global_config.resolution_hack.clamp_textures_2048)
            PostFXInitClampDims::InstallAtOffset(0x12F218);

        GfxSetDepthTargetSwapchainFix::InstallAtOffset(0x29C4E0);

        if (global_config.debug.active && global_config.debug.enable_debug_flags)
            GfxSetRenderAndDepthTargetSwapchainFix::InstallAtOffset(0x29C670);
    }
}  // namespace d3
