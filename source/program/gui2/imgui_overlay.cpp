#include "program/gui2/imgui_overlay.hpp"

#include <array>
#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

#include "lib/hook/trampoline.hpp"
#include "lib/reloc/reloc.hpp"
#include "lib/util/sys/mem_layout.hpp"
#include "nn/hid.hpp"  // IWYU pragma: keep
#include "nn/oe.hpp"   // IWYU pragma: keep
#include "nn/os.hpp"   // IWYU pragma: keep
#include "program/romfs_assets.hpp"
#include "program/gui2/ui/overlay.hpp"
#include "program/gui2/ui/windows/notifications_window.hpp"
#include "program/gui2/imgui_glyph_ranges.hpp"
#include "program/d3/setting.hpp"
#include "symbols/common.hpp"

#include "imgui/imgui.h"
#include "imgui_backend/imgui_impl_nvn.hpp"

// Pull in only the C NVN API (avoid the NVN C++ funcptr shim headers which declare
// a conflicting `nvnBootstrapLoader` symbol).
#include "nvn/nvn.h"
// NVN C++ proc loader (fills pfncpp_* function pointers).
#include "nvn/nvn_CppFuncPtrImpl.h"

extern "C" auto nvnBootstrapLoader(const char *) -> PFNNVNGENERICFUNCPTRPROC;

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
    auto GetSharedFontLoadState(SharedFontType sharedFontType) noexcept
        -> SharedFontLoadState;
    auto GetSharedFontAddress(SharedFontType sharedFontType) noexcept -> const
        void *;
    auto GetSharedFontSize(SharedFontType sharedFontType) noexcept -> size_t;
}  // namespace nn::pl

namespace d3::imgui_overlay {
    namespace {

        using NvnDeviceGetProcAddressFn     = PFNNVNDEVICEGETPROCADDRESSPROC;
        using NvnQueuePresentTextureFn      = PFNNVNQUEUEPRESENTTEXTUREPROC;
        using NvnCommandBufferInitializeFn  = PFNNVNCOMMANDBUFFERINITIALIZEPROC;
        using NvnWindowBuilderSetTexturesFn = PFNNVNWINDOWBUILDERSETTEXTURESPROC;
        using NvnWindowSetCropFn            = PFNNVNWINDOWSETCROPPROC;

        constexpr bool kImGuiBringup_DrawText                = true;
        constexpr bool kImGuiBringup_AlwaysSubmitProofOfLife = false;

        NvnDeviceGetProcAddressFn    g_orig_get_proc           = nullptr;
        NvnQueuePresentTextureFn     g_orig_present            = nullptr;
        NvnCommandBufferInitializeFn g_orig_cmd_buf_initialize = nullptr;
        NVNdevice                   *g_device                  = nullptr;
        NVNqueue                    *g_queue                   = nullptr;

        // Captured game command buffer (preferred vs creating our own).
        NVNcommandBuffer *g_cmd_buf = nullptr;

        constexpr int                                           kMaxSwapchainTextures = 8;
        std::array<const nvn::Texture *, kMaxSwapchainTextures> g_swapchain_textures {};
        int                                                     g_swapchain_texture_count = 0;

        NvnWindowBuilderSetTexturesFn g_orig_window_builder_set_textures = nullptr;
        NvnWindowSetCropFn            g_orig_window_set_crop             = nullptr;

        int g_crop_w = 0;
        int g_crop_h = 0;

        void WindowBuilderSetTexturesHook(NVNwindowBuilder *builder, int num_textures, NVNtexture *const *textures) {
            if (textures != nullptr && num_textures > 0) {
                g_swapchain_texture_count = std::min(num_textures, kMaxSwapchainTextures);
                for (int i = 0; i < g_swapchain_texture_count; ++i) {
                    g_swapchain_textures[i] = reinterpret_cast<const nvn::Texture *>(textures[i]);
                }
            }

            if (g_orig_window_builder_set_textures != nullptr) {
                g_orig_window_builder_set_textures(builder, num_textures, textures);
            }
        }

        void WindowSetCropHook(NVNwindow *window, int x, int y, int w, int h) {
            g_crop_w = w;
            g_crop_h = h;

            if (g_orig_window_set_crop != nullptr) {
                g_orig_window_set_crop(window, x, y, w, h);
            }
        }

        auto ImGuiAlloc(size_t size, void *user_data) -> void * {
            (void)user_data;

            if (SigmaMemoryNew != nullptr) {
                // PRINT("Using SigmaMemoryNew for ImGuiAlloc(%zu)", size);
                return SigmaMemoryNew(size, 0x20, nullptr, false);
            }

            return std::malloc(size);
        }

        void ImGuiFree(void *ptr, void *user_data) {
            (void)user_data;
            if (ptr == nullptr) {
                return;
            }

            if (SigmaMemoryFree != nullptr) {
                SigmaMemoryFree(ptr, nullptr);
                return;
            }

            std::free(ptr);
        }

        bool    g_imgui_ctx_initialized = false;
        bool    g_imgui_allocators_set  = false;
        bool    g_backend_initialized   = false;
        bool    g_font_atlas_built      = false;
        bool    g_font_uploaded         = false;
        ImFont *g_font_body             = nullptr;
        ImFont *g_font_title            = nullptr;

        // NOTE: The backend expects NVN C++ wrapper types (nvn::Device/Queue/CommandBuffer).
        // We will wire those up once we hook stable NVN init entrypoints.
        bool g_hooks_installed = false;

        bool g_font_build_attempted   = false;
        bool g_font_build_in_progress = false;

        float                  g_overlay_toggle_hold_s = 0.0f;
        bool                   g_overlay_toggle_armed  = true;
        std::atomic<bool>      g_block_gamepad_input_to_game {false};
        bool                   g_hid_hooks_installed = false;
        std::atomic<int>       g_hid_passthrough_depth {0};
        std::atomic<uintptr_t> g_hid_passthrough_thread {0};

        static auto GetCurrentThreadToken() -> uintptr_t {
            const auto *thread = nn::os::GetCurrentThread();
            return reinterpret_cast<uintptr_t>(thread);
        }

        struct ScopedHidPassthroughForOverlay {
            ScopedHidPassthroughForOverlay() {
                const int prev = g_hid_passthrough_depth.fetch_add(1, std::memory_order_acq_rel);
                if (prev == 0) {
                    const auto token = GetCurrentThreadToken();
                    if (token != 0) {
                        g_hid_passthrough_thread.store(token, std::memory_order_release);
                    }
                }
            }
            ~ScopedHidPassthroughForOverlay() {
                const int prev = g_hid_passthrough_depth.fetch_sub(1, std::memory_order_acq_rel);
                if (prev <= 1) {
                    g_hid_passthrough_depth.store(0, std::memory_order_relaxed);
                    g_hid_passthrough_thread.store(0, std::memory_order_release);
                }
            }
        };

        template<typename TState>
        static void ClearNpadButtonsAndSticks(TState *st) {
            if (st == nullptr) {
                return;
            }
            st->mButtons      = {};
            st->mAnalogStickL = {};
            st->mAnalogStickR = {};
        }

        template<typename TState>
        static void ResetNpadState(TState *st) {
            if (st == nullptr) {
                return;
            }
            *st = {};
        }

        template<typename TState>
        static void ResetNpadStates(TState *out, int count) {
            if (out == nullptr || count <= 0) {
                return;
            }
            for (int i = 0; i < count; ++i) {
                ResetNpadState(out + i);
            }
        }

        static auto ShouldBlockGamepadInput() -> bool {
            if (!g_block_gamepad_input_to_game.load(std::memory_order_relaxed)) {
                return false;
            }

            if (g_hid_passthrough_depth.load(std::memory_order_relaxed) <= 0) {
                return true;
            }

            const auto passthrough_thread = g_hid_passthrough_thread.load(std::memory_order_acquire);
            if (passthrough_thread != 0 && passthrough_thread == GetCurrentThreadToken()) {
                return false;
            }

            return true;
        }

        struct NpadCombinedState {
            u64                       buttons = 0;
            nn::hid::AnalogStickState stick_l {};
            nn::hid::AnalogStickState stick_r {};
            bool                      have_stick_l = false;
            bool                      have_stick_r = false;
        };

        static auto NormalizeStickComponent(s32 v) -> float {
            constexpr float kMax = 32767.0f;
            float           out  = static_cast<float>(v) / kMax;
            if (out < -1.0f)
                out = -1.0f;
            if (out > 1.0f)
                out = 1.0f;
            return out;
        }

        static auto CombineNpadState(NpadCombinedState &out, uint port) -> bool {
            out = {};

            ScopedHidPassthroughForOverlay const passthrough_guard;

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
        static void UpdateImGuiStyleAndScale(ImVec2 viewport_size);

        static bool       g_imgui_style_initialized = false;
        static ImGuiStyle g_imgui_base_style {};
        static float      g_last_gui_scale = -1.0f;

        static auto ComputeGuiScale(ImVec2 viewport_size) -> float {
            constexpr float kMinViewportH = 720.0f;
            constexpr float kMaxViewportH = 1440.0f;
            constexpr float kMinScale     = 0.9f;
            constexpr float kMaxScale     = 1.3f;

            const float h = viewport_size.y;
            if (!(h > 0.0f)) {
                return kMaxScale;
            }

            const float t = std::clamp((h - kMinViewportH) / (kMaxViewportH - kMinViewportH), 0.0f, 1.0f);
            return kMinScale + t * (kMaxScale - kMinScale);
        }

        static void InitializeImGuiStyle() {
            ImGui::StyleColorsDark();

            ImGuiStyle &style       = ImGui::GetStyle();
            style.WindowPadding     = ImVec2(14.0f, 12.0f);
            style.FramePadding      = ImVec2(10.0f, 8.0f);
            style.ItemSpacing       = ImVec2(10.0f, 8.0f);
            style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
            style.IndentSpacing     = 18.0f;
            style.ScrollbarSize     = 18.0f;
            style.GrabMinSize       = 12.0f;
            style.TouchExtraPadding = ImVec2(6.0f, 6.0f);

            style.WindowRounding    = 6.0f;
            style.ChildRounding     = 4.0f;
            style.FrameRounding     = 4.0f;
            style.PopupRounding     = 4.0f;
            style.ScrollbarRounding = 9.0f;
            style.GrabRounding      = 4.0f;
            style.TabRounding       = 4.0f;

            style.WindowBorderSize = 1.0f;
            style.FrameBorderSize  = 1.0f;
            style.TabBorderSize    = 1.0f;

            // D3-ish palette: dark stone + gold accent.
            constexpr ImVec4 kAccent      = ImVec4(0.86f, 0.67f, 0.18f, 1.00f);
            constexpr ImVec4 kAccentHi    = ImVec4(0.95f, 0.80f, 0.28f, 1.00f);
            constexpr ImVec4 kBg          = ImVec4(0.08f, 0.08f, 0.09f, 0.94f);
            constexpr ImVec4 kPanel       = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
            constexpr ImVec4 kPanelHover  = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
            constexpr ImVec4 kPanelActive = ImVec4(0.26f, 0.26f, 0.29f, 1.00f);

            ImVec4 *colors                = style.Colors;
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

            g_imgui_base_style        = style;
            g_imgui_style_initialized = true;
        }

        static void UpdateImGuiStyleAndScale(ImVec2 viewport_size) {
            if (!g_imgui_style_initialized) {
                InitializeImGuiStyle();
                g_last_gui_scale = -1.0f;
            }

            constexpr float kTouchFontScale   = 1.00f;
            constexpr float kTouchButtonScale = 1.35f;

            const float base_scale = ComputeGuiScale(viewport_size);
            const float font_scale = base_scale * kTouchFontScale;
            if (g_last_gui_scale > 0.0f && std::fabs(font_scale - g_last_gui_scale) < 0.01f) {
                return;
            }

            ImGuiStyle &style = ImGui::GetStyle();
            style             = g_imgui_base_style;
            style.ScaleAllSizes(font_scale * 0.8f);
            style.FramePadding = ImVec2(style.FramePadding.x * kTouchButtonScale, style.FramePadding.y * kTouchButtonScale);

            style.FontScaleMain = font_scale;
            g_last_gui_scale    = font_scale;
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

            if (st.have_stick_l) {
                const float     x        = NormalizeStickComponent(st.stick_l.X);
                const float     y        = NormalizeStickComponent(st.stick_l.Y);
                constexpr float deadzone = 0.20f;
                io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, x < -deadzone, std::max(0.0f, -x));
                io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, x > deadzone, std::max(0.0f, x));
                io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, y > deadzone, std::max(0.0f, y));
                io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, y < -deadzone, std::max(0.0f, -y));
            } else {
                io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, false, 0.0f);
                io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, false, 0.0f);
                io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, false, 0.0f);
                io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, false, 0.0f);
            }
        }

        static auto PushImGuiTouchInputs(const ImVec2 &viewport_size) -> bool {
            if (!g_imgui_ctx_initialized) {
                return false;
            }

            // Avoid calling into hid early; wait until the game has completed shell initialization.
            if (d3::g_ptMainRWindow == nullptr) {
                return false;
            }

            static bool s_touch_init_tried = false;
            static s32  s_prev_touch_count = 0;
            static s32  s_last_touch_x     = 0;
            static s32  s_last_touch_y     = 0;
            static bool s_clear_hover_next = false;

            ImGuiIO &io = ImGui::GetIO();

            // Keep touch reads scoped so our own HID hooks don't block us while the overlay is visible.
            ScopedHidPassthroughForOverlay const passthrough_guard;

            if (!s_touch_init_tried) {
                s_touch_init_tried = true;
                nn::hid::InitializeTouchScreen();
            }

            nn::hid::TouchScreenState<nn::hid::TouchStateCountMax> st {};
            nn::hid::GetTouchScreenState(&st);

            const bool down        = st.count > 0;
            const bool released    = !down && s_prev_touch_count > 0;
            const bool active      = down || released;
            const bool clear_hover = s_clear_hover_next && !down;

            const auto op_mode = nn::oe::GetOperationMode();
            if (op_mode != nn::oe::OperationMode_Handheld && !active) {
                if (clear_hover) {
                    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
                    s_clear_hover_next = false;
                }
                return false;
            }

            if (down) {
                s_last_touch_x     = st.touches[0].x;
                s_last_touch_y     = st.touches[0].y;
                s_clear_hover_next = false;
            }

            if (active) {
                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);

                // Touch coordinates are typically reported in 1280x720 space; rescale to the active viewport.
                float x = (static_cast<float>(s_last_touch_x) / 1280.0f) * viewport_size.x;
                float y = (static_cast<float>(s_last_touch_y) / 720.0f) * viewport_size.y;
                x       = std::clamp(x, 0.0f, viewport_size.x);
                y       = std::clamp(y, 0.0f, viewport_size.y);

                io.AddMousePosEvent(x, y);
                io.AddMouseButtonEvent(ImGuiMouseButton_Left, down);
                io.MouseDrawCursor = (x >= 1.0f && y >= 1.0f);

                if (released) {
                    s_clear_hover_next = true;
                }
            }

            if (clear_hover) {
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
                s_clear_hover_next = false;
            }

            s_prev_touch_count = st.count;

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
            ScopedHidPassthroughForOverlay const passthrough_guard;

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

            ScopedHidPassthroughForOverlay const passthrough_guard;

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

        using GetNpadStateFullKeyFn  = void (*)(nn::hid::NpadFullKeyState *, uint const &);
        using GetNpadStateHandheldFn = void (*)(nn::hid::NpadHandheldState *, uint const &);
        using GetNpadStateJoyDualFn  = void (*)(nn::hid::NpadJoyDualState *, uint const &);
        using GetNpadStateJoyLeftFn  = void (*)(nn::hid::NpadJoyLeftState *, uint const &);
        using GetNpadStateJoyRightFn = void (*)(nn::hid::NpadJoyRightState *, uint const &);

        using GetNpadStatesFullKeyFn  = void (*)(nn::hid::NpadFullKeyState *, int, uint const &);
        using GetNpadStatesHandheldFn = void (*)(nn::hid::NpadHandheldState *, int, uint const &);
        using GetNpadStatesJoyDualFn  = void (*)(nn::hid::NpadJoyDualState *, int, uint const &);
        using GetNpadStatesJoyLeftFn  = void (*)(nn::hid::NpadJoyLeftState *, int, uint const &);
        using GetNpadStatesJoyRightFn = void (*)(nn::hid::NpadJoyRightState *, int, uint const &);

        using GetNpadStatesDetailFullKeyFn  = int (*)(int *, nn::hid::NpadFullKeyState *, int, uint const &);
        using GetNpadStatesDetailHandheldFn = int (*)(int *, nn::hid::NpadHandheldState *, int, uint const &);
        using GetNpadStatesDetailJoyDualFn  = int (*)(int *, nn::hid::NpadJoyDualState *, int, uint const &);
        using GetNpadStatesDetailJoyLeftFn  = int (*)(int *, nn::hid::NpadJoyLeftState *, int, uint const &);
        using GetNpadStatesDetailJoyRightFn = int (*)(int *, nn::hid::NpadJoyRightState *, int, uint const &);

        using GetMouseStateFn    = void (*)(nn::hid::MouseState *);
        using GetKeyboardStateFn = void (*)(nn::hid::KeyboardState *);

        static auto LookupSymbolAddress(const char *name) -> void * {
            for (int i = static_cast<int>(exl::util::ModuleIndex::Start);
                 i < static_cast<int>(exl::util::ModuleIndex::End);
                 ++i) {
                const auto index = static_cast<exl::util::ModuleIndex>(i);
                if (!exl::util::HasModule(index)) {
                    continue;
                }
                const auto *sym = exl::reloc::GetSymbol(index, name);
                if (sym == nullptr || sym->st_value == 0) {
                    continue;
                }
                const auto &info = exl::util::GetModuleInfo(index);
                return reinterpret_cast<void *>(info.m_Total.m_Start + sym->st_value);
            }
            return nullptr;
        }

        // clang-format off
#define DEFINE_HID_GET_STATE_HOOK(HOOK_NAME, STATE_TYPE)                           \
    HOOK_DEFINE_TRAMPOLINE(HOOK_NAME) {                                            \
        static auto Callback(nn::hid::STATE_TYPE *out, uint const &port) -> void { \
            if (ShouldBlockGamepadInput()) {                                       \
                ResetNpadState(out);                                               \
                return;                                                            \
            }                                                                      \
            Orig(out, port);                                                       \
        }                                                                          \
    };

#define DEFINE_HID_GET_STATES_HOOK(HOOK_NAME, STATE_TYPE)                               \
    HOOK_DEFINE_TRAMPOLINE(HOOK_NAME) {                                                 \
        static auto Callback(nn::hid::STATE_TYPE *out, int count, uint const &port)     \
            -> void {                                                                   \
            if (ShouldBlockGamepadInput()) {                                            \
                ResetNpadStates(out, count);                                            \
                return;                                                                 \
            }                                                                           \
            Orig(out, count, port);                                                     \
        }                                                                               \
    };

#define DEFINE_HID_DETAIL_GET_STATES_HOOK(HOOK_NAME, STATE_TYPE)                        \
    HOOK_DEFINE_TRAMPOLINE(HOOK_NAME) {                                                 \
        static auto Callback(int *out_count, nn::hid::STATE_TYPE *out, int count,       \
                             uint const &port) -> int {                                 \
            if (ShouldBlockGamepadInput()) {                                            \
                if (out_count != nullptr) {                                             \
                    *out_count = 0;                                                     \
                }                                                                       \
                ResetNpadStates(out, count);                                            \
                return 0;                                                               \
            }                                                                           \
            return Orig(out_count, out, count, port);                                   \
        }                                                                               \
    };
        // clang-format on

        DEFINE_HID_GET_STATE_HOOK(HidGetNpadStateFullKeyHook, NpadFullKeyState)
        DEFINE_HID_GET_STATE_HOOK(HidGetNpadStateHandheldHook, NpadHandheldState)
        DEFINE_HID_GET_STATE_HOOK(HidGetNpadStateJoyDualHook, NpadJoyDualState)
        DEFINE_HID_GET_STATE_HOOK(HidGetNpadStateJoyLeftHook, NpadJoyLeftState)
        DEFINE_HID_GET_STATE_HOOK(HidGetNpadStateJoyRightHook, NpadJoyRightState)

        DEFINE_HID_GET_STATES_HOOK(HidGetNpadStatesFullKeyHook, NpadFullKeyState)
        DEFINE_HID_GET_STATES_HOOK(HidGetNpadStatesHandheldHook, NpadHandheldState)
        DEFINE_HID_GET_STATES_HOOK(HidGetNpadStatesJoyDualHook, NpadJoyDualState)
        DEFINE_HID_GET_STATES_HOOK(HidGetNpadStatesJoyLeftHook, NpadJoyLeftState)
        DEFINE_HID_GET_STATES_HOOK(HidGetNpadStatesJoyRightHook, NpadJoyRightState)

        DEFINE_HID_DETAIL_GET_STATES_HOOK(HidDetailGetNpadStatesFullKeyHook, NpadFullKeyState)
        DEFINE_HID_DETAIL_GET_STATES_HOOK(HidDetailGetNpadStatesHandheldHook, NpadHandheldState)
        DEFINE_HID_DETAIL_GET_STATES_HOOK(HidDetailGetNpadStatesJoyDualHook, NpadJoyDualState)
        DEFINE_HID_DETAIL_GET_STATES_HOOK(HidDetailGetNpadStatesJoyLeftHook, NpadJoyLeftState)
        DEFINE_HID_DETAIL_GET_STATES_HOOK(HidDetailGetNpadStatesJoyRightHook, NpadJoyRightState)

        HOOK_DEFINE_TRAMPOLINE(HidGetMouseStateHook) {
            static auto Callback(nn::hid::MouseState *out) -> void {
                if (ShouldBlockGamepadInput()) {
                    if (out != nullptr) {
                        *out = {};
                    }
                    return;
                }
                Orig(out);
            }
        };

        HOOK_DEFINE_TRAMPOLINE(HidGetKeyboardStateHook) {
            static auto Callback(nn::hid::KeyboardState *out) -> void {
                if (ShouldBlockGamepadInput()) {
                    if (out != nullptr) {
                        *out = {};
                    }
                    return;
                }
                Orig(out);
            }
        };

#undef DEFINE_HID_DETAIL_GET_STATES_HOOK
#undef DEFINE_HID_GET_STATES_HOOK
#undef DEFINE_HID_GET_STATE_HOOK

        static void InstallHidHooks() {
            if (g_hid_hooks_installed) {
                return;
            }

            HidGetNpadStateFullKeyHook::InstallAtFuncPtr(static_cast<GetNpadStateFullKeyFn>(&nn::hid::GetNpadState));
            HidGetNpadStateHandheldHook::InstallAtFuncPtr(static_cast<GetNpadStateHandheldFn>(&nn::hid::GetNpadState));
            HidGetNpadStateJoyDualHook::InstallAtFuncPtr(static_cast<GetNpadStateJoyDualFn>(&nn::hid::GetNpadState));
            HidGetNpadStateJoyLeftHook::InstallAtFuncPtr(static_cast<GetNpadStateJoyLeftFn>(&nn::hid::GetNpadState));
            HidGetNpadStateJoyRightHook::InstallAtFuncPtr(static_cast<GetNpadStateJoyRightFn>(&nn::hid::GetNpadState));

            HidGetNpadStatesFullKeyHook::InstallAtFuncPtr(static_cast<GetNpadStatesFullKeyFn>(&nn::hid::GetNpadStates));
            HidGetNpadStatesHandheldHook::InstallAtFuncPtr(static_cast<GetNpadStatesHandheldFn>(&nn::hid::GetNpadStates));
            HidGetNpadStatesJoyDualHook::InstallAtFuncPtr(static_cast<GetNpadStatesJoyDualFn>(&nn::hid::GetNpadStates));
            HidGetNpadStatesJoyLeftHook::InstallAtFuncPtr(static_cast<GetNpadStatesJoyLeftFn>(&nn::hid::GetNpadStates));
            HidGetNpadStatesJoyRightHook::InstallAtFuncPtr(static_cast<GetNpadStatesJoyRightFn>(&nn::hid::GetNpadStates));

            HidGetMouseStateHook::InstallAtFuncPtr(static_cast<GetMouseStateFn>(&nn::hid::GetMouseState));
            HidGetKeyboardStateHook::InstallAtFuncPtr(static_cast<GetKeyboardStateFn>(&nn::hid::GetKeyboardState));

            struct DetailHookEntry {
                const char *name;
                void (*install)(uintptr_t);
            };

            const std::array<DetailHookEntry, 5> detail_hooks = {{
                {.name    = "_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_"
                            "16NpadFullKeyStateEiRKj",
                 .install = [](uintptr_t addr) -> void {
                     HidDetailGetNpadStatesFullKeyHook::InstallAtPtr(addr);
                 }},
                {.name    = "_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_"
                            "17NpadHandheldStateEiRKj",
                 .install = [](uintptr_t addr) -> void {
                     HidDetailGetNpadStatesHandheldHook::InstallAtPtr(addr);
                 }},
                {.name    = "_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_"
                            "16NpadJoyDualStateEiRKj",
                 .install = [](uintptr_t addr) -> void {
                     HidDetailGetNpadStatesJoyDualHook::InstallAtPtr(addr);
                 }},
                {.name    = "_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_"
                            "16NpadJoyLeftStateEiRKj",
                 .install = [](uintptr_t addr) -> void {
                     HidDetailGetNpadStatesJoyLeftHook::InstallAtPtr(addr);
                 }},
                {.name    = "_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_"
                            "17NpadJoyRightStateEiRKj",
                 .install = [](uintptr_t addr) -> void {
                     HidDetailGetNpadStatesJoyRightHook::InstallAtPtr(addr);
                 }},
            }};

            for (const auto &hook : detail_hooks) {
                void const *addr = LookupSymbolAddress(hook.name);
                if (addr == nullptr) {
                    PRINT("[imgui_overlay] nn::hid detail symbol missing: %s", hook.name);
                    continue;
                }
                hook.install(reinterpret_cast<uintptr_t>(addr));
            }

            g_hid_hooks_installed = true;
            PRINT_LINE("[imgui_overlay] nn::hid input hooks installed");
        }

        static auto CmdBufInitializeWrapper(NVNcommandBuffer *buffer, NVNdevice *device) -> NVNboolean {
            if (g_cmd_buf == nullptr && buffer != nullptr) {
                g_cmd_buf = buffer;
            }

            if (g_orig_cmd_buf_initialize == nullptr) {
                PRINT_LINE("[imgui_overlay] ERROR: CmdBufInitializeWrapper called without orig ptr");
                return 0;
            }

            const auto ret = g_orig_cmd_buf_initialize(buffer, device);

            return ret;
        }

        void QueuePresentTextureWrapper(NVNqueue *queue, NVNwindow *window, int texture_index) {
            static bool s_logged_present = false;
            if (!s_logged_present) {
                s_logged_present = true;
                PRINT_LINE("[imgui_overlay] Present hook entered");
            }

            if (g_queue == nullptr && queue != nullptr) {
                g_queue = queue;
            }

            // Create the ImGui context on the render/present thread to avoid cross-thread visibility issues
            // with ImGui's global context pointer.
            if (!g_imgui_ctx_initialized) {
                IMGUI_CHECKVERSION();
                if (!g_imgui_allocators_set) {
                    ImGui::SetAllocatorFunctions(ImGuiAlloc, ImGuiFree);
                    g_imgui_allocators_set = true;
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
            }

            if (!g_backend_initialized && g_device && g_queue) {
                if (g_orig_get_proc != nullptr) {
                    nvnLoadCPPProcs(reinterpret_cast<nvn::Device *>(g_device), reinterpret_cast<nvn::DeviceGetProcAddressFunc>(g_orig_get_proc));
                }

                if (g_orig_get_proc != nullptr && g_cmd_buf != nullptr) {
                    ImguiNvnBackend::NvnBackendInitInfo init_info {};
                    init_info.device = reinterpret_cast<nvn::Device *>(g_device);
                    init_info.queue  = reinterpret_cast<nvn::Queue *>(g_queue);
                    init_info.cmdBuf = reinterpret_cast<nvn::CommandBuffer *>(g_cmd_buf);
                    ImguiNvnBackend::InitBackend(init_info);
                    g_backend_initialized = true;
                    PRINT_LINE("[imgui_overlay] ImGui NVN backend initialized");
                }
            }

            if (g_backend_initialized) {
                // Build the font atlas only once the shell is initialized. This avoids blocking the early
                // boot path (MainInit) and prevents any surprise default-font work during NewFrame().
                if (kImGuiBringup_DrawText && g_imgui_ctx_initialized && !g_font_atlas_built) {
                    PrepareFonts();
                }

                // Dynamic viewport: prefer crop (if available), otherwise swapchain texture size.
                int w = g_crop_w;
                int h = g_crop_h;
                if (w <= 0 || h <= 0) {
                    if (texture_index >= 0 && texture_index < g_swapchain_texture_count &&
                        g_swapchain_textures[texture_index] != nullptr) {
                        w = g_swapchain_textures[texture_index]->GetWidth();
                        h = g_swapchain_textures[texture_index]->GetHeight();
                    }
                }
                if (w <= 0 || h <= 0) {
                    w = 1280;
                    h = 720;
                }
                ImguiNvnBackend::getBackendData()->viewportSize = ImVec2(static_cast<float>(w), static_cast<float>(h));

                // Provide the active swapchain texture to the backend so it can bind it
                // as the render target for proof-of-life draws.
                if (texture_index >= 0 && texture_index < g_swapchain_texture_count) {
                    ImguiNvnBackend::SetRenderTarget(g_swapchain_textures[texture_index]);
                } else {
                    ImguiNvnBackend::SetRenderTarget(nullptr);
                }

                if (kImGuiBringup_AlwaysSubmitProofOfLife) {
                    ImguiNvnBackend::SubmitProofOfLifeClear();
                }

                g_overlay.EnsureConfigLoaded();

                g_block_gamepad_input_to_game.store(g_overlay.imgui_render_enabled() && g_overlay.overlay_visible(), std::memory_order_relaxed);

                if (kImGuiBringup_DrawText && g_imgui_ctx_initialized && g_font_atlas_built && g_overlay.imgui_render_enabled()) {
                    ImguiNvnBackend::newFrame();

                    const ImVec2 viewport_size = ImguiNvnBackend::getBackendData()->viewportSize;
                    UpdateImGuiStyleAndScale(viewport_size);
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
                        g_crop_w,
                        g_crop_h,
                        g_swapchain_texture_count,
                        viewport_size,
                        g_last_npad_valid,
                        static_cast<unsigned long long>(g_last_npad.buttons),
                        g_last_npad.stick_l.X,
                        g_last_npad.stick_l.Y,
                        g_last_npad.stick_r.X,
                        g_last_npad.stick_r.Y
                    );

                    ImGui::Render();
                    ImguiNvnBackend::renderDrawData(ImGui::GetDrawData());
                    ImFontAtlas const   *fonts = ImGui::GetIO().Fonts;
                    ImTextureData const *font_tex =
                        fonts ? fonts->TexData : nullptr;
                    g_font_uploaded = (font_tex != nullptr && font_tex->Status == ImTextureStatus_OK);
                }
            }

            if (g_orig_present != nullptr) {
                g_orig_present(queue, window, texture_index);
            }
        }

        // Hook nvnDeviceGetProcAddress directly (works even when nvnBootstrapLoader isn't in dynsym).
        HOOK_DEFINE_TRAMPOLINE(DeviceGetProcAddressHook) {
            static auto Callback(const NVNdevice *device, const char *name) -> PFNNVNGENERICFUNCPTRPROC {
                if (g_device == nullptr && device != nullptr) {
                    g_device = const_cast<NVNdevice *>(device);
                }

                PFNNVNGENERICFUNCPTRPROC fn = Orig(device, name);
                if (fn == nullptr || name == nullptr) {
                    return fn;
                }

                if (std::strcmp(name, "nvnDeviceGetProcAddress") == 0) {
                    g_orig_get_proc = reinterpret_cast<NvnDeviceGetProcAddressFn>(fn);
                }

                // Hook init entrypoints using the C ABI.
                if (std::strcmp(name, "nvnCommandBufferInitialize") == 0) {
                    g_orig_cmd_buf_initialize = reinterpret_cast<NvnCommandBufferInitializeFn>(fn);
                    return reinterpret_cast<PFNNVNGENERICFUNCPTRPROC>(&CmdBufInitializeWrapper);
                }

                if (std::strcmp(name, "nvnWindowBuilderSetTextures") == 0) {
                    g_orig_window_builder_set_textures = reinterpret_cast<NvnWindowBuilderSetTexturesFn>(fn);
                    return reinterpret_cast<PFNNVNGENERICFUNCPTRPROC>(&WindowBuilderSetTexturesHook);
                }

                if (std::strcmp(name, "nvnWindowSetCrop") == 0) {
                    g_orig_window_set_crop = reinterpret_cast<NvnWindowSetCropFn>(fn);
                    return reinterpret_cast<PFNNVNGENERICFUNCPTRPROC>(&WindowSetCropHook);
                }

                if (std::strcmp(name, "nvnQueuePresentTexture") == 0) {
                    g_orig_present = reinterpret_cast<NvnQueuePresentTextureFn>(fn);
                    return reinterpret_cast<PFNNVNGENERICFUNCPTRPROC>(&QueuePresentTextureWrapper);
                }

                return fn;
            }
        };

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

        // Resolve symbol via bootstrap loader (most reliable path in NVN environments).
        const auto get_proc = nvnBootstrapLoader("nvnDeviceGetProcAddress");
        if (get_proc == nullptr) {
            PRINT_LINE("[imgui_overlay] ERROR: Failed to resolve nvnDeviceGetProcAddress via nvnBootstrapLoader");
            return;
        }

        g_orig_get_proc = reinterpret_cast<NvnDeviceGetProcAddressFn>(get_proc);
        DeviceGetProcAddressHook::InstallAtPtr(reinterpret_cast<uintptr_t>(get_proc));
        g_hooks_installed = true;

        InstallHidHooks();

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

        static std::string s_font_lang;
        static std::string s_ranges_lang;
        if (g_font_atlas_built && s_font_lang != desired_lang) {
            PRINT("[imgui_overlay] Font lang changed (%s -> %s); rebuilding atlas", s_font_lang.c_str(), desired_lang.c_str());
            ImGuiIO &io = ImGui::GetIO();
            io.Fonts->Clear();
            g_font_atlas_built     = false;
            g_font_uploaded        = false;
            g_font_build_attempted = false;
            g_font_body            = nullptr;
            g_font_title           = nullptr;
            s_ranges_lang.clear();
        }

        if (g_font_atlas_built) {
            return;
        }

        // Heuristic for "boot is far enough along" to do heavier work.
        // This matches how we gate HID calls elsewhere in this file.
        if (d3::g_ptMainRWindow == nullptr) {
            static bool s_logged_defer = false;
            if (!s_logged_defer) {
                PRINT_LINE("[imgui_overlay] PrepareFonts: deferring until ShellInitialized");
                s_logged_defer = true;
            }
            return;
        }

        if (g_font_build_attempted) {
            return;
        }

        // Avoid re-entrancy if PrepareFonts() is called from multiple threads/hooks.
        // (Keep this lock-free/simple: Switch builds may not provide libatomic helpers for 1-byte atomics.)
        if (g_font_build_in_progress) {
            return;
        }
        g_font_build_in_progress = true;
        struct ScopedClearInProgress {
            ~ScopedClearInProgress() { g_font_build_in_progress = false; }
        } const clear_in_progress {};

        g_font_build_attempted = true;

        ImGuiIO &io = ImGui::GetIO();
        if (io.Fonts == nullptr) {
            PRINT_LINE("[imgui_overlay] ERROR: ImGuiIO::Fonts is null; cannot build font atlas");
            return;
        }

        PRINT_LINE("[imgui_overlay] Preparing ImGui font atlas...");
        io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
        ImFontConfig font_cfg {};
        font_cfg.Name[0]               = 'd';
        font_cfg.Name[1]               = '3';
        font_cfg.Name[2]               = '\0';
        constexpr float kFontSizeBody  = 16.0f;
        constexpr float kFontSizeTitle = 18.0f;
        // Build for docked readability (we apply a runtime scale for 720p).
        font_cfg.SizePixels  = kFontSizeBody;
        font_cfg.OversampleH = 2;
        font_cfg.OversampleV = 2;
        static ImVector<ImWchar> s_font_ranges;
        static bool              s_font_ranges_built = false;
        if (!s_font_ranges_built || s_ranges_lang != desired_lang) {
            ImFontGlyphRangesBuilder builder;
            builder.AddRanges(glyph_ranges::GetDefault());
            builder.AddRanges(glyph_ranges::GetKorean());
            if (desired_lang == "zh") {
                builder.AddRanges(glyph_ranges::GetChineseFull());
            } else {
                builder.AddRanges(glyph_ranges::GetChineseSimplifiedCommon());
            }
            builder.AddRanges(glyph_ranges::GetJapanese());
            builder.AddRanges(glyph_ranges::GetCyrillic());
            builder.BuildRanges(&s_font_ranges);
            s_font_ranges_built = true;
            s_ranges_lang       = desired_lang;
        }
        font_cfg.GlyphRanges = s_font_ranges.Data;

        ImFont const *font_body        = nullptr;
        ImFont const *font_title       = nullptr;
        bool          used_shared_font = false;

        const std::array<ImWchar, 3> nvn_ext_ranges = {0xE000, 0xE152, 0};

        const bool is_chinese = (desired_lang == "zh");

        const nn::pl::SharedFontType shared_base =
            is_chinese ? nn::pl::SharedFontType_ChineseSimple : nn::pl::SharedFontType_Standard;
        const nn::pl::SharedFontType shared_ext =
            is_chinese ? nn::pl::SharedFontType_ChineseSimpleExtension : nn::pl::SharedFontType_NintendoExtension;

        nn::pl::RequestSharedFontLoad(shared_base);
        nn::pl::RequestSharedFontLoad(shared_ext);
        nn::pl::RequestSharedFontLoad(nn::pl::SharedFontType_NintendoExtension);

        const bool shared_base_ready =
            nn::pl::GetSharedFontLoadState(shared_base) == nn::pl::SharedFontLoadState_Loaded;
        const bool shared_ext_ready =
            nn::pl::GetSharedFontLoadState(shared_ext) == nn::pl::SharedFontLoadState_Loaded;
        const bool shared_nvn_ext_ready =
            nn::pl::GetSharedFontLoadState(nn::pl::SharedFontType_NintendoExtension) == nn::pl::SharedFontLoadState_Loaded;

        if (shared_base_ready) {
            const void  *shared_font = nn::pl::GetSharedFontAddress(shared_base);
            const size_t shared_size = nn::pl::GetSharedFontSize(shared_base);
            if (shared_font != nullptr && shared_size > 0) {
                font_cfg.FontDataOwnedByAtlas = false;
                font_body                     = io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(shared_font), static_cast<int>(shared_size), font_cfg.SizePixels, &font_cfg);
                used_shared_font              = (font_body != nullptr);
                if (used_shared_font) {
                    PRINT("[imgui_overlay] Loaded Nintendo shared system font (base=%d)", static_cast<int>(shared_base));
                } else {
                    PRINT_LINE("[imgui_overlay] ERROR: AddFontFromMemoryTTF failed for shared base font");
                }
            } else {
                PRINT_LINE("[imgui_overlay] Shared base font unavailable; address/size invalid");
            }
        } else {
            PRINT_LINE("[imgui_overlay] Shared base font not yet loaded; falling back");
        }

        if (used_shared_font && shared_ext_ready) {
            const void  *shared_ext_font = nn::pl::GetSharedFontAddress(shared_ext);
            const size_t shared_ext_size = nn::pl::GetSharedFontSize(shared_ext);
            if (shared_ext_font != nullptr && shared_ext_size > 0) {
                ImFontConfig ext_cfg         = font_cfg;
                ext_cfg.MergeMode            = true;
                ext_cfg.FontDataOwnedByAtlas = false;
                ext_cfg.GlyphRanges          = is_chinese ? s_font_ranges.Data : nvn_ext_ranges.data();
                if (io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(shared_ext_font), static_cast<int>(shared_ext_size), ext_cfg.SizePixels, &ext_cfg) == nullptr) {
                    PRINT_LINE("[imgui_overlay] ERROR: AddFontFromMemoryTTF failed for shared extension font");
                } else {
                    PRINT("[imgui_overlay] Loaded Nintendo shared system font (ext=%d)", static_cast<int>(shared_ext));
                }
            } else {
                PRINT_LINE("[imgui_overlay] Shared extension font unavailable; address/size invalid");
            }
        }

        if (used_shared_font && shared_nvn_ext_ready) {
            const void  *nvn_ext_font = nn::pl::GetSharedFontAddress(nn::pl::SharedFontType_NintendoExtension);
            const size_t nvn_ext_size = nn::pl::GetSharedFontSize(nn::pl::SharedFontType_NintendoExtension);
            if (nvn_ext_font != nullptr && nvn_ext_size > 0) {
                ImFontConfig ext_cfg         = font_cfg;
                ext_cfg.MergeMode            = true;
                ext_cfg.FontDataOwnedByAtlas = false;
                ext_cfg.GlyphRanges          = nvn_ext_ranges.data();
                if (io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(nvn_ext_font), static_cast<int>(nvn_ext_size), ext_cfg.SizePixels, &ext_cfg) == nullptr) {
                    PRINT_LINE("[imgui_overlay] ERROR: AddFontFromMemoryTTF failed for shared Nintendo extension");
                } else {
                    PRINT_LINE("[imgui_overlay] Loaded Nintendo shared system font (NintendoExtension)");
                }
            }
        }

        if (used_shared_font) {
            ImFontConfig title_cfg         = font_cfg;
            title_cfg.SizePixels           = kFontSizeTitle;
            title_cfg.FontDataOwnedByAtlas = false;
            font_title                     = io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(nn::pl::GetSharedFontAddress(shared_base)), static_cast<int>(nn::pl::GetSharedFontSize(shared_base)), title_cfg.SizePixels, &title_cfg);
            if (font_title == nullptr) {
                PRINT_LINE("[imgui_overlay] ERROR: AddFontFromMemoryTTF failed for shared title font");
            }

            if (font_title != nullptr && shared_ext_ready) {
                const void  *shared_ext_font = nn::pl::GetSharedFontAddress(shared_ext);
                const size_t shared_ext_size = nn::pl::GetSharedFontSize(shared_ext);
                if (shared_ext_font != nullptr && shared_ext_size > 0) {
                    ImFontConfig ext_cfg         = title_cfg;
                    ext_cfg.MergeMode            = true;
                    ext_cfg.FontDataOwnedByAtlas = false;
                    ext_cfg.GlyphRanges          = is_chinese ? s_font_ranges.Data : nvn_ext_ranges.data();
                    (void)io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(shared_ext_font), static_cast<int>(shared_ext_size), ext_cfg.SizePixels, &ext_cfg);
                }
            }

            if (font_title != nullptr && shared_nvn_ext_ready) {
                const void  *nvn_ext_font = nn::pl::GetSharedFontAddress(nn::pl::SharedFontType_NintendoExtension);
                const size_t nvn_ext_size = nn::pl::GetSharedFontSize(nn::pl::SharedFontType_NintendoExtension);
                if (nvn_ext_font != nullptr && nvn_ext_size > 0) {
                    ImFontConfig ext_cfg         = title_cfg;
                    ext_cfg.MergeMode            = true;
                    ext_cfg.FontDataOwnedByAtlas = false;
                    ext_cfg.GlyphRanges          = nvn_ext_ranges.data();
                    (void)io.Fonts->AddFontFromMemoryTTF(const_cast<void *>(nvn_ext_font), static_cast<int>(nvn_ext_size), ext_cfg.SizePixels, &ext_cfg);
                }
            }
        }

        if (!used_shared_font) {
            ImFontConfig body_cfg  = font_cfg;
            body_cfg.SizePixels    = kFontSizeBody;
            font_body              = io.Fonts->AddFontDefault(&body_cfg);
            ImFontConfig title_cfg = font_cfg;
            title_cfg.SizePixels   = kFontSizeTitle;
            font_title             = io.Fonts->AddFontDefault(&title_cfg);
        }

        g_font_body  = const_cast<ImFont *>(font_body);
        g_font_title = const_cast<ImFont *>(font_title != nullptr ? font_title : font_body);

        g_font_atlas_built = true;
        s_font_lang        = desired_lang;
        PRINT_LINE("[imgui_overlay] ImGui font atlas prepared");
    }

    auto GetTitleFont() -> ImFont * {
        return g_font_title;
    }

    auto GetBodyFont() -> ImFont * {
        return g_font_body;
    }

}  // namespace d3::imgui_overlay
