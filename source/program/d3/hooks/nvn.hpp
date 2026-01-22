#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>

#include "lib/hook/inline.hpp"
#include "lib/util/modules.hpp"
#include "../../config.hpp"
#include "nvn/nvn.h"

namespace d3::nvn {
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

    inline auto ClampTexturesEnabled() -> bool {
        return global_config.resolution_hack.active && global_config.resolution_hack.ClampTexturesEnabled();
    }

    inline auto GetClampDims(u32 &limitW, u32 &limitH) -> bool {
        if (!ClampTexturesEnabled())
            return false;
        limitH = global_config.resolution_hack.ClampTextureHeightPx();
        limitW = global_config.resolution_hack.ClampTextureWidthPx();
        return limitW != 0 && limitH != 0;
    }

    inline auto IsDepthFormat(u32 fmt) -> bool {
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

    inline auto LocalCalcRenderScale(u32 width, u32 height, u32 limitW, u32 limitH) -> float {
        if (width == 0 || height == 0)
            return 1.0f;
        float scale = 1.0f;
        if (width > limitW)
            scale = static_cast<float>(limitW) / static_cast<float>(width);
        if (height > limitH) {
            const float hscale = static_cast<float>(limitH) / static_cast<float>(height);
            scale              = std::min(hscale, scale);
        }
        scale = std::min(scale, 1.0f);
        if (scale <= 0.0f)
            scale = 1.0f;
        return scale;
    }

    inline auto LocalScaleDim(u32 dim, float scale) -> u32 {
        auto scaled = static_cast<u32>(dim * scale);
        scaled &= ~1u;  // keep even
        scaled = std::max<u32>(scaled, 2);
        return scaled;
    }

    constexpr uintptr_t kSwapchainTexCreateLrOffsets[] = {
        0xE6988,
        0xE69BC,
        0xE69F0,
    };

    inline auto IsSwapchainTexCreateLR(uintptr_t lrOffset) -> bool {
        for (const auto lr : kSwapchainTexCreateLrOffsets) {
            if (lrOffset == lr || lrOffset + 4 == lr)
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
        uintptr_t namePtr;
        bool      hasName;
        bool      valid;
        char      name[64];
    };

    inline TexDefCallInfo g_lastTexDef = {};

    inline auto TexDefNameContains(const TexDefCallInfo &info, const char *needle) -> bool {
        if (!info.hasName || info.name[0] == '\0' || (needle == nullptr) || needle[0] == '\0')
            return false;
        return std::strstr(info.name, needle) != nullptr;
    }

    inline auto IsBackbufferDepthName(const TexDefCallInfo &info) -> bool {
        return TexDefNameContains(info, "BACKBUFFER_DEPTH") || TexDefNameContains(info, "BACKBUFFER");
    }

    inline auto IsFullResEffectName(const TexDefCallInfo &info) -> bool {
        if (TexDefNameContains(info, "BACKBUFFER"))
            return true;
        if (TexDefNameContains(info, "SCREEN_"))
            return true;
        return false;
    }

    inline auto IsCriticalDepthName(const TexDefCallInfo &info) -> bool {
        if (IsBackbufferDepthName(info))
            return true;
        if (TexDefNameContains(info, "DEPTH Effect RT"))
            return true;
        return false;
    }

    inline auto IsClampableDepthName(const TexDefCallInfo &info) -> bool {
        if (TexDefNameContains(info, "Shadow"))
            return true;
        if (TexDefNameContains(info, "DepthTarget"))
            return true;
        return false;
    }

    inline void CaptureTexDefCreateCall(exl::hook::InlineCtx *ctx) {
        if (ctx == nullptr)
            return;

        TexDefCallInfo info = {};
        info.fmt            = ctx->W[1];
        info.width          = ctx->W[2];
        info.height         = ctx->W[3];
        info.usage          = ctx->W[5];
        info.flags          = ctx->W[6];

        const auto namePtr = static_cast<uintptr_t>(ctx->X[20]);
        info.namePtr       = namePtr;
        info.hasName       = false;
        info.name[0]       = '\0';
        if (namePtr != 0) {
            const auto *module = exl::util::TryGetModule(namePtr);
            if (module != nullptr) {
                const char *name = reinterpret_cast<const char *>(namePtr);
                for (u32 i = 0; i + 1 < static_cast<u32>(sizeof(info.name)); ++i) {
                    const char c = name[i];
                    info.name[i] = c;
                    if (c == '\0')
                        break;
                }
                info.name[sizeof(info.name) - 1] = '\0';
                info.hasName                     = true;
            }
        }

        info.valid   = true;
        g_lastTexDef = info;
    }

    HOOK_DEFINE_INLINE(TexDefCreateCallHook) {
        // @ 0x0028365C: sTextureDefinitionCreate callsite into TextureCreate.
        static void Callback(exl::hook::InlineCtx *ctx) { CaptureTexDefCreateCall(ctx); }
    };

    HOOK_DEFINE_INLINE(NVNTexInfoCreateHook) {
        // @ 0x00E6B20: NVNTexInfo::CreateNVNTexture / texture creation path
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto *tex = reinterpret_cast<NVNTexInfo *>(ctx->X[0]);
            if (tex == nullptr)
                return;

            const u32  width      = tex->dwPixelWidth;
            const u32  height     = tex->dwPixelHeight;
            const u32  arg2       = ctx->W[2];
            const u32  arg3       = ctx->W[3];
            const auto lr         = static_cast<uintptr_t>(ctx->m_Gpr.m_Lr.X);
            const auto lrOffset   = static_cast<uintptr_t>(lr - exl::util::modules::GetTargetStart());
            u32        cur_width  = width;
            u32        cur_height = height;

            const u32 outW = global_config.resolution_hack.OutputWidthPx();
            const u32 outH = global_config.resolution_hack.OutputHeightPx();

            if (global_config.resolution_hack.active && outW != 0 && outH != 0 && IsSwapchainTexCreateLR(lrOffset)) {
                if (cur_width != outW || cur_height != outH) {
                    tex->dwPixelWidth  = outW;
                    tex->dwPixelHeight = outH;
                    cur_width          = outW;
                    cur_height         = outH;

                    if (tex->dwMipLevels > 1) {
                        const u32 longest = std::max(outW, outH);
                        const u32 maxMips = 1u + static_cast<u32>(
                                                     std::floor(std::log2(static_cast<double>(longest)))
                                                 );
                        tex->dwMipLevels = std::min(tex->dwMipLevels, maxMips);
                    }
                }
                return;
            }

            TexDefCallInfo const last          = g_lastTexDef;
            const bool           matchedTexdef = last.valid && last.width == width && last.height == height && last.fmt == tex->eTexFormat;
            if (matchedTexdef)
                g_lastTexDef.valid = false;

            u32 limit_w = 0;
            u32 limit_h = 0;
            if (!GetClampDims(limit_w, limit_h))
                return;

            const bool depthTarget = matchedTexdef && (last.flags & 2u) != 0u;
            const bool isWindow    = (arg3 != 0);

            const bool depthFmt       = IsDepthFormat(tex->eTexFormat);
            const bool texdefDepth    = matchedTexdef && (last.flags & 2u) != 0u;
            const bool texdefDepthTag = matchedTexdef && TexDefNameContains(last, "DEPTH");
            const bool isDepth        = depthFmt || texdefDepth || texdefDepthTag;

            if (!isWindow && isDepth && outH > 1280 && tex->dwMSAALevel != 0)
                tex->dwMSAALevel = 0;

            if (isDepth) {
                const bool criticalDepth  = matchedTexdef && IsCriticalDepthName(last);
                const bool clampableDepth = matchedTexdef && IsClampableDepthName(last) && outH != 0 && outH > 1280;
                if (!clampableDepth || criticalDepth)
                    return;
            }

            if (matchedTexdef) {
                const bool allowFullresSkip = (outH != 0 && outH <= 1280);
                if (allowFullresSkip && IsFullResEffectName(last))
                    return;
            }

            if (isWindow)
                return;
            if (arg2 == 0)
                return;

            const bool oversized = (cur_width > limit_w) || (cur_height > limit_h) || (cur_height == 0);
            if (!oversized)
                return;

            const bool skipOutputDepth = depthTarget && outW != 0 && outH != 0 && cur_width == outW && cur_height == outH;
            if (skipOutputDepth)
                return;

            const bool skipBackbufferDepth = depthTarget && IsBackbufferDepthName(last);
            if (skipBackbufferDepth)
                return;

            const float scale = LocalCalcRenderScale(
                (cur_width != 0u) ? cur_width : 1u,
                (cur_height != 0u) ? cur_height : 1u,
                limit_w,
                limit_h
            );

            const u32 newW = LocalScaleDim((cur_width != 0u) ? cur_width : 1u, scale);
            const u32 newH = LocalScaleDim((cur_height != 0u) ? cur_height : 1u, scale);

            tex->dwPixelWidth  = newW;
            tex->dwPixelHeight = newH;

            if (tex->dwMipLevels > 1) {
                const u32 longest = std::max(newW, newH);
                const u32 maxMips = 1u + static_cast<u32>(
                                             std::floor(std::log2(static_cast<double>(longest)))
                                         );
                tex->dwMipLevels = std::min(tex->dwMipLevels, maxMips);
            }

            if (tex->dwMSAALevel != 0)
                tex->dwMSAALevel = 0;
        }
    };

}  // namespace d3::nvn
