#pragma once

#include "lib/hook/inline.hpp"
#include "lib/hook/trampoline.hpp"
#include "d3/_util.hpp"
#include "d3/types/common.hpp"

namespace d3 {
    namespace {
        constexpr uint32 kInternalClampDim = 2048;
        constexpr SNO    kSwapchainDetachDepthSno = -6115;  // Empirically: clamped swapchain depth (avoid inventory depth bugs).
        constexpr uint32 kCompareUnknown          = 0;
        constexpr uint32 kCompareAlways           = 8;
        constexpr uint32 kRENDERLAYER_OPAQUE      = 5;

        struct ResolutionRTView {
            SNO   snoRTTex;
            int32 eCubeMapFace;
        };

        struct ResolutionRenderParams {
            uint32 eCullMode;
            uint32 fZBufferWriteEnable;
            uint32 eZBufferCompareFunc;
            float  flZBias;
            float  flZSlopeScaleBias;
            uint32 fStencilBufferEnable;
            uint32 eStencilBufferCompareFunc;
            int32  nStencilBufferRefValue;
            uint32 eStencilOpPass;
            uint32 eStencilOpFail;
            uint32 eStencilOpZFail;
            uint32 fAlphaTestEnable;
            uint32 eAlphaTestCompareFunc;
            uint8  bAlphaTestRefValue;
            uint8  pad_alpha_ref[3];
            uint32 fAlphaMaskEnable;
            uint32 fFogEnable;
            uint32 eFillMode;
            uint32 fColorWriteEnable;
            uint32 fAlphaWriteEnable;
            uint32 fAlphaBlendEnable;
            uint32 eBlendOp;
            uint32 eSrcBlendFactor;
            uint32 eDestBlendFactor;
            uint32 rgbaTexFactor;
        };

        struct ResolutionRenderPass {
            uint32                 eRenderLayer;
            uint32                 eVertexPreprocessStep;
            ResolutionRenderParams tRenderParams;
        };

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

        inline bool GetResolutionRT0(ResolutionRTView &out) {
            if (!g_ptGfxData)
                return false;
            memcpy(&out, &g_ptGfxData->workerData[0].tCurrentRTs[0], sizeof(out));
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

        struct SwapchainDepthGateState {
            uint32 serial;
            uint32 consumed;
            uint32 layer;
            uint32 z_write;
            uint32 z_cmp;
            uint32 stencil;
            bool   allow_detach;
        };

        static SwapchainDepthGateState s_swapchain_depth_gate = {};

        inline bool PassAllowsSwapchainDepthDetach(uint32 layer, uint32 z_write, uint32 z_cmp, uint32 stencil) {
            if (z_write == 0 || stencil != 0)
                return false;
            return (z_cmp == 4 && layer == kRENDERLAYER_OPAQUE);
        }

        inline void UpdateSwapchainDepthGate(uint32 layer, uint32 z_write, uint32 z_cmp, uint32 stencil) {
            ++s_swapchain_depth_gate.serial;
            s_swapchain_depth_gate.layer        = layer;
            s_swapchain_depth_gate.z_write      = z_write;
            s_swapchain_depth_gate.z_cmp        = z_cmp;
            s_swapchain_depth_gate.stencil      = stencil;
            s_swapchain_depth_gate.allow_detach = PassAllowsSwapchainDepthDetach(layer, z_write, z_cmp, stencil);
        }

        inline void ClearSwapchainDepthGate() {
            s_swapchain_depth_gate.allow_detach = false;
        }

        inline bool ConsumeSwapchainDepthGate() {
            if (!s_swapchain_depth_gate.allow_detach)
                return false;
            // if (s_swapchain_depth_gate.serial == s_swapchain_depth_gate.consumed)
            //     return false;
            s_swapchain_depth_gate.consumed = s_swapchain_depth_gate.serial;
            return true;
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
            // PRINT_EXPR("PostFXInitClampDims called");
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
                PRINT("PostFXGfxInit clamp: %ux%u -> %ux%u (scale %.3f)", width, height, new_width, new_height, scale);
            }
        };
    };

    HOOK_DEFINE_TRAMPOLINE(GfxSetDepthTargetSwapchainFix) {
        // @ 0x29C4E0: GfxSetDepthTarget(SNO depth)
        // Detach the internal clamped depth target when rendering directly to the swapchain.
        // This prevents the 2048x1152 depth attachment from clamping the swapchain render area at 1152p+ output.
        static void Callback(const SNO snoDepth) {
            // PRINT_EXPR("GfxSetDepthTargetSwapchainFix called");
            if (!global_config.resolution_hack.active || !global_config.resolution_hack.clamp_textures_2048 || !g_ptGfxData) {
                Orig(snoDepth);
                return;
            }

            ResolutionRTView rt0 {};
            if (!GetResolutionRT0(rt0) || rt0.snoRTTex != 0) {
                Orig(snoDepth);
                return;
            }

            uint32 out_w = 0;
            uint32 out_h = 0;
            if (!GetSwapchainDims(out_w, out_h)) {
                Orig(snoDepth);
                return;
            }

            uint32 int_w = 0;
            uint32 int_h = 0;
            float  scale = 1.0f;
            if (!GetInternalDims(out_w, out_h, int_w, int_h, scale)) {
                Orig(snoDepth);
                return;
            }

            // Detach only when the current swapchain pass does not use depth/stencil.
            if (ConsumeSwapchainDepthGate()) {
                if (global_config.debug.active) {
                    struct DepthLogEntry {
                        SNO    depth;
                        uint32 count;
                    };
                    static DepthLogEntry s_entries[64] = {};
                    static uint32        s_num_entries = 0;

                    uint32 *count_ptr = nullptr;
                    for (uint32 i = 0; i < s_num_entries; ++i) {
                        if (s_entries[i].depth == snoDepth) {
                            count_ptr = &s_entries[i].count;
                            break;
                        }
                    }
                    if (!count_ptr && s_num_entries < 64) {
                        s_entries[s_num_entries].depth = snoDepth;
                        s_entries[s_num_entries].count = 0;
                        count_ptr                      = &s_entries[s_num_entries].count;
                        ++s_num_entries;
                    }

                    if (count_ptr && *count_ptr < 32) {
                        PRINT(
                            "Swapchain depth detach (state gate): depth=%d out=%ux%u int=%ux%u",
                            snoDepth,
                            out_w,
                            out_h,
                            int_w,
                            int_h
                        );
                        ++(*count_ptr);
                    }
                }
                Orig(-1);
                return;
            }

            Orig(snoDepth);
        }
    };

    HOOK_DEFINE_TRAMPOLINE(SetStateBlockStatesSwapchainLog) {
        // @ 0x2A6E10: sSetStateBlockStates(RenderPass const*)
        // Log depth/stencil state when swapchain is bound and internal clamp is active.
        static void Callback(const ResolutionRenderPass *tPass) {
            if (!g_ptGfxData) {
                Orig(tPass);
                return;
            }

            ResolutionRTView rt0 {};
            if (!GetResolutionRT0(rt0)) {  //} || rt0.snoRTTex != 0) {
                ClearSwapchainDepthGate();
                Orig(tPass);
                return;
            }

            uint32 out_w = 0;
            uint32 out_h = 0;
            if (!GetSwapchainDims(out_w, out_h)) {
                ClearSwapchainDepthGate();
                Orig(tPass);
                return;
            }

            uint32 int_w = 0;
            uint32 int_h = 0;
            float  scale = 1.0f;
            if (!GetInternalDims(out_w, out_h, int_w, int_h, scale)) {
                ClearSwapchainDepthGate();
                Orig(tPass);
                return;
            }

            if (tPass) {
                const uint32 render_layer = tPass->eRenderLayer;
                const uint32 z_write      = tPass->tRenderParams.fZBufferWriteEnable ? 1u : 0u;
                const uint32 z_cmp        = tPass->tRenderParams.eZBufferCompareFunc;
                const uint32 stencil      = tPass->tRenderParams.fStencilBufferEnable ? 1u : 0u;
                const SNO    depth        = g_ptGfxData->workerData[0].snoCurrentDepthTarget;

                UpdateSwapchainDepthGate(render_layer, z_write, z_cmp, stencil);

                struct StateLogEntry {
                    uint32 layer;
                    uint32 z_write;
                    uint32 z_cmp;
                    uint32 stencil;
                    SNO    depth;
                    uint32 count;
                };

                static StateLogEntry s_entries[64] = {};
                static uint32        s_num_entries = 0;

                StateLogEntry *entry = nullptr;
                for (uint32 i = 0; i < s_num_entries; ++i) {
                    if (s_entries[i].layer == render_layer && s_entries[i].z_write == z_write && s_entries[i].z_cmp == z_cmp && s_entries[i].stencil == stencil && s_entries[i].depth == depth) {
                        entry = &s_entries[i];
                        break;
                    }
                }
                if (!entry && s_num_entries < 64) {
                    entry          = &s_entries[s_num_entries++];
                    entry->layer   = render_layer;
                    entry->z_write = z_write;
                    entry->z_cmp   = z_cmp;
                    entry->stencil = stencil;
                    entry->depth   = depth;
                    entry->count   = 0;
                }

                if (global_config.debug.active && entry && entry->count < 2) {
                    PRINT(
                        "Swapchain pass state: layer=%u z_write=%u z_cmp=%u stencil=%u depth=%d out=%ux%u int=%ux%u",
                        render_layer,
                        z_write,
                        z_cmp,
                        stencil,
                        depth,
                        out_w,
                        out_h,
                        int_w,
                        int_h
                    );
                    ++entry->count;
                }
            }

            Orig(tPass);
        }
    };

    HOOK_DEFINE_TRAMPOLINE(GfxViewportSetSwapchainDepthGate) {
        // @ 0x0F0280: void __fastcall GFXNX64NVN::ViewportSet(GFXNX64NVN*, IRect2D*)
        using GFXNX64NVNHandle = uintptr_t;
        static void Callback(GFXNX64NVNHandle *self, IRect2D *rectViewport) {
            if (!rectViewport) {
                Orig(self, rectViewport);
                return;
            }

            if (!global_config.resolution_hack.active || !global_config.resolution_hack.clamp_textures_2048 || !g_ptGfxData) {
                Orig(self, rectViewport);
                return;
            }

            ResolutionRTView rt0 {};
            if (!GetResolutionRT0(rt0) || rt0.snoRTTex != 0) {
                Orig(self, rectViewport);
                return;
            }

            uint32 out_w = 0;
            uint32 out_h = 0;
            if (!GetSwapchainDims(out_w, out_h)) {
                Orig(self, rectViewport);
                return;
            }

            uint32 int_w = 0;
            uint32 int_h = 0;
            float  scale = 1.0f;
            if (!GetInternalDims(out_w, out_h, int_w, int_h, scale)) {
                Orig(self, rectViewport);
                return;
            }

            const int32 raw_vp_w = rectViewport->right - rectViewport->left;
            const int32 raw_vp_h = rectViewport->bottom - rectViewport->top;
            if (raw_vp_w > 0 && raw_vp_h > 0) {
                const uint32 vp_w = static_cast<uint32>(raw_vp_w);
                const uint32 vp_h = static_cast<uint32>(raw_vp_h);

                // Only detach for the full swapchain viewport; partial viewports are used by UI sub-views.
                if (vp_w == out_w && vp_h == out_h) {
                    const SNO current_depth = g_ptGfxData->workerData[0].snoCurrentDepthTarget;
                    if (ConsumeSwapchainDepthGate() && current_depth != -1) {
                        if (global_config.debug.active) {
                            struct DepthLogEntry {
                                SNO    depth;
                                uint32 count;
                            };
                            static DepthLogEntry s_entries[64] = {};
                            static uint32        s_num_entries = 0;

                            uint32 *count_ptr = nullptr;
                            for (uint32 i = 0; i < s_num_entries; ++i) {
                                if (s_entries[i].depth == current_depth) {
                                    count_ptr = &s_entries[i].count;
                                    break;
                                }
                            }
                            if (!count_ptr && s_num_entries < 64) {
                                s_entries[s_num_entries].depth = current_depth;
                                s_entries[s_num_entries].count = 0;
                                count_ptr                      = &s_entries[s_num_entries].count;
                                ++s_num_entries;
                            }

                            if (count_ptr && *count_ptr < 32) {
                                PRINT(
                                    "Swapchain depth detach (vp state gate): depth=%d vp=%ux%u out=%ux%u int=%ux%u",
                                    current_depth,
                                    vp_w,
                                    vp_h,
                                    out_w,
                                    out_h,
                                    int_w,
                                    int_h
                                );
                                ++(*count_ptr);
                            }
                        }
                        GfxSetDepthTargetSwapchainFix::Orig(-1);
                    }
                }
            }

            Orig(self, rectViewport);
        }
    };

    HOOK_DEFINE_TRAMPOLINE(CGameVariableResInitializeForRWindowHook) {
        // @ 0x03CB90: CGameVariableResInitializeForRWindow()
        static VariableResRWindowData *Callback() {
            auto *result            = Orig();
            auto  orig_flMinPercent = result->flMinPercent;
            // if (!global_config.initialized || global_config.defaults_only)
            //     return result;

            if (result && global_config.resolution_hack.active)
                result->flMinPercent = global_config.resolution_hack.min_res_scale * 0.01f;

            PRINT_EXPR("%f -> %f", orig_flMinPercent, result->flMinPercent);
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

        if (global_config.debug.active)
            SetStateBlockStatesSwapchainLog::InstallAtOffset(0x2A6E10);

        GfxSetDepthTargetSwapchainFix::InstallAtOffset(0x29C4E0);
        GfxViewportSetSwapchainDepthGate::InstallAtOffset(0x0F0280);
    }
}  // namespace d3
