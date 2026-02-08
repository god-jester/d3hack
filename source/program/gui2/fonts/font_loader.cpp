#include "program/gui2/fonts/font_loader.hpp"

#include <array>
#include <cstddef>

#include "program/d3/setting.hpp"
#include "program/gui2/imgui_glyph_ranges.hpp"
#include "program/gui2/ui/overlay.hpp"

#include "imgui/imgui.h"

struct RWindow;
namespace d3 {
    extern ::RWindow *g_ptMainRWindow;
}

namespace nn::pl {
    enum SharedFontType {
        SharedFontType_Standard,
        SharedFontType_ChineseSimple,
        SharedFontType_ChineseSimpleExtension,
        SharedFontType_ChineseTraditional,
        SharedFontType_Korean,
        SharedFontType_NintendoExtension,
        SharedFontType_Max,
    };

    enum SharedFontLoadState {
        SharedFontLoadState_Loading,
        SharedFontLoadState_Loaded,
    };

    void RequestSharedFontLoad(SharedFontType sharedFontType) noexcept;
    auto GetSharedFontLoadState(SharedFontType sharedFontType) noexcept -> SharedFontLoadState;
    auto GetSharedFontAddress(SharedFontType sharedFontType) noexcept -> const void *;
    auto GetSharedFontSize(SharedFontType sharedFontType) noexcept -> size_t;
}  // namespace nn::pl

namespace d3::gui2::fonts::font_loader {
    namespace {
        bool    g_font_atlas_built       = false;
        bool    g_font_build_attempted   = false;
        bool    g_font_build_in_progress = false;
        ImFont *g_font_body              = nullptr;
        ImFont *g_font_title             = nullptr;
    }  // namespace

    auto IsFontAtlasBuilt() -> bool {
        return g_font_atlas_built;
    }

    auto GetTitleFont() -> ImFont * {
        return g_font_title;
    }

    auto GetBodyFont() -> ImFont * {
        return g_font_body;
    }

    auto PrepareFonts(d3::gui2::ui::Overlay &overlay, const std::string &desired_lang)
        -> PrepareResult {
        PrepareResult result {};

        static std::string s_font_lang;
        static std::string s_ranges_lang;
        static bool        s_logged_shared_defer = false;

        if (g_font_atlas_built && s_font_lang != desired_lang) {
            PRINT("[font_loader] Font lang changed (%s -> %s); rebuilding atlas", s_font_lang.c_str(), desired_lang.c_str());
            ImGuiIO &io = ImGui::GetIO();
            if (io.Fonts != nullptr) {
                io.Fonts->Clear();
            }
            g_font_atlas_built     = false;
            g_font_build_attempted = false;
            g_font_body            = nullptr;
            g_font_title           = nullptr;
            s_ranges_lang.clear();
            result.atlas_invalidated = true;
        }

        if (g_font_atlas_built) {
            result.atlas_built = true;
            return result;
        }

        // Heuristic for "boot is far enough along" to do heavier work.
        if (d3::g_ptMainRWindow == nullptr) {
            static bool s_logged_defer = false;
            if (!s_logged_defer) {
                PRINT_LINE("[font_loader] PrepareFonts: deferring until ShellInitialized");
                s_logged_defer = true;
            }
            return result;
        }

        if (g_font_build_attempted) {
            return result;
        }

        // Avoid re-entrancy if PrepareFonts() is called from multiple threads/hooks.
        // (Keep this lock-free/simple: Switch builds may not provide libatomic helpers for 1-byte atomics.)
        if (g_font_build_in_progress) {
            return result;
        }
        g_font_build_in_progress = true;
        struct ScopedClearInProgress {
            ~ScopedClearInProgress() { g_font_build_in_progress = false; }
        } const clear_in_progress {};

        g_font_build_attempted = true;

        ImGuiIO &io = ImGui::GetIO();
        if (io.Fonts == nullptr) {
            PRINT_LINE("[font_loader] ERROR: ImGuiIO::Fonts is null; cannot build font atlas");
            return result;
        }

        PRINT_LINE("[font_loader] Preparing ImGui font atlas...");
        // Prefer smaller atlas allocations on real hardware.
        // Non-power-of-two height can significantly reduce waste for large (CJK) atlases.
        io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
        io.Fonts->TexDesiredFormat   = ImTextureFormat_Alpha8;
        io.Fonts->TexPixelsUseColors = false;
        ImFontConfig font_cfg {};
        font_cfg.Name[0]              = 'd';
        font_cfg.Name[1]              = '3';
        font_cfg.Name[2]              = '\0';
        constexpr float kFontSizeBody = 15.0f;
        // Build for docked readability (we apply a runtime scale for 720p).
        font_cfg.SizePixels = kFontSizeBody;
        // Keep the atlas smaller on real hardware (especially for CJK).
        font_cfg.OversampleH                        = (desired_lang == "zh") ? 1 : 2;
        font_cfg.OversampleV                        = (desired_lang == "zh") ? 1 : 2;
        const std::array<ImWchar, 3> nvn_ext_ranges = {0xE000, 0xE152, 0};

        static ImVector<ImWchar> s_font_ranges;
        static bool              s_font_ranges_built = false;
        if (!s_font_ranges_built || s_ranges_lang != desired_lang) {
            // Keep the atlas small/stable: build a language-focused range set and rely on the
            // Nintendo shared system fonts for coverage, rather than importing every CJK block.
            ImFontGlyphRangesBuilder builder;
            builder.AddRanges(d3::imgui_overlay::glyph_ranges::GetDefault());

            if (desired_lang == "zh") {
                // Ultra-low-memory path: build ranges from the actual translated UI strings.
                overlay.AppendTranslationGlyphs(builder);
            } else if (desired_lang == "ko") {
                builder.AddRanges(d3::imgui_overlay::glyph_ranges::GetKorean());
            } else if (desired_lang == "ja") {
                builder.AddRanges(d3::imgui_overlay::glyph_ranges::GetJapanese());
            } else {
                // "Good enough" default: support Cyrillic for RU users without pulling in all CJK.
                builder.AddRanges(d3::imgui_overlay::glyph_ranges::GetCyrillic());
            }

            // Private-use glyphs used by Nintendo's extension fonts.
            builder.AddRanges(nvn_ext_ranges.data());

            s_font_ranges.clear();
            builder.BuildRanges(&s_font_ranges);
            s_font_ranges_built = true;
            s_ranges_lang       = desired_lang;
        }

        const ImWchar *ranges = s_font_ranges.Data;
        if (ranges == nullptr || ranges[0] == 0) {
            ranges = d3::imgui_overlay::glyph_ranges::GetDefault();
        }

        auto get_shared_font_type = [](const std::string &lang, bool extension) -> nn::pl::SharedFontType {
            if (lang == "zh") {
                return extension ? nn::pl::SharedFontType_ChineseSimpleExtension : nn::pl::SharedFontType_ChineseSimple;
            }
            if (lang == "ko") {
                return nn::pl::SharedFontType_Korean;
            }
            // Default: use Standard, with optional NintendoExtension for private-use glyphs.
            return extension ? nn::pl::SharedFontType_NintendoExtension : nn::pl::SharedFontType_Standard;
        };

        bool          used_shared_font = false;
        ImFont const *font_body        = nullptr;

        const nn::pl::SharedFontType shared_base = get_shared_font_type(desired_lang, false);
        const nn::pl::SharedFontType shared_ext  = get_shared_font_type(desired_lang, true);

        nn::pl::RequestSharedFontLoad(shared_base);
        if (shared_ext != shared_base) {
            nn::pl::RequestSharedFontLoad(shared_ext);
        } else {
            nn::pl::RequestSharedFontLoad(nn::pl::SharedFontType_NintendoExtension);
        }

        const bool base_loaded =
            nn::pl::GetSharedFontLoadState(shared_base) == nn::pl::SharedFontLoadState_Loaded;
        const bool ext_loaded =
            (shared_ext == shared_base) ||
            (nn::pl::GetSharedFontLoadState(shared_ext) == nn::pl::SharedFontLoadState_Loaded);

        if (!base_loaded) {
            if (!s_logged_shared_defer) {
                PRINT_LINE("[font_loader] Shared base font still loading; deferring atlas build");
                s_logged_shared_defer = true;
            }
            g_font_build_attempted = false;
            return result;
        } else {
            s_logged_shared_defer = false;
            const void  *shared_font = nn::pl::GetSharedFontAddress(shared_base);
            const size_t shared_size = nn::pl::GetSharedFontSize(shared_base);
            if (shared_font == nullptr || shared_size == 0) {
                PRINT_LINE("[font_loader] Shared base font unavailable; address/size invalid");
            } else {
                ImFontConfig body_cfg         = font_cfg;
                body_cfg.FontDataOwnedByAtlas = false;
                body_cfg.GlyphRanges          = ranges;
                body_cfg.SizePixels           = kFontSizeBody;
                font_body                     = io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(shared_font), static_cast<int>(shared_size), body_cfg.SizePixels, &body_cfg);
                if (font_body == nullptr) {
                    PRINT_LINE("[font_loader] ERROR: AddFontFromMemoryTTF failed for shared base font");
                } else {
                    used_shared_font = true;
                }
            }
        }

        if (used_shared_font && ext_loaded) {
            const void  *shared_ext_font = nn::pl::GetSharedFontAddress(shared_ext);
            const size_t shared_ext_size = nn::pl::GetSharedFontSize(shared_ext);
            if (shared_ext_font == nullptr || shared_ext_size == 0) {
                PRINT_LINE("[font_loader] Shared extension font unavailable; address/size invalid");
            } else {
                ImFontConfig ext_cfg         = font_cfg;
                ext_cfg.MergeMode            = true;
                ext_cfg.FontDataOwnedByAtlas = false;
                ext_cfg.GlyphRanges          = nvn_ext_ranges.data();
                ext_cfg.SizePixels           = kFontSizeBody;
                if (io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(shared_ext_font), static_cast<int>(shared_ext_size), ext_cfg.SizePixels, &ext_cfg) == nullptr) {
                    PRINT_LINE("[font_loader] ERROR: AddFontFromMemoryTTF failed for shared extension font");
                }
            }
        }

        // Always try to merge the NVN extension (private-use) font if present.
        if (used_shared_font) {
            const void  *nvn_ext_font = nn::pl::GetSharedFontAddress(nn::pl::SharedFontType_NintendoExtension);
            const size_t nvn_ext_size = nn::pl::GetSharedFontSize(nn::pl::SharedFontType_NintendoExtension);
            if (nvn_ext_font != nullptr && nvn_ext_size != 0) {
                ImFontConfig ext_cfg         = font_cfg;
                ext_cfg.MergeMode            = true;
                ext_cfg.FontDataOwnedByAtlas = false;
                ext_cfg.GlyphRanges          = nvn_ext_ranges.data();
                ext_cfg.SizePixels           = kFontSizeBody;
                if (io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(nvn_ext_font), static_cast<int>(nvn_ext_size), ext_cfg.SizePixels, &ext_cfg) == nullptr) {
                    PRINT_LINE("[font_loader] ERROR: AddFontFromMemoryTTF failed for shared Nintendo extension");
                } else {
                    PRINT_LINE("[font_loader] Loaded Nintendo shared system font (NintendoExtension)");
                }
            }
        }

        if (!used_shared_font) {
            ImFontConfig body_cfg = font_cfg;
            body_cfg.SizePixels   = kFontSizeBody;
            font_body             = io.Fonts->AddFontDefault(&body_cfg);
        }

        g_font_body  = const_cast<ImFont *>(font_body);
        g_font_title = g_font_body;

        g_font_atlas_built = true;
        result.atlas_built = true;
        s_font_lang        = desired_lang;
        PRINT_LINE("[font_loader] ImGui font atlas prepared");
        return result;
    }

}  // namespace d3::gui2::fonts::font_loader
