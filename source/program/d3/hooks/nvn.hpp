#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>

#include "lib/hook/inline.hpp"
#include "lib/util/modules.hpp"
#include "../../config.hpp"
#include "nvn/nvn.h"

namespace d3 {
    namespace nvn {
        namespace {
            struct NVNTexInfo {
                u32 eTexFormat;
                u32 dwMSAALevel;
                u32 dwLastFrameUsed;
                u32 eType;
                u32 dwPixelWidth;
                u32 dwPixelHeight;
                u32 dwPixelDepth;
                u32 dwMipLevels;
                u8  tMemory[0x38];
                u32 bTargetUpdated;
                u32 bIsWindowTexture;
            };

            static_assert(offsetof(NVNTexInfo, bIsWindowTexture) == 0x5C);

            inline bool ClampTexturesEnabled() {
                return global_config.resolution_hack.active && global_config.resolution_hack.ClampTexturesEnabled();
            }

            inline bool GetClampDims(u32 &limit_w, u32 &limit_h) {
                if (!ClampTexturesEnabled())
                    return false;
                limit_h = global_config.resolution_hack.ClampTextureHeightPx();
                limit_w = global_config.resolution_hack.ClampTextureWidthPx();
                if (limit_w == 0 || limit_h == 0)
                    return false;
                return true;
            }

            inline bool IsDepthFormat(u32 fmt) {
                switch (fmt) {
                case NVN_FORMAT_STENCIL8:
                case NVN_FORMAT_DEPTH16:
                case NVN_FORMAT_DEPTH24:
                case NVN_FORMAT_DEPTH32F:
                case NVN_FORMAT_DEPTH24_STENCIL8:
                case NVN_FORMAT_DEPTH32F_STENCIL8:
                    return true;
                default:
                    return false;
                }
            }

            inline float LocalCalcRenderScale(u32 width, u32 height, u32 limit_w, u32 limit_h) {
                if (width == 0 || height == 0)
                    return 1.0f;
                float scale = 1.0f;
                if (width > limit_w)
                    scale = static_cast<float>(limit_w) / static_cast<float>(width);
                if (height > limit_h) {
                    const float hscale = static_cast<float>(limit_h) / static_cast<float>(height);
                    if (hscale < scale)
                        scale = hscale;
                }
                if (scale > 1.0f)
                    scale = 1.0f;
                if (scale <= 0.0f)
                    scale = 1.0f;
                return scale;
            }

            inline u32 LocalScaleDim(u32 dim, float scale) {
                auto scaled = static_cast<u32>(dim * scale);
                scaled &= ~1u;  // keep even
                if (scaled < 2)
                    scaled = 2;
                return scaled;
            }

            constexpr uintptr_t kSwapchainTexCreateLrOffsets[] = {
                0xE6988,
                0xE69BC,
                0xE69F0,
            };

            inline bool IsSwapchainTexCreateLR(uintptr_t lr_offset) {
                for (const auto lr : kSwapchainTexCreateLrOffsets) {
                    if (lr_offset == lr || lr_offset + 4 == lr)
                        return true;
                }
                return false;
            }

            struct TexDefCallInfo {
                u32       fmt;
                u32       width;
                u32       height;
                u32       usage;
                u32       flags;
                uintptr_t name_ptr;
                bool      has_name;
                bool      valid;
                char      name[64];
            };

            static TexDefCallInfo s_last_texdef = {};

            inline bool TexDefNameContains(const TexDefCallInfo &info, const char *needle) {
                if (!info.has_name || info.name[0] == '\0' || !needle || needle[0] == '\0')
                    return false;
                return std::strstr(info.name, needle) != nullptr;
            }

            inline bool IsBackbufferDepthName(const TexDefCallInfo &info) {
                return TexDefNameContains(info, "BACKBUFFER_DEPTH") || TexDefNameContains(info, "BACKBUFFER");
            }

            inline bool IsFullResEffectName(const TexDefCallInfo &info) {
                if (TexDefNameContains(info, "BACKBUFFER"))
                    return true;
                if (TexDefNameContains(info, "SCREEN_"))
                    return true;
                return false;
            }

            inline bool IsCriticalDepthName(const TexDefCallInfo &info) {
                if (IsBackbufferDepthName(info))
                    return true;
                if (TexDefNameContains(info, "DEPTH Effect RT"))
                    return true;
                return false;
            }

            inline bool IsClampableDepthName(const TexDefCallInfo &info) {
                if (TexDefNameContains(info, "Shadow"))
                    return true;
                if (TexDefNameContains(info, "DepthTarget"))
                    return true;
                return false;
            }

            inline void CaptureTexDefCreateCall(exl::hook::InlineCtx *ctx) {
                if (!ctx)
                    return;

                TexDefCallInfo info = {};
                info.fmt            = static_cast<u32>(ctx->W[1]);
                info.width          = static_cast<u32>(ctx->W[2]);
                info.height         = static_cast<u32>(ctx->W[3]);
                info.usage          = static_cast<u32>(ctx->W[5]);
                info.flags          = static_cast<u32>(ctx->W[6]);

                const uintptr_t name_ptr = static_cast<uintptr_t>(ctx->X[20]);
                info.name_ptr            = name_ptr;
                info.has_name            = false;
                info.name[0]             = '\0';
                if (name_ptr != 0) {
                    const auto *module = exl::util::TryGetModule(name_ptr);
                    if (module) {
                        const char *name = reinterpret_cast<const char *>(name_ptr);
                        for (u32 i = 0; i + 1 < static_cast<u32>(sizeof(info.name)); ++i) {
                            const char c = name[i];
                            info.name[i] = c;
                            if (c == '\0')
                                break;
                        }
                        info.name[sizeof(info.name) - 1] = '\0';
                        info.has_name                    = true;
                    }
                }

                info.valid    = true;
                s_last_texdef = info;
            }
        }  // namespace

        HOOK_DEFINE_INLINE(TexDefCreateCallHook) {
            // @ 0x0028365C: sTextureDefinitionCreate callsite into TextureCreate.
            static void Callback(exl::hook::InlineCtx *ctx) { CaptureTexDefCreateCall(ctx); }
        };

        HOOK_DEFINE_INLINE(NVNTexInfoCreateHook) {
            // @ 0x00E6B20: NVNTexInfo::CreateNVNTexture / texture creation path
            static void Callback(exl::hook::InlineCtx *ctx) {
                auto *tex = reinterpret_cast<NVNTexInfo *>(ctx->X[0]);
                if (!tex)
                    return;

                const u32  width      = tex->dwPixelWidth;
                const u32  height     = tex->dwPixelHeight;
                const u32  arg2       = static_cast<u32>(ctx->W[2]);
                const u32  arg3       = static_cast<u32>(ctx->W[3]);
                const auto lr         = static_cast<uintptr_t>(ctx->m_Gpr.m_Lr.X);
                const auto lr_offset  = static_cast<uintptr_t>(lr - exl::util::modules::GetTargetStart());
                u32        cur_width  = width;
                u32        cur_height = height;

                const u32 out_w = global_config.resolution_hack.OutputWidthPx();
                const u32 out_h = global_config.resolution_hack.OutputHeightPx();

                if (global_config.resolution_hack.active && out_w != 0 && out_h != 0 && IsSwapchainTexCreateLR(lr_offset)) {
                    if (cur_width != out_w || cur_height != out_h) {
                        tex->dwPixelWidth  = out_w;
                        tex->dwPixelHeight = out_h;
                        cur_width          = out_w;
                        cur_height         = out_h;

                        if (tex->dwMipLevels > 1) {
                            const u32 longest  = std::max(out_w, out_h);
                            const u32 max_mips = 1u + static_cast<u32>(
                                                          std::floor(std::log2(static_cast<double>(longest)))
                                                      );
                            tex->dwMipLevels = std::min(tex->dwMipLevels, max_mips);
                        }
                    }
                    return;
                }

                TexDefCallInfo last           = s_last_texdef;
                const bool     matched_texdef = last.valid && last.width == width && last.height == height && last.fmt == tex->eTexFormat;
                if (matched_texdef)
                    s_last_texdef.valid = false;

                u32 limit_w = 0;
                u32 limit_h = 0;
                if (!GetClampDims(limit_w, limit_h))
                    return;

                const bool depth_target = matched_texdef && (last.flags & 2u) != 0u;
                const bool is_window    = (arg3 != 0);

                const bool depth_fmt        = IsDepthFormat(tex->eTexFormat);
                const bool texdef_depth     = matched_texdef && (last.flags & 2u) != 0u;
                const bool texdef_depth_tag = matched_texdef && TexDefNameContains(last, "DEPTH");
                const bool is_depth         = depth_fmt || texdef_depth || texdef_depth_tag;

                if (!is_window && is_depth && out_h > 1280 && tex->dwMSAALevel != 0)
                    tex->dwMSAALevel = 0;

                if (is_depth) {
                    const bool critical_depth  = matched_texdef && IsCriticalDepthName(last);
                    const bool clampable_depth = matched_texdef && IsClampableDepthName(last) && out_h != 0 && out_h > 1280;
                    if (!clampable_depth || critical_depth)
                        return;
                }

                if (matched_texdef) {
                    const bool allow_fullres_skip = (out_h != 0 && out_h <= 1280);
                    if (allow_fullres_skip && IsFullResEffectName(last))
                        return;
                }

                if (is_window)
                    return;
                if (arg2 == 0)
                    return;

                const bool oversized = (cur_width > limit_w) || (cur_height > limit_h) || (cur_height == 0);
                if (!oversized)
                    return;

                const bool skip_output_depth = depth_target && out_w != 0 && out_h != 0 && cur_width == out_w && cur_height == out_h;
                if (skip_output_depth)
                    return;

                const bool skip_backbuffer_depth = depth_target && IsBackbufferDepthName(last);
                if (skip_backbuffer_depth)
                    return;

                const float scale = LocalCalcRenderScale(
                    cur_width ? cur_width : 1u,
                    cur_height ? cur_height : 1u,
                    limit_w,
                    limit_h
                );

                const u32 new_w = LocalScaleDim(cur_width ? cur_width : 1u, scale);
                const u32 new_h = LocalScaleDim(cur_height ? cur_height : 1u, scale);

                tex->dwPixelWidth  = new_w;
                tex->dwPixelHeight = new_h;

                if (tex->dwMipLevels > 1) {
                    const u32 longest  = std::max(new_w, new_h);
                    const u32 max_mips = 1u + static_cast<u32>(
                                                  std::floor(std::log2(static_cast<double>(longest)))
                                              );
                    tex->dwMipLevels = std::min(tex->dwMipLevels, max_mips);
                }

                if (tex->dwMSAALevel != 0)
                    tex->dwMSAALevel = 0;
            }
        };

    }  // namespace nvn
}  // namespace d3
