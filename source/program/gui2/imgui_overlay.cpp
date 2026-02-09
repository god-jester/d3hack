#include "program/gui2/imgui_overlay.hpp"

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>
#include <string>

#include "types.h"

#include "nn/nn_common.hpp"
#include "nn/hid.hpp"  // IWYU pragma: keep
#include "nn/oe.hpp"   // IWYU pragma: keep
#include "nn/os.hpp"   // IWYU pragma: keep
#include "program/romfs_assets.hpp"
#include "program/gui2/backend/nvn_hooks.hpp"
#include "program/gui2/fonts/font_loader.hpp"
#include "program/gui2/input/hid_block.hpp"
#include "program/gui2/memory/imgui_alloc.hpp"
#include "program/gui2/ui/overlay.hpp"
#include "program/gui2/ui/windows/notifications_window.hpp"
#include "program/gui2/input_util.hpp"
#include "program/d3/_util.hpp"
#include "program/d3/setting.hpp"
#include "program/log_once.hpp"
#include "symbols/common.hpp"
#include "tomlplusplus/toml.hpp"

#include "imgui/imgui.h"
#include "imgui_backend/imgui_impl_nvn.hpp"

namespace d3::imgui_overlay {
    namespace {

        constexpr bool kImGuiBringup_DrawText                = true;
        constexpr bool kImGuiBringup_AlwaysSubmitProofOfLife = false;
        // Docking is required for the main overlay layout/dockspace.
        bool g_imgui_ctx_initialized = false;
        bool g_backend_initialized   = false;
        bool g_font_uploaded         = false;
        bool g_hooks_installed       = false;

        float g_overlay_toggle_hold_s = 0.0f;
        bool  g_overlay_toggle_armed  = true;

        static void OnPresent(NVNqueue *queue, NVNwindow *window, int texture_index);

        struct NpadCombinedState {
            u64                       buttons = 0;
            nn::hid::AnalogStickState stick_l {};
            nn::hid::AnalogStickState stick_r {};
            bool                      have_stick_l = false;
            bool                      have_stick_r = false;
        };

        static auto CombineNpadState(NpadCombinedState &out, uint port) -> bool {
            out = {};

            d3::gui2::input::hid_block::ScopedHidPassthroughForOverlay const passthrough_guard;

            auto consider_stick = [](bool &have, nn::hid::AnalogStickState &dst, const nn::hid::AnalogStickState &src) -> void {
                const auto mag     = static_cast<u64>(std::abs(src.X)) + static_cast<u64>(std::abs(src.Y));
                const auto dst_mag = static_cast<u64>(std::abs(dst.X)) + static_cast<u64>(std::abs(dst.Y));
                if (!have || mag > dst_mag) {
                    dst  = src;
                    have = true;
                }
            };

            auto accumulate = [&](const nn::hid::NpadBaseState &state) -> void {
                out.buttons |= state.mButtons.field[0];
                consider_stick(out.have_stick_l, out.stick_l, state.mAnalogStickL);
                consider_stick(out.have_stick_r, out.stick_r, state.mAnalogStickR);
            };

            constexpr uint              kHandheldPort = 0x20;
            const nn::hid::NpadStyleSet style         = nn::hid::GetNpadStyleSet(port);
            bool                        read_any      = false;

            if (style.isBitSet(nn::hid::NpadStyleTag::NpadStyleFullKey)) {
                nn::hid::NpadFullKeyState full {};
                nn::hid::GetNpadState(&full, port);
                accumulate(full);
                read_any = true;
            }
            if (style.isBitSet(nn::hid::NpadStyleTag::NpadStyleJoyDual)) {
                nn::hid::NpadJoyDualState joy_dual {};
                nn::hid::GetNpadState(&joy_dual, port);
                accumulate(joy_dual);
                read_any = true;
            }
            if (style.isBitSet(nn::hid::NpadStyleTag::NpadStyleJoyLeft)) {
                nn::hid::NpadJoyLeftState joy_left {};
                nn::hid::GetNpadState(&joy_left, port);
                accumulate(joy_left);
                read_any = true;
            }
            if (style.isBitSet(nn::hid::NpadStyleTag::NpadStyleJoyRight)) {
                nn::hid::NpadJoyRightState joy_right {};
                nn::hid::GetNpadState(&joy_right, port);
                accumulate(joy_right);
                read_any = true;
            }

            if (style.isBitSet(nn::hid::NpadStyleTag::NpadStyleHandheld) || !read_any) {
                nn::hid::NpadHandheldState handheld {};
                nn::hid::GetNpadState(&handheld, kHandheldPort);
                accumulate(handheld);
                read_any = true;
            }

            return out.buttons != 0 || out.have_stick_l || out.have_stick_r;
        }

        static auto NpadButtonDown(u64 buttons, nn::hid::NpadButton button) -> bool {
            const auto bit = static_cast<u64>(button);
            return (buttons & (1ULL << bit)) != 0;
        }

        static auto KeyboardKeyDown(const nn::hid::KeyboardState &state, nn::hid::KeyboardKey key) -> bool {
            return state.keys.isBitSet(key);
        }

        static d3::gui2::ui::Overlay g_overlay {};
        static NpadCombinedState     g_last_npad {};
        static bool                  g_last_npad_valid = false;

        static void PushImGuiGamepadInputs(float dt_s);
        static auto PushImGuiTouchInputs(const ImVec2 &viewport_size) -> bool;
        static auto PushImGuiMouseInputs(const ImVec2 &viewport_size) -> bool;
        static void PushImGuiKeyboardInputs();
        static void DetectOverlaySwipe(const ImVec2 &viewport_size);
        static void UpdateImGuiStyleAndScale(ImVec2 viewport_size);

        static bool g_touch_init_tried = false;

        static void EnsureTouchInitialized() {
            if (g_touch_init_tried) {
                return;
            }
            g_touch_init_tried = true;
            nn::hid::InitializeTouchScreen();
        }

        static bool                   g_imgui_style_initialized = false;
        static ImGuiStyle             g_imgui_base_style {};
        static float                  g_last_gui_scale = -1.0f;
        static d3::gui2::ui::GuiTheme g_imgui_theme    = d3::gui2::ui::GuiTheme::D3Dark;

        static auto ComputeGuiScale(ImVec2 viewport_size) -> float {
            constexpr float kScaleMinClamp = 0.95f;
            constexpr float kScaleMaxClamp = 1.80f;

            constexpr float kH0 = 720.0f;
            constexpr float kS0 = 1.00f;
            constexpr float kH1 = 1080.0f;
            constexpr float kS1 = 1.12f;
            constexpr float kH2 = 1440.0f;
            constexpr float kS2 = 1.28f;
            constexpr float kH3 = 2160.0f;
            constexpr float kS3 = 1.58f;

            const float h = viewport_size.y;
            if (!(h > 0.0f)) {
                return kS1;
            }

            float scale = kS0;
            if (h <= kH1) {
                const float t = std::clamp((h - kH0) / (kH1 - kH0), 0.0f, 1.0f);
                scale         = kS0 + t * (kS1 - kS0);
            } else if (h <= kH2) {
                const float t = std::clamp((h - kH1) / (kH2 - kH1), 0.0f, 1.0f);
                scale         = kS1 + t * (kS2 - kS1);
            } else if (h <= kH3) {
                const float t = std::clamp((h - kH2) / (kH3 - kH2), 0.0f, 1.0f);
                scale         = kS2 + t * (kS3 - kS2);
            } else {
                constexpr float kGrowthPerPx = 0.00016f;

                scale = kS3 + (h - kH3) * kGrowthPerPx;
            }

            return std::clamp(scale, kScaleMinClamp, kScaleMaxClamp);
        }

        static void ApplyImGuiStyleCommon(ImGuiStyle &style) {
            style.WindowPadding     = ImVec2(14.0f, 12.0f);
            style.FramePadding      = ImVec2(10.0f, 8.0f);
            style.ItemSpacing       = ImVec2(10.0f, 9.0f);
            style.ItemInnerSpacing  = ImVec2(6.0f, 5.0f);
            style.IndentSpacing     = 18.0f;
            style.ScrollbarSize     = 18.0f;
            style.GrabMinSize       = 12.0f;
            style.TouchExtraPadding = ImVec2(6.0f, 6.0f);

            style.WindowRounding    = 7.0f;
            style.ChildRounding     = 5.0f;
            style.FrameRounding     = 5.0f;
            style.PopupRounding     = 5.0f;
            style.ScrollbarRounding = 9.0f;
            style.GrabRounding      = 5.0f;
            style.TabRounding       = 5.0f;

            style.WindowBorderSize         = 1.0f;
            style.WindowBorderHoverPadding = 4.0f;
            style.FrameBorderSize          = 1.0f;
            style.TabBorderSize            = 1.0f;
        }

        static auto ThemeOverrideStem(d3::gui2::ui::GuiTheme theme) -> const char * {
            switch (theme) {
            case d3::gui2::ui::GuiTheme::D3Dark:
                return "d3";
            case d3::gui2::ui::GuiTheme::Blueish:
                return "blueish";
            }
            return "d3";
        }

        static auto NormalizeThemeKey(std::string_view input) -> std::string {
            std::string out;
            out.reserve(input.size());
            for (const char ch : input) {
                const unsigned char uch = static_cast<unsigned char>(ch);
                if (std::isspace(uch) || ch == '_' || ch == '-') {
                    continue;
                }
                out.push_back(static_cast<char>(std::tolower(uch)));
            }
            if (out.rfind("imguicol", 0) == 0) {
                out.erase(0, std::strlen("imguicol"));
            }
            return out;
        }

        static auto ParseHexNibble(char ch, u8 &out) -> bool {
            if (ch >= '0' && ch <= '9') {
                out = static_cast<u8>(ch - '0');
                return true;
            }
            if (ch >= 'a' && ch <= 'f') {
                out = static_cast<u8>(10 + (ch - 'a'));
                return true;
            }
            if (ch >= 'A' && ch <= 'F') {
                out = static_cast<u8>(10 + (ch - 'A'));
                return true;
            }
            return false;
        }

        static auto ParseHexColor(std::string_view s, ImVec4 &out) -> bool {
            if (s.empty()) {
                return false;
            }
            if (s.front() == '#') {
                s.remove_prefix(1);
            }
            if (s.size() != 6 && s.size() != 8) {
                return false;
            }

            auto parse_byte = [&](size_t i, u8 &b) -> bool {
                u8 hi = 0;
                u8 lo = 0;
                if (!ParseHexNibble(s[i], hi) || !ParseHexNibble(s[i + 1], lo)) {
                    return false;
                }
                b = static_cast<u8>((hi << 4) | lo);
                return true;
            };

            u8 r = 0, g = 0, b = 0, a = 0xFF;
            if (!parse_byte(0, r) || !parse_byte(2, g) || !parse_byte(4, b)) {
                return false;
            }
            if (s.size() == 8) {
                if (!parse_byte(6, a)) {
                    return false;
                }
            }

            out = ImVec4(
                static_cast<float>(r) / 255.0f,
                static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f,
                static_cast<float>(a) / 255.0f
            );
            return true;
        }

        static void TryApplyThemeOverrides(ImGuiStyle &style, d3::gui2::ui::GuiTheme theme) {
            std::string path = "sd:/config/d3hack-nx/themes/";
            path += ThemeOverrideStem(theme);
            path += ".toml";

            std::string text;
            if (!d3::romfs::ReadFileToString(path.c_str(), text, 64 * 1024)) {
                return;
            }

            auto result = toml::parse(text, std::string_view {path});
            if (!result) {
                if (d3::log_once::ShouldLog("gui.theme_override.parse_failed")) {
                    const auto &err = result.error();
                    PRINT("[gui] theme override parse failed: %s", std::string(err.description()).c_str());
                }
                return;
            }

            const toml::table &root   = result.table();
            const toml::table *colors = nullptr;
            if (auto node = root.get("colors"); node && node->is_table()) {
                colors = node->as_table();
            }
            if (colors == nullptr) {
                return;
            }

            auto apply_color = [&](std::string_view key, const toml::node &node) -> void {
                auto v = node.value<std::string>();
                if (!v) {
                    return;
                }
                ImVec4 color {};
                if (!ParseHexColor(*v, color)) {
                    return;
                }

                const std::string normalized = NormalizeThemeKey(key);
                if (normalized == "text") {
                    style.Colors[ImGuiCol_Text] = color;
                } else if (normalized == "textdisabled") {
                    style.Colors[ImGuiCol_TextDisabled] = color;
                } else if (normalized == "windowbg") {
                    style.Colors[ImGuiCol_WindowBg] = color;
                } else if (normalized == "popupbg") {
                    style.Colors[ImGuiCol_PopupBg] = color;
                } else if (normalized == "border") {
                    style.Colors[ImGuiCol_Border] = color;
                } else if (normalized == "framebg") {
                    style.Colors[ImGuiCol_FrameBg] = color;
                } else if (normalized == "framebghovered") {
                    style.Colors[ImGuiCol_FrameBgHovered] = color;
                } else if (normalized == "framebgactive") {
                    style.Colors[ImGuiCol_FrameBgActive] = color;
                } else if (normalized == "titlebg") {
                    style.Colors[ImGuiCol_TitleBg] = color;
                } else if (normalized == "titlebgactive") {
                    style.Colors[ImGuiCol_TitleBgActive] = color;
                } else if (normalized == "menubarbg") {
                    style.Colors[ImGuiCol_MenuBarBg] = color;
                } else if (normalized == "checkmark") {
                    style.Colors[ImGuiCol_CheckMark] = color;
                } else if (normalized == "slidergrab") {
                    style.Colors[ImGuiCol_SliderGrab] = color;
                } else if (normalized == "slidergrabactive") {
                    style.Colors[ImGuiCol_SliderGrabActive] = color;
                } else if (normalized == "button") {
                    style.Colors[ImGuiCol_Button] = color;
                } else if (normalized == "buttonhovered") {
                    style.Colors[ImGuiCol_ButtonHovered] = color;
                } else if (normalized == "buttonactive") {
                    style.Colors[ImGuiCol_ButtonActive] = color;
                } else if (normalized == "header") {
                    style.Colors[ImGuiCol_Header] = color;
                } else if (normalized == "headerhovered") {
                    style.Colors[ImGuiCol_HeaderHovered] = color;
                } else if (normalized == "headeractive") {
                    style.Colors[ImGuiCol_HeaderActive] = color;
                } else if (normalized == "separator") {
                    style.Colors[ImGuiCol_Separator] = color;
                } else if (normalized == "separatorhovered") {
                    style.Colors[ImGuiCol_SeparatorHovered] = color;
                } else if (normalized == "separatoractive") {
                    style.Colors[ImGuiCol_SeparatorActive] = color;
                } else if (normalized == "tab") {
                    style.Colors[ImGuiCol_Tab] = color;
                } else if (normalized == "tabhovered") {
                    style.Colors[ImGuiCol_TabHovered] = color;
                } else if (normalized == "tabselected") {
                    style.Colors[ImGuiCol_TabSelected] = color;
                } else if (normalized == "textselectedbg") {
                    style.Colors[ImGuiCol_TextSelectedBg] = color;
                } else if (normalized == "navcursor") {
                    style.Colors[ImGuiCol_NavCursor] = color;
                } else if (normalized == "navwindowinghighlight") {
                    style.Colors[ImGuiCol_NavWindowingHighlight] = color;
                }
            };

            colors->for_each([&](const toml::key &k, const toml::node &v) -> void {
                apply_color(std::string_view {k.str()}, v);
            });

            const char *key = (theme == d3::gui2::ui::GuiTheme::Blueish) ? "gui.theme_override.applied.blueish" : "gui.theme_override.applied.d3";
            if (d3::log_once::ShouldLog(key)) {
                PRINT("[gui] theme overrides applied: %s", path.c_str());
            }
        }

        static void ApplyImGuiThemeColors(ImGuiStyle &style, d3::gui2::ui::GuiTheme theme) {
            ImVec4 *colors = style.Colors;

            if (theme == d3::gui2::ui::GuiTheme::Blueish) {
                // Lunakit-inspired: deep navy + misty cyan accent.
                constexpr ImVec4 kAccent      = ImVec4(0.40f, 0.78f, 0.82f, 1.00f);
                constexpr ImVec4 kAccentHi    = ImVec4(0.55f, 0.88f, 0.92f, 1.00f);
                constexpr ImVec4 kBg          = ImVec4(0.06f, 0.07f, 0.09f, 0.94f);
                constexpr ImVec4 kPanel       = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
                constexpr ImVec4 kPanelHover  = ImVec4(0.18f, 0.19f, 0.23f, 1.00f);
                constexpr ImVec4 kPanelActive = ImVec4(0.22f, 0.23f, 0.27f, 1.00f);

                colors[ImGuiCol_Text]         = ImVec4(0.93f, 0.95f, 0.98f, 1.00f);
                colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.58f, 0.62f, 1.00f);
                colors[ImGuiCol_WindowBg]     = kBg;
                colors[ImGuiCol_PopupBg]      = ImVec4(0.05f, 0.06f, 0.08f, 0.94f);
                colors[ImGuiCol_Border]       = ImVec4(0.20f, 0.23f, 0.27f, 0.90f);
                colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

                colors[ImGuiCol_FrameBg]        = kPanel;
                colors[ImGuiCol_FrameBgHovered] = kPanelHover;
                colors[ImGuiCol_FrameBgActive]  = kPanelActive;

                colors[ImGuiCol_TitleBg]          = ImVec4(0.08f, 0.10f, 0.14f, 1.00f);
                colors[ImGuiCol_TitleBgActive]    = ImVec4(0.10f, 0.13f, 0.18f, 1.00f);
                colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.10f, 0.14f, 0.80f);
                colors[ImGuiCol_MenuBarBg]        = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);

                colors[ImGuiCol_CheckMark]        = kAccent;
                colors[ImGuiCol_SliderGrab]       = kAccent;
                colors[ImGuiCol_SliderGrabActive] = kAccentHi;

                colors[ImGuiCol_Button]        = ImVec4(0.16f, 0.17f, 0.20f, 1.00f);
                colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.24f, 0.28f, 1.00f);
                colors[ImGuiCol_ButtonActive]  = ImVec4(0.26f, 0.28f, 0.32f, 1.00f);

                colors[ImGuiCol_Header]        = ImVec4(0.18f, 0.19f, 0.23f, 1.00f);
                colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.26f, 0.30f, 1.00f);
                colors[ImGuiCol_HeaderActive]  = ImVec4(0.28f, 0.30f, 0.34f, 1.00f);

                colors[ImGuiCol_Separator]        = ImVec4(0.20f, 0.23f, 0.27f, 1.00f);
                colors[ImGuiCol_SeparatorHovered] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.85f);
                colors[ImGuiCol_SeparatorActive]  = ImVec4(kAccentHi.x, kAccentHi.y, kAccentHi.z, 0.95f);

                colors[ImGuiCol_Tab]               = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);
                colors[ImGuiCol_TabHovered]        = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.55f);
                colors[ImGuiCol_TabSelected]       = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
                colors[ImGuiCol_TabDimmed]         = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
                colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);

                colors[ImGuiCol_TextSelectedBg]        = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.35f);
                colors[ImGuiCol_NavCursor]             = ImVec4(kAccentHi.x, kAccentHi.y, kAccentHi.z, 0.80f);
                colors[ImGuiCol_NavWindowingHighlight] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.80f);
                TryApplyThemeOverrides(style, theme);
                return;
            }

            // D3-ish palette: dark stone + gold accent.
            constexpr ImVec4 kAccent      = ImVec4(0.88f, 0.70f, 0.20f, 1.00f);
            constexpr ImVec4 kAccentHi    = ImVec4(0.97f, 0.84f, 0.32f, 1.00f);
            constexpr ImVec4 kBg          = ImVec4(0.07f, 0.07f, 0.08f, 0.94f);
            constexpr ImVec4 kPanel       = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
            constexpr ImVec4 kPanelHover  = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
            constexpr ImVec4 kPanelActive = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);

            colors[ImGuiCol_Text]         = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.58f, 1.00f);
            colors[ImGuiCol_WindowBg]     = kBg;
            colors[ImGuiCol_PopupBg]      = ImVec4(0.06f, 0.06f, 0.07f, 0.94f);
            colors[ImGuiCol_Border]       = ImVec4(0.23f, 0.23f, 0.26f, 0.90f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

            colors[ImGuiCol_FrameBg]        = kPanel;
            colors[ImGuiCol_FrameBgHovered] = kPanelHover;
            colors[ImGuiCol_FrameBgActive]  = kPanelActive;

            colors[ImGuiCol_TitleBg]          = ImVec4(0.12f, 0.04f, 0.04f, 1.00f);
            colors[ImGuiCol_TitleBgActive]    = ImVec4(0.18f, 0.06f, 0.06f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.12f, 0.04f, 0.04f, 0.80f);
            colors[ImGuiCol_MenuBarBg]        = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);

            colors[ImGuiCol_CheckMark]        = kAccent;
            colors[ImGuiCol_SliderGrab]       = kAccent;
            colors[ImGuiCol_SliderGrabActive] = kAccentHi;

            colors[ImGuiCol_Button]        = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
            colors[ImGuiCol_ButtonActive]  = ImVec4(0.30f, 0.30f, 0.33f, 1.00f);

            colors[ImGuiCol_Header]        = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.28f, 0.31f, 1.00f);
            colors[ImGuiCol_HeaderActive]  = ImVec4(0.32f, 0.32f, 0.35f, 1.00f);

            colors[ImGuiCol_Separator]        = ImVec4(0.23f, 0.23f, 0.26f, 1.00f);
            colors[ImGuiCol_SeparatorHovered] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 1.00f);
            colors[ImGuiCol_SeparatorActive]  = ImVec4(kAccentHi.x, kAccentHi.y, kAccentHi.z, 1.00f);

            colors[ImGuiCol_Tab]               = ImVec4(0.13f, 0.13f, 0.14f, 1.00f);
            colors[ImGuiCol_TabHovered]        = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.55f);
            colors[ImGuiCol_TabSelected]       = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_TabDimmed]         = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
            colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);

            colors[ImGuiCol_TextSelectedBg]        = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.35f);
            colors[ImGuiCol_NavCursor]             = ImVec4(kAccentHi.x, kAccentHi.y, kAccentHi.z, 0.80f);
            colors[ImGuiCol_NavWindowingHighlight] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.80f);

            TryApplyThemeOverrides(style, theme);
        }

        static void InitializeImGuiStyle(d3::gui2::ui::GuiTheme theme) {
            ImGuiStyle &style = ImGui::GetStyle();
            style             = ImGuiStyle();
            ImGui::StyleColorsDark(&style);
            ApplyImGuiStyleCommon(style);
            ApplyImGuiThemeColors(style, theme);
            if (style.WindowBorderHoverPadding < 1.0f) {
                style.WindowBorderHoverPadding = 1.0f;
            }

            g_imgui_base_style        = style;
            g_imgui_style_initialized = true;
            g_imgui_theme             = theme;
        }

        static void UpdateImGuiStyleAndScale(ImVec2 viewport_size) {
            const auto desired_theme = g_overlay.theme();
            if (!g_imgui_style_initialized || desired_theme != g_imgui_theme) {
                InitializeImGuiStyle(desired_theme);
                g_last_gui_scale = -1.0f;
            }

            const float gui_scale = ComputeGuiScale(viewport_size);
            if (g_last_gui_scale > 0.0f && std::fabs(gui_scale - g_last_gui_scale) < 0.01f) {
                return;
            }

            ImGuiStyle &style = ImGui::GetStyle();
            style             = g_imgui_base_style;
            style.ScaleAllSizes(gui_scale);

            // Maintain explicit touch-target floor instead of relying only on scaled padding.
            constexpr float kFallbackBaseFontPx  = 13.0f;
            constexpr float kMinFrameHeightAt720 = 34.0f;
            const float     min_frame_height_px  = kMinFrameHeightAt720 * gui_scale;

            const float font_px       = std::max(ImGui::GetFontSize(), kFallbackBaseFontPx * gui_scale);
            const float min_padding_y = std::max((min_frame_height_px - font_px) * 0.5f, 0.0f);
            style.FramePadding.y      = std::max(style.FramePadding.y, min_padding_y);

            if (style.WindowBorderHoverPadding < 1.0f) {
                style.WindowBorderHoverPadding = 1.0f;
            }

            // Slightly lower CPU/GPU cost and atlas complexity.
            style.AntiAliasedLines       = false;
            style.AntiAliasedLinesUseTex = false;
            style.AntiAliasedFill        = false;

            style.FontScaleMain = gui_scale;
            g_last_gui_scale    = gui_scale;
        }

        static void PushImGuiGamepadInputs(float dt_s) {
            if (!g_imgui_ctx_initialized) {
                return;
            }

            // Avoid calling into hid early; wait until the game has completed shell initialization.
            if (d3::g_ptMainRWindow == nullptr) {
                return;
            }

            NpadCombinedState st {};
            g_last_npad_valid = CombineNpadState(st, 0);
            g_last_npad       = st;

            const bool plus  = NpadButtonDown(st.buttons, nn::hid::NpadButton::Plus);
            const bool minus = NpadButtonDown(st.buttons, nn::hid::NpadButton::Minus);

            if (plus && minus) {
                g_overlay_toggle_hold_s += dt_s;
                if (g_overlay_toggle_hold_s >= 0.5f && g_overlay_toggle_armed) {
                    g_overlay.ToggleVisibleAndPersist();
                    PRINT("[imgui_overlay] overlay visible=%d", g_overlay.overlay_visible() ? 1 : 0);
                    g_overlay_toggle_armed = false;
                }
            } else {
                g_overlay_toggle_hold_s = 0.0f;
                g_overlay_toggle_armed  = true;
            }

            ImGuiIO &io = ImGui::GetIO();
            io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

            // Nintendo-friendly mapping: A=confirm, B=cancel.
            io.AddKeyEvent(ImGuiKey_GamepadFaceDown, NpadButtonDown(st.buttons, nn::hid::NpadButton::A));
            io.AddKeyEvent(ImGuiKey_GamepadFaceRight, NpadButtonDown(st.buttons, nn::hid::NpadButton::B));
            io.AddKeyEvent(ImGuiKey_GamepadFaceUp, NpadButtonDown(st.buttons, nn::hid::NpadButton::X));
            io.AddKeyEvent(ImGuiKey_GamepadFaceLeft, NpadButtonDown(st.buttons, nn::hid::NpadButton::Y));

            io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, NpadButtonDown(st.buttons, nn::hid::NpadButton::Left));
            io.AddKeyEvent(ImGuiKey_GamepadDpadRight, NpadButtonDown(st.buttons, nn::hid::NpadButton::Right));
            io.AddKeyEvent(ImGuiKey_GamepadDpadUp, NpadButtonDown(st.buttons, nn::hid::NpadButton::Up));
            io.AddKeyEvent(ImGuiKey_GamepadDpadDown, NpadButtonDown(st.buttons, nn::hid::NpadButton::Down));

            io.AddKeyEvent(ImGuiKey_GamepadStart, plus);
            io.AddKeyEvent(ImGuiKey_GamepadBack, minus);

            io.AddKeyEvent(ImGuiKey_GamepadL1, NpadButtonDown(st.buttons, nn::hid::NpadButton::L));
            io.AddKeyEvent(ImGuiKey_GamepadR1, NpadButtonDown(st.buttons, nn::hid::NpadButton::R));
            io.AddKeyEvent(ImGuiKey_GamepadL2, NpadButtonDown(st.buttons, nn::hid::NpadButton::ZL));
            io.AddKeyEvent(ImGuiKey_GamepadR2, NpadButtonDown(st.buttons, nn::hid::NpadButton::ZR));
            io.AddKeyEvent(ImGuiKey_GamepadL3, NpadButtonDown(st.buttons, nn::hid::NpadButton::StickL));
            io.AddKeyEvent(ImGuiKey_GamepadR3, NpadButtonDown(st.buttons, nn::hid::NpadButton::StickR));

            constexpr float deadzone = 0.20f;
            io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, false, 0.0f);
            io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, false, 0.0f);
            io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, false, 0.0f);
            io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, false, 0.0f);

            if (st.have_stick_r) {
                const float x = d3::gui2::NormalizeStickComponentS16(st.stick_r.X);
                const float y = d3::gui2::NormalizeStickComponentS16(st.stick_r.Y);
                io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft, x < -deadzone, std::max(0.0f, -x));
                io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, x > deadzone, std::max(0.0f, x));
                io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp, y > deadzone, std::max(0.0f, y));
                io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown, y < -deadzone, std::max(0.0f, -y));
            } else {
                io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft, false, 0.0f);
                io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, false, 0.0f);
                io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp, false, 0.0f);
                io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown, false, 0.0f);
            }
        }

        static void DetectOverlaySwipe(const ImVec2 &viewport_size) {
            if (!g_imgui_ctx_initialized) {
                return;
            }

            if (d3::g_ptMainRWindow == nullptr) {
                return;
            }

            if (!g_overlay.imgui_render_enabled()) {
                return;
            }

            if (g_overlay.overlay_visible()) {
                return;
            }

            d3::gui2::input::hid_block::ScopedHidPassthroughForOverlay const passthrough_guard;
            EnsureTouchInitialized();

            nn::hid::TouchScreenState<nn::hid::TouchStateCountMax> st {};
            nn::hid::GetTouchScreenState(&st);

            static bool s_swipe_active    = false;
            static bool s_swipe_triggered = false;
            static s32  s_start_x         = 0;
            static s32  s_start_y         = 0;

            const bool down = st.count > 0;
            if (!down) {
                s_swipe_active    = false;
                s_swipe_triggered = false;
                return;
            }

            if (!s_swipe_active) {
                s_swipe_active    = true;
                s_swipe_triggered = false;
                s_start_x         = st.touches[0].x;
                s_start_y         = st.touches[0].y;
            }

            if (s_swipe_triggered) {
                return;
            }

            constexpr float kTouchWidth        = 1280.0f;
            constexpr float kTouchHeight       = 720.0f;
            constexpr float kEdgeFrac          = 0.05f;
            constexpr float kTriggerFrac       = 0.20f;
            constexpr float kMaxVerticalFrac   = 0.25f;
            const float     edge_px            = kTouchWidth * kEdgeFrac;
            const float     trigger_px         = kTouchWidth * kTriggerFrac;
            const float     max_vertical_delta = kTouchHeight * kMaxVerticalFrac;

            if (s_start_x > static_cast<s32>(edge_px)) {
                return;
            }

            const s32 dx = st.touches[0].x - s_start_x;
            const s32 dy = st.touches[0].y - s_start_y;
            if (dx >= static_cast<s32>(trigger_px) &&
                std::abs(dy) <= static_cast<s32>(max_vertical_delta)) {
                g_overlay.set_overlay_visible_persist(true);
                s_swipe_triggered = true;
                PRINT_LINE("[imgui_overlay] Swipe opened overlay");
            }

            (void)viewport_size;
        }

        static auto PushImGuiTouchInputs(const ImVec2 &viewport_size) -> bool {
            if (!g_imgui_ctx_initialized) {
                return false;
            }

            // Avoid calling into hid early; wait until the game has completed shell initialization.
            if (d3::g_ptMainRWindow == nullptr) {
                return false;
            }

            static bool s_prev_down    = false;
            static s32  s_last_touch_x = 0;
            static s32  s_last_touch_y = 0;

            ImGuiIO &io = ImGui::GetIO();

            // Keep touch reads scoped so our own HID hooks don't block us while the overlay is visible.
            d3::gui2::input::hid_block::ScopedHidPassthroughForOverlay const passthrough_guard;

            EnsureTouchInitialized();

            nn::hid::TouchScreenState<nn::hid::TouchStateCountMax> st {};
            nn::hid::GetTouchScreenState(&st);

            const bool down     = st.count > 0;
            const bool pressed  = down && !s_prev_down;
            const bool released = !down && s_prev_down;
            const bool active   = down || released;

            const auto op_mode = nn::oe::GetOperationMode();
            if (op_mode != nn::oe::OperationMode_Handheld && !active) {
                s_prev_down = down;
                return false;
            }

            if (down) {
                s_last_touch_x = st.touches[0].x;
                s_last_touch_y = st.touches[0].y;
            }

            if (active) {
                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);

                // Touch coordinates are typically reported in 1280x720 space; rescale to the active viewport.
                float x = (static_cast<float>(s_last_touch_x) / 1280.0f) * viewport_size.x;
                float y = (static_cast<float>(s_last_touch_y) / 720.0f) * viewport_size.y;
                x       = std::clamp(x, 0.0f, viewport_size.x);
                y       = std::clamp(y, 0.0f, viewport_size.y);

                io.AddMousePosEvent(x, y);
                if (pressed) {
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
                } else if (released) {
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
                    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
                }
            }

            s_prev_down = down;

            return active;
        }

        static auto PushImGuiMouseInputs(const ImVec2 &viewport_size) -> bool {
            if (!g_imgui_ctx_initialized) {
                return false;
            }

            // Avoid calling into hid early; wait until the game has completed shell initialization.
            if (d3::g_ptMainRWindow == nullptr) {
                return false;
            }

            // Keep KBM init and reads scoped so our own HID hooks don't block us while the overlay is visible.
            d3::gui2::input::hid_block::ScopedHidPassthroughForOverlay const passthrough_guard;

            static bool s_mouse_init_tried = false;
            if (!s_mouse_init_tried) {
                s_mouse_init_tried = true;
                nn::hid::InitializeMouse();
            }

            nn::hid::MouseState st {};
            nn::hid::GetMouseState(&st);
            const bool mouse_connected = st.attributes.isBitSet(nn::hid::MouseAttribute::IsConnected);

            ImGuiIO &io = ImGui::GetIO();
            if (!mouse_connected) {
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
                return false;
            }
            io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);

            // Mouse/touch coordinates are typically reported in 1280x720 space; rescale to the active viewport.
            float x = std::clamp((static_cast<float>(st.x) / 1280.0f) * viewport_size.x, 0.0f, viewport_size.x);
            float y = std::clamp((static_cast<float>(st.y) / 720.0f) * viewport_size.y, 0.0f, viewport_size.y);

            io.AddMousePosEvent(x, y);

            const bool left   = st.buttons.isBitSet(nn::hid::MouseButton::Left);
            const bool right  = st.buttons.isBitSet(nn::hid::MouseButton::Right);
            const bool middle = st.buttons.isBitSet(nn::hid::MouseButton::Middle);

            io.AddMouseButtonEvent(ImGuiMouseButton_Left, left);
            io.AddMouseButtonEvent(ImGuiMouseButton_Right, right);
            io.AddMouseButtonEvent(ImGuiMouseButton_Middle, middle);

            if (st.wheelDeltaX != 0) {
                io.AddMouseWheelEvent(st.wheelDeltaX > 0 ? 0.5f : -0.5f, 0.0f);
            }
            if (st.wheelDeltaY != 0) {
                io.AddMouseWheelEvent(0.0f, st.wheelDeltaY > 0 ? 0.5f : -0.5f);
            }

            return mouse_connected;
        }

        static void PushImGuiKeyboardInputs() {
            if (!g_imgui_ctx_initialized) {
                return;
            }

            // Avoid calling into hid early; wait until the game has completed shell initialization.
            if (d3::g_ptMainRWindow == nullptr) {
                return;
            }

            d3::gui2::input::hid_block::ScopedHidPassthroughForOverlay const passthrough_guard;

            static bool s_keyboard_init_tried = false;
            if (!s_keyboard_init_tried) {
                s_keyboard_init_tried = true;
                nn::hid::InitializeKeyboard();
            }

            nn::hid::KeyboardState st {};
            nn::hid::GetKeyboardState(&st);

            ImGuiIO &io = ImGui::GetIO();

            const bool ctrl_left   = KeyboardKeyDown(st, nn::hid::KeyboardKey::LeftControl);
            const bool ctrl_right  = KeyboardKeyDown(st, nn::hid::KeyboardKey::RightControl);
            const bool shift_left  = KeyboardKeyDown(st, nn::hid::KeyboardKey::LeftShift);
            const bool shift_right = KeyboardKeyDown(st, nn::hid::KeyboardKey::RightShift);
            const bool alt_left    = KeyboardKeyDown(st, nn::hid::KeyboardKey::LeftAlt);
            const bool alt_right   = KeyboardKeyDown(st, nn::hid::KeyboardKey::RightAlt);
            const bool gui_left    = KeyboardKeyDown(st, nn::hid::KeyboardKey::LeftGui);
            const bool gui_right   = KeyboardKeyDown(st, nn::hid::KeyboardKey::RightGui);
            const bool ctrl_mod    = st.modifiers.isBitSet(nn::hid::KeyboardModifier::Control);
            const bool shift_mod   = st.modifiers.isBitSet(nn::hid::KeyboardModifier::Shift);
            const bool alt_mod     = st.modifiers.isBitSet(nn::hid::KeyboardModifier::LeftAlt) ||
                                 st.modifiers.isBitSet(nn::hid::KeyboardModifier::RightAlt);
            const bool gui_mod = st.modifiers.isBitSet(nn::hid::KeyboardModifier::Gui);

            io.AddKeyEvent(ImGuiKey_LeftCtrl, ctrl_left || (ctrl_mod && !ctrl_right));
            io.AddKeyEvent(ImGuiKey_RightCtrl, ctrl_right);
            io.AddKeyEvent(ImGuiKey_LeftShift, shift_left || (shift_mod && !shift_right));
            io.AddKeyEvent(ImGuiKey_RightShift, shift_right);
            io.AddKeyEvent(ImGuiKey_LeftAlt, alt_left || (alt_mod && !alt_right));
            io.AddKeyEvent(ImGuiKey_RightAlt, alt_right);
            io.AddKeyEvent(ImGuiKey_LeftSuper, gui_left || (gui_mod && !gui_right));
            io.AddKeyEvent(ImGuiKey_RightSuper, gui_right);

            auto set_key = [&](ImGuiKey key, nn::hid::KeyboardKey hid_key) -> void {
                io.AddKeyEvent(key, KeyboardKeyDown(st, hid_key));
            };

            set_key(ImGuiKey_Tab, nn::hid::KeyboardKey::Tab);
            set_key(ImGuiKey_LeftArrow, nn::hid::KeyboardKey::LeftArrow);
            set_key(ImGuiKey_RightArrow, nn::hid::KeyboardKey::RightArrow);
            set_key(ImGuiKey_UpArrow, nn::hid::KeyboardKey::UpArrow);
            set_key(ImGuiKey_DownArrow, nn::hid::KeyboardKey::DownArrow);
            set_key(ImGuiKey_Enter, nn::hid::KeyboardKey::Return);
            set_key(ImGuiKey_Escape, nn::hid::KeyboardKey::Escape);
            set_key(ImGuiKey_Backspace, nn::hid::KeyboardKey::Backspace);
            set_key(ImGuiKey_Space, nn::hid::KeyboardKey::Space);

            set_key(ImGuiKey_0, nn::hid::KeyboardKey::D0);
            set_key(ImGuiKey_1, nn::hid::KeyboardKey::D1);
            set_key(ImGuiKey_2, nn::hid::KeyboardKey::D2);
            set_key(ImGuiKey_3, nn::hid::KeyboardKey::D3);
            set_key(ImGuiKey_4, nn::hid::KeyboardKey::D4);
            set_key(ImGuiKey_5, nn::hid::KeyboardKey::D5);
            set_key(ImGuiKey_6, nn::hid::KeyboardKey::D6);
            set_key(ImGuiKey_7, nn::hid::KeyboardKey::D7);
            set_key(ImGuiKey_8, nn::hid::KeyboardKey::D8);
            set_key(ImGuiKey_9, nn::hid::KeyboardKey::D9);

            set_key(ImGuiKey_A, nn::hid::KeyboardKey::A);
            set_key(ImGuiKey_B, nn::hid::KeyboardKey::B);
            set_key(ImGuiKey_C, nn::hid::KeyboardKey::C);
            set_key(ImGuiKey_D, nn::hid::KeyboardKey::D);
            set_key(ImGuiKey_E, nn::hid::KeyboardKey::E);
            set_key(ImGuiKey_F, nn::hid::KeyboardKey::F);
            set_key(ImGuiKey_G, nn::hid::KeyboardKey::G);
            set_key(ImGuiKey_H, nn::hid::KeyboardKey::H);
            set_key(ImGuiKey_I, nn::hid::KeyboardKey::I);
            set_key(ImGuiKey_J, nn::hid::KeyboardKey::J);
            set_key(ImGuiKey_K, nn::hid::KeyboardKey::K);
            set_key(ImGuiKey_L, nn::hid::KeyboardKey::L);
            set_key(ImGuiKey_M, nn::hid::KeyboardKey::M);
            set_key(ImGuiKey_N, nn::hid::KeyboardKey::N);
            set_key(ImGuiKey_O, nn::hid::KeyboardKey::O);
            set_key(ImGuiKey_P, nn::hid::KeyboardKey::P);
            set_key(ImGuiKey_Q, nn::hid::KeyboardKey::Q);
            set_key(ImGuiKey_R, nn::hid::KeyboardKey::R);
            set_key(ImGuiKey_S, nn::hid::KeyboardKey::S);
            set_key(ImGuiKey_T, nn::hid::KeyboardKey::T);
            set_key(ImGuiKey_U, nn::hid::KeyboardKey::U);
            set_key(ImGuiKey_V, nn::hid::KeyboardKey::V);
            set_key(ImGuiKey_W, nn::hid::KeyboardKey::W);
            set_key(ImGuiKey_X, nn::hid::KeyboardKey::X);
            set_key(ImGuiKey_Y, nn::hid::KeyboardKey::Y);
            set_key(ImGuiKey_Z, nn::hid::KeyboardKey::Z);
        }

        static void OnPresent(NVNqueue *queue, NVNwindow *window, int texture_index) {
            (void)queue;
            (void)window;

            // Create the ImGui context on the render/present thread to avoid cross-thread visibility issues
            // with ImGui's global context pointer.
            if (!g_imgui_ctx_initialized) {
                IMGUI_CHECKVERSION();
                if (!d3::gui2::memory::imgui_alloc::TryConfigureImGuiAllocators()) {
                    return;
                }

                ImGui::CreateContext();
                g_imgui_ctx_initialized = true;

                ImGuiIO &io    = ImGui::GetIO();
                io.IniFilename = nullptr;
                io.LogFilename = nullptr;
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
                io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
                io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
                io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
                io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
                io.BackendPlatformName = "Nintendo Switch";

                g_overlay.OnImGuiContextCreated();
            }

            if (!g_backend_initialized) {
                d3::gui2::backend::nvn_hooks::LoadCppProcsOnce();

                auto *device  = d3::gui2::backend::nvn_hooks::GetDevice();
                auto *q       = d3::gui2::backend::nvn_hooks::GetQueue();
                auto *cmd_buf = d3::gui2::backend::nvn_hooks::GetCommandBuffer();
                if (device != nullptr && q != nullptr && cmd_buf != nullptr) {
                    ImguiNvnBackend::NvnBackendInitInfo init_info {};
                    init_info.device = reinterpret_cast<nvn::Device *>(device);
                    init_info.queue  = reinterpret_cast<nvn::Queue *>(q);
                    init_info.cmdBuf = reinterpret_cast<nvn::CommandBuffer *>(cmd_buf);
                    ImguiNvnBackend::InitBackend(init_info);
                    g_backend_initialized = true;
                    PRINT_LINE("[imgui_overlay] ImGui NVN backend initialized");
                }
            }

            if (!g_backend_initialized) {
                return;
            }

            if (kImGuiBringup_DrawText && g_imgui_ctx_initialized && !d3::gui2::fonts::font_loader::IsFontAtlasBuilt()) {
                PrepareFonts();
            }

            int w = d3::gui2::backend::nvn_hooks::GetCropWidth();
            int h = d3::gui2::backend::nvn_hooks::GetCropHeight();
            if (w <= 0 || h <= 0) {
                int tex_w = 0;
                int tex_h = 0;
                if (d3::gui2::backend::nvn_hooks::GetSwapchainTextureSize(texture_index, &tex_w, &tex_h)) {
                    w = tex_w;
                    h = tex_h;
                }
            }
            if (w <= 0 || h <= 0) {
                w = 1280;
                h = 720;
            }
            ImguiNvnBackend::getBackendData()->viewportSize = ImVec2(static_cast<float>(w), static_cast<float>(h));

            const auto *rt = d3::gui2::backend::nvn_hooks::GetSwapchainTextureForBackend(texture_index);
            ImguiNvnBackend::SetRenderTarget(rt);

            if (kImGuiBringup_AlwaysSubmitProofOfLife) {
                ImguiNvnBackend::SubmitProofOfLifeClear();
            }

            g_overlay.EnsureConfigLoaded();

            if (kImGuiBringup_DrawText && g_imgui_ctx_initialized && d3::gui2::fonts::font_loader::IsFontAtlasBuilt() && g_overlay.imgui_render_enabled()) {
                ImguiNvnBackend::newFrame();

                const ImVec2 viewport_size = ImguiNvnBackend::getBackendData()->viewportSize;
                UpdateImGuiStyleAndScale(viewport_size);
                DetectOverlaySwipe(viewport_size);
                ImGuiIO   &io              = ImGui::GetIO();
                const bool overlay_visible = g_overlay.overlay_visible();

                if (!overlay_visible) {
                    // Prevent stale hover state when the overlay is hidden.
                    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
                    io.MouseDrawCursor = false;
                }

                PushImGuiGamepadInputs(io.DeltaTime);

                if (overlay_visible) {
                    bool       mouse_connected = false;
                    const bool touch_active    = PushImGuiTouchInputs(viewport_size);
                    if (!touch_active) {
                        mouse_connected = PushImGuiMouseInputs(viewport_size);
                    }
                    PushImGuiKeyboardInputs();
                    io.MouseDrawCursor = touch_active || mouse_connected;
                }

                ImGui::NewFrame();

                g_overlay.UpdateFrame(
                    g_font_uploaded,
                    d3::gui2::backend::nvn_hooks::GetCropWidth(),
                    d3::gui2::backend::nvn_hooks::GetCropHeight(),
                    d3::gui2::backend::nvn_hooks::GetSwapchainTextureCount(),
                    viewport_size,
                    g_last_npad_valid,
                    static_cast<unsigned long long>(g_last_npad.buttons),
                    g_last_npad.stick_l.X,
                    g_last_npad.stick_l.Y,
                    g_last_npad.stick_r.X,
                    g_last_npad.stick_r.Y
                );

                d3::gui2::input::hid_block::SetBlockGamepadInputToGame(g_overlay.focus_state().should_block_game_input);
                d3::gui2::input::hid_block::SetAllowLeftStickPassthrough(g_overlay.focus_state().allow_left_stick_passthrough);

                ImGui::Render();
                g_overlay.AfterFrame();
                ImguiNvnBackend::renderDrawData(ImGui::GetDrawData());

                ImFontAtlas const   *fonts    = ImGui::GetIO().Fonts;
                ImTextureData const *font_tex = fonts ? fonts->TexData : nullptr;
                g_font_uploaded               = (font_tex != nullptr && font_tex->Status == ImTextureStatus_OK);
            }
        }
    }  // namespace

    auto GetGuiFocusState() -> const d3::gui2::ui::GuiFocusState & {
        return g_overlay.focus_state();
    }

    void PostOverlayNotification(const ImVec4 &color, float ttl_s, const char *fmt, ...) {
        auto *notifications = g_overlay.notifications_window();
        if (notifications == nullptr) {
            return;
        }

        std::array<char, 1024> buf {};
        va_list                ap;
        va_start(ap, fmt);
        vsnprintf(buf.data(), buf.size(), fmt, ap);
        va_end(ap);

        notifications->AddNotification(color, ttl_s, "%s", buf.data());
    }

    void Initialize() {
        if (g_hooks_installed) {
            return;
        }

        PRINT_LINE("[imgui_overlay] Initialize: begin");
        if (!d3::gui2::backend::nvn_hooks::InstallHooks(&OnPresent)) {
            PRINT_LINE("[imgui_overlay] ERROR: Failed to install NVN hooks");
            return;
        }

        d3::gui2::input::hid_block::InstallHidHooks();
        g_hooks_installed = true;

        // NOTE: Font preparation is intentionally deferred until after ShellInitialized (or, as a
        // fallback, from the NVN present hook once g_ptMainRWindow is non-null). Calling into the
        // default font decompression path during MainInit can stall early boot.

        PRINT_LINE("[imgui_overlay] Initialize: end");
    }

    void PrepareFonts() {
        if (!g_imgui_ctx_initialized) {
            return;
        }

        g_overlay.EnsureConfigLoaded();
        g_overlay.EnsureTranslationsLoaded();

        std::string desired_lang = g_overlay.translations_lang();
        if (desired_lang.empty()) {
            desired_lang = "en";
        }
        (void)d3::gui2::fonts::font_loader::PrepareFonts(g_overlay, desired_lang);
    }

    auto GetTitleFont() -> ImFont * {
        return d3::gui2::fonts::font_loader::GetTitleFont();
    }

    auto GetBodyFont() -> ImFont * {
        return d3::gui2::fonts::font_loader::GetBodyFont();
    }

}  // namespace d3::imgui_overlay
