#pragma once

#include "lib/hook/inline.hpp"
#include "lib/util/modules.hpp"
#include "../../config.hpp"

namespace d3 {
    namespace nvn {

        namespace {
            struct NVNTexInfo {
                uint32 eTexFormat;
                uint32 dwMSAALevel;
                uint32 dwLastFrameUsed;
                uint32 eType;
                uint32 dwPixelWidth;
                uint32 dwPixelHeight;
                uint32 dwPixelDepth;
                uint32 dwMipLevels;
                uint8  tMemory[0x38];
                uint32 bTargetUpdated;
                uint32 bIsWindowTexture;
            };

            static_assert(offsetof(NVNTexInfo, bIsWindowTexture) == 0x5C);

            constexpr bool ShouldClampNvntCreate(uintptr_t lr_offset) {
                switch (lr_offset) {
                case 0xE6988:
                case 0xE69BC:
                case 0xE69F0:
                case 0xEB260:
                    return true;
                default:
                    return false;
                }
            }

            inline bool ClampTexturesEnabled() {
                return global_config.resolution_hack.active && global_config.resolution_hack.ClampTexturesEnabled();
            }

            inline bool GetClampDims(uint32 &limit_w, uint32 &limit_h) {
                if (!ClampTexturesEnabled())
                    return false;
                limit_h = global_config.resolution_hack.ClampTextureHeightPx();
                limit_w = global_config.resolution_hack.ClampTextureWidthPx();
                if (limit_w == 0 || limit_h == 0)
                    return false;
                return true;
            }

            inline float LocalCalcRenderScale(uint32 width, uint32 height, uint32 limit_w, uint32 limit_h) {
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

            inline uint32 LocalScaleDim(uint32 dim, float scale) {
                auto scaled = static_cast<uint32>(dim * scale);
                scaled &= ~1u;  // keep even
                if (scaled < 2)
                    scaled = 2;
                return scaled;
            }
        }  // namespace

        HOOK_DEFINE_INLINE(NVNTexInfoCreateHook) {
            // @ 0x00E6B20: NVNTexInfo::CreateNVNTexture / texture creation path
            static void Callback(exl::hook::InlineCtx *ctx) {
                auto *tex = reinterpret_cast<NVNTexInfo *>(ctx->X[0]);
                if (!tex)
                    return;

                const auto width     = tex->dwPixelWidth;
                const auto height    = tex->dwPixelHeight;
                const auto etype     = tex->eType;
                const auto lr_offset = static_cast<uintptr_t>(ctx->X[30] - exl::util::modules::GetTargetStart());

                // Minimal logging to avoid slowdowns: only large dims.
                if (width >= 8192 || height >= 8192) {
                    PRINT(
                        "NVNTexInfo large create: eType=%u fmt=%u msaa=%u %ux%u depth=%u mips=%u win=%u LR=0x%lx",
                        etype, tex->eTexFormat, tex->dwMSAALevel, width, height, tex->dwPixelDepth,
                        tex->dwMipLevels, tex->bIsWindowTexture, ctx->X[30]
                    );
                }

                uint32 limit_w = 0;
                uint32 limit_h = 0;
                if (!GetClampDims(limit_w, limit_h))
                    return;

                const bool oversized     = (width > limit_w) || (height > limit_h) || (height == 0);
                const bool known_rt_path = ShouldClampNvntCreate(lr_offset);

                if (oversized || known_rt_path) {
                    const float scale = LocalCalcRenderScale(width ? width : 1u, height ? height : 1u, limit_w, limit_h);

                    if (scale < 1.0f || oversized) {
                        const uint32 new_w = LocalScaleDim(width ? width : 1u, scale);
                        const uint32 new_h = LocalScaleDim(height ? height : 1u, scale);

                        tex->dwPixelWidth  = new_w;
                        tex->dwPixelHeight = new_h;

                        if (tex->dwMipLevels > 1) {
                            const uint32 longest  = std::max(new_w, new_h);
                            const uint32 max_mips = 1u + static_cast<uint32>(std::floor(std::log2(static_cast<double>(longest))));
                            tex->dwMipLevels      = std::min(tex->dwMipLevels, max_mips);
                        }

                        if (tex->dwMSAALevel != 0)
                            tex->dwMSAALevel = 0;

                        PRINT(
                            "NVNTexInfo clamp@0x%lx -> %ux%u (from %ux%u) mips=%u fmt=%u msaa=%u",
                            lr_offset, tex->dwPixelWidth, tex->dwPixelHeight, width, height,
                            tex->dwMipLevels, tex->eTexFormat, tex->dwMSAALevel
                        );
                    }
                }
            }
        };

    }  // namespace nvn
}  // namespace d3
