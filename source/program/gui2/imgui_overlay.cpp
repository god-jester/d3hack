#include "program/gui2/imgui_overlay.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

#include "lib/hook/trampoline.hpp"
#include "lib/reloc/reloc.hpp"
#include "lib/util/sys/mem_layout.hpp"
#include "nn/fs.hpp"
#include "nn/hid.hpp"
#include "nn/os.hpp"
#include "program/romfs_assets.hpp"
#include "program/gui2/ui/overlay.hpp"
#include "program/gui2/ui/windows/notifications_window.hpp"
#include "program/d3/setting.hpp"
#include "symbols/common.hpp"

#include "imgui/imgui.h"
#include "imgui_backend/imgui_impl_nvn.hpp"

// Pull in only the C NVN API (avoid the NVN C++ funcptr shim headers which declare
// a conflicting `nvnBootstrapLoader` symbol).
#include "nvn/nvn.h"
// NVN C++ proc loader (fills pfncpp_* function pointers).
#include "nvn/nvn_CppFuncPtrImpl.h"

extern "C" PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *);

struct RWindow;
namespace d3 {
    extern ::RWindow *g_ptMainRWindow;
}

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

        constexpr int       kMaxSwapchainTextures = 8;
        const nvn::Texture *g_swapchain_textures[kMaxSwapchainTextures] {};
        int                 g_swapchain_texture_count = 0;

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

        void *ImGuiAlloc(size_t size, void *user_data) {
            (void)user_data;

            if (SigmaMemoryNew != nullptr) {
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

        bool ReadFileToImGuiBuffer(const char *path, unsigned char **out_data, size_t *out_size, size_t max_size) {
            if (out_data == nullptr || out_size == nullptr) {
                return false;
            }
            *out_data = nullptr;
            *out_size = 0;

            if (path == nullptr || path[0] == '\0') {
                return false;
            }

            nn::fs::FileHandle fh {};
            auto               rc = nn::fs::OpenFile(&fh, path, nn::fs::OpenMode_Read);
            if (R_FAILED(rc)) {
                return false;
            }

            long size = 0;
            rc        = nn::fs::GetFileSize(&size, fh);
            if (R_FAILED(rc) || size <= 0) {
                nn::fs::CloseFile(fh);
                return false;
            }

            const size_t size_st = static_cast<size_t>(size);
            if (size_st > max_size) {
                nn::fs::CloseFile(fh);
                return false;
            }

            auto *buffer = static_cast<unsigned char *>(IM_ALLOC(size_st));
            if (buffer == nullptr) {
                nn::fs::CloseFile(fh);
                return false;
            }

            rc = nn::fs::ReadFile(fh, 0, buffer, static_cast<ulong>(size_st));
            nn::fs::CloseFile(fh);
            if (R_FAILED(rc)) {
                IM_FREE(buffer);
                return false;
            }

            *out_data = buffer;
            *out_size = size_st;
            return true;
        }

        bool g_imgui_ctx_initialized = false;
        bool g_imgui_allocators_set  = false;
        bool g_backend_initialized   = false;
        bool g_font_atlas_built      = false;
        bool g_font_uploaded         = false;

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

        static uintptr_t GetCurrentThreadToken() {
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

        static bool ShouldBlockGamepadInput() {
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

        static float NormalizeStickComponent(s32 v) {
            constexpr float kMax = 32767.0f;
            float           out  = static_cast<float>(v) / kMax;
            if (out < -1.0f)
                out = -1.0f;
            if (out > 1.0f)
                out = 1.0f;
            return out;
        }

        static bool CombineNpadState(NpadCombinedState &out, uint port) {
            out = {};

            ScopedHidPassthroughForOverlay passthrough_guard;

            nn::hid::NpadFullKeyState  full {};
            nn::hid::NpadHandheldState handheld {};
            nn::hid::NpadJoyDualState  joy_dual {};
            nn::hid::NpadJoyLeftState  joy_left {};
            nn::hid::NpadJoyRightState joy_right {};

            nn::hid::GetNpadState(&full, port);
            nn::hid::GetNpadState(&handheld, port);
            nn::hid::GetNpadState(&joy_dual, port);
            nn::hid::GetNpadState(&joy_left, port);
            nn::hid::GetNpadState(&joy_right, port);

            out.buttons |= full.mButtons.field[0];
            out.buttons |= handheld.mButtons.field[0];
            out.buttons |= joy_dual.mButtons.field[0];
            out.buttons |= joy_left.mButtons.field[0];
            out.buttons |= joy_right.mButtons.field[0];

            auto consider_stick = [](bool &have, nn::hid::AnalogStickState &dst, const nn::hid::AnalogStickState &src) {
                const auto mag     = static_cast<u64>(std::abs(src.X)) + static_cast<u64>(std::abs(src.Y));
                const auto dst_mag = static_cast<u64>(std::abs(dst.X)) + static_cast<u64>(std::abs(dst.Y));
                if (!have || mag > dst_mag) {
                    dst  = src;
                    have = true;
                }
            };

            consider_stick(out.have_stick_l, out.stick_l, full.mAnalogStickL);
            consider_stick(out.have_stick_l, out.stick_l, handheld.mAnalogStickL);
            consider_stick(out.have_stick_l, out.stick_l, joy_dual.mAnalogStickL);
            consider_stick(out.have_stick_l, out.stick_l, joy_left.mAnalogStickL);
            consider_stick(out.have_stick_l, out.stick_l, joy_right.mAnalogStickL);

            consider_stick(out.have_stick_r, out.stick_r, full.mAnalogStickR);
            consider_stick(out.have_stick_r, out.stick_r, handheld.mAnalogStickR);
            consider_stick(out.have_stick_r, out.stick_r, joy_dual.mAnalogStickR);
            consider_stick(out.have_stick_r, out.stick_r, joy_left.mAnalogStickR);
            consider_stick(out.have_stick_r, out.stick_r, joy_right.mAnalogStickR);

            return out.buttons != 0 || out.have_stick_l || out.have_stick_r;
        }

        static bool NpadButtonDown(u64 buttons, nn::hid::NpadButton button) {
            const auto bit = static_cast<u64>(button);
            return (buttons & (1ULL << bit)) != 0;
        }

        static d3::gui2::ui::Overlay g_overlay {};
        static NpadCombinedState     g_last_npad {};
        static bool                  g_last_npad_valid = false;

        static void PushImGuiGamepadInputs(float dt_s);
        static void PushImGuiMouseInputs(const ImVec2 &viewport_size);
        static void UpdateImGuiStyleAndScale(ImVec2 viewport_size);

        static bool       g_imgui_style_initialized = false;
        static ImGuiStyle g_imgui_base_style {};
        static float      g_last_gui_scale = -1.0f;

        static float ComputeGuiScale(ImVec2 viewport_size) {
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

            ImGuiStyle &style      = ImGui::GetStyle();
            style.WindowPadding    = ImVec2(14.0f, 12.0f);
            style.FramePadding     = ImVec2(10.0f, 6.0f);
            style.ItemSpacing      = ImVec2(10.0f, 8.0f);
            style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
            style.IndentSpacing    = 18.0f;
            style.ScrollbarSize    = 18.0f;
            style.GrabMinSize      = 12.0f;

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

            colors[ImGuiCol_Tab]                = ImVec4(0.13f, 0.13f, 0.14f, 1.00f);
            colors[ImGuiCol_TabHovered]         = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.55f);
            colors[ImGuiCol_TabActive]          = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_TabUnfocused]       = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
            colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);

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

            const float scale = ComputeGuiScale(viewport_size);
            if (g_last_gui_scale > 0.0f && std::fabs(scale - g_last_gui_scale) < 0.01f) {
                return;
            }

            ImGuiStyle &style = ImGui::GetStyle();
            style             = g_imgui_base_style;
            style.ScaleAllSizes(scale);

            ImGui::GetIO().FontGlobalScale = scale;
            g_last_gui_scale               = scale;
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

        static void PushImGuiMouseInputs(const ImVec2 &viewport_size) {
            if (!g_imgui_ctx_initialized) {
                return;
            }

            // Avoid calling into hid early; wait until the game has completed shell initialization.
            if (d3::g_ptMainRWindow == nullptr) {
                return;
            }

            // Keep KBM init and reads scoped so our own HID hooks don't block us while the overlay is visible.
            ScopedHidPassthroughForOverlay passthrough_guard;

            static bool s_mouse_init_tried = false;
            if (!s_mouse_init_tried) {
                s_mouse_init_tried = true;
                nn::hid::InitializeMouse();
            }

            nn::hid::MouseState st {};
            nn::hid::GetMouseState(&st);

            ImGuiIO &io = ImGui::GetIO();

            // Mouse/touch coordinates are typically reported in 1280x720 space; rescale to the active viewport.
            float x = (static_cast<float>(st.x) / 1280.0f) * viewport_size.x;
            float y = (static_cast<float>(st.y) / 720.0f) * viewport_size.y;
            if (x < 0.0f)
                x = 0.0f;
            if (y < 0.0f)
                y = 0.0f;
            if (x > viewport_size.x)
                x = viewport_size.x;
            if (y > viewport_size.y)
                y = viewport_size.y;

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

        using GetMouseStateFn = void (*)(nn::hid::MouseState *);

        static void *LookupSymbolAddress(const char *name) {
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

#define DEFINE_HID_GET_STATE_HOOK(HOOK_NAME, STATE_TYPE)                    \
    HOOK_DEFINE_TRAMPOLINE(HOOK_NAME) {                                     \
        static void Callback(nn::hid::STATE_TYPE * out, uint const &port) { \
            if (ShouldBlockGamepadInput()) {                                \
                ResetNpadState(out);                                        \
    return;                                                                 \
    }                                                                       \
    Orig(out, port);                                                        \
    }                                                                       \
    }                                                                       \
    ;

#define DEFINE_HID_GET_STATES_HOOK(HOOK_NAME, STATE_TYPE)                              \
    HOOK_DEFINE_TRAMPOLINE(HOOK_NAME) {                                                \
        static void Callback(nn::hid::STATE_TYPE * out, int count, uint const &port) { \
            if (ShouldBlockGamepadInput()) {                                           \
                ResetNpadStates(out, count);                                           \
    return;                                                                            \
    }                                                                                  \
    Orig(out, count, port);                                                            \
    }                                                                                  \
    }                                                                                  \
    ;

#define DEFINE_HID_DETAIL_GET_STATES_HOOK(HOOK_NAME, STATE_TYPE)                                     \
    HOOK_DEFINE_TRAMPOLINE(HOOK_NAME) {                                                              \
        static int Callback(int *out_count, nn::hid::STATE_TYPE *out, int count, uint const &port) { \
            if (ShouldBlockGamepadInput()) {                                                         \
                if (out_count != nullptr) {                                                          \
                        *out_count = 0;                                                              \
    }                                                                                                \
    ResetNpadStates(out, count);                                                                     \
    return 0;                                                                                        \
    }                                                                                                \
    return Orig(out_count, out, count, port);                                                        \
    }                                                                                                \
    }                                                                                                \
    ;

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
            static void Callback(nn::hid::MouseState *out) {
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

            struct DetailHookEntry {
                const char *name;
                void (*install)(uintptr_t);
            };

            const DetailHookEntry detail_hooks[] = {
                {"_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_16NpadFullKeyStateEiRKj",
                 [](uintptr_t addr) { HidDetailGetNpadStatesFullKeyHook::InstallAtPtr(addr); }},
                {"_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_17NpadHandheldStateEiRKj",
                 [](uintptr_t addr) { HidDetailGetNpadStatesHandheldHook::InstallAtPtr(addr); }},
                {"_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_16NpadJoyDualStateEiRKj",
                 [](uintptr_t addr) { HidDetailGetNpadStatesJoyDualHook::InstallAtPtr(addr); }},
                {"_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_16NpadJoyLeftStateEiRKj",
                 [](uintptr_t addr) { HidDetailGetNpadStatesJoyLeftHook::InstallAtPtr(addr); }},
                {"_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_17NpadJoyRightStateEiRKj",
                 [](uintptr_t addr) { HidDetailGetNpadStatesJoyRightHook::InstallAtPtr(addr); }},
            };

            for (const auto &hook : detail_hooks) {
                void *addr = LookupSymbolAddress(hook.name);
                if (addr == nullptr) {
                    PRINT("[imgui_overlay] nn::hid detail symbol missing: %s", hook.name);
                    continue;
                }
                hook.install(reinterpret_cast<uintptr_t>(addr));
            }

            g_hid_hooks_installed = true;
            PRINT_LINE("[imgui_overlay] nn::hid input hooks installed");
        }

        static NVNboolean CmdBufInitializeWrapper(NVNcommandBuffer *buffer, NVNdevice *device) {
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
                io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
                io.BackendPlatformName = "d3hack";
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

                if (kImGuiBringup_DrawText && !g_font_uploaded && g_font_atlas_built) {
                    if (ImguiNvnBackend::setupFont()) {
                        g_font_uploaded = true;
                        PRINT_LINE("[imgui_overlay] ImGui font uploaded");
                    } else {
                        PRINT_LINE("[imgui_overlay] ERROR: ImGui font upload failed");
                    }
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

                if (kImGuiBringup_DrawText && g_imgui_ctx_initialized && g_font_uploaded && g_overlay.imgui_render_enabled()) {
                    ImguiNvnBackend::newFrame();

                    const ImVec2 viewport_size = ImguiNvnBackend::getBackendData()->viewportSize;
                    UpdateImGuiStyleAndScale(viewport_size);
                    PushImGuiGamepadInputs(ImGui::GetIO().DeltaTime);
                    if (g_overlay.overlay_visible()) {
                        PushImGuiMouseInputs(viewport_size);
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
                    if (g_overlay.focus_state().visible) {
                        ImguiNvnBackend::renderDrawData(ImGui::GetDrawData());
                    }
                }
            }

            if (g_orig_present != nullptr) {
                g_orig_present(queue, window, texture_index);
            }
        }

        // Hook nvnDeviceGetProcAddress directly (works even when nvnBootstrapLoader isn't in dynsym).
        HOOK_DEFINE_TRAMPOLINE(DeviceGetProcAddressHook) {
            static PFNNVNGENERICFUNCPTRPROC Callback(const NVNdevice *device, const char *name) {
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

    const d3::gui2::ui::GuiFocusState &GetGuiFocusState() {
        return g_overlay.focus_state();
    }

    void PostOverlayNotification(const ImVec4 &color, float ttl_s, const char *fmt, ...) {
        auto *notifications = g_overlay.notifications_window();
        if (notifications == nullptr) {
            return;
        }

        char    buf[1024];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        notifications->AddNotification(color, ttl_s, "%s", buf);
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
        } clear_in_progress {};

        g_font_build_attempted = true;

        ImGuiIO &io = ImGui::GetIO();
        if (io.Fonts == nullptr) {
            PRINT_LINE("[imgui_overlay] ERROR: ImGuiIO::Fonts is null; cannot build font atlas");
            return;
        }

        PRINT_LINE("[imgui_overlay] Building ImGui font atlas...");
        ImFontConfig font_cfg {};
        font_cfg.Name[0] = 'd';
        font_cfg.Name[1] = '3';
        font_cfg.Name[2] = '\0';
        // Build for docked readability (we apply a runtime scale for 720p).
        font_cfg.SizePixels  = 16.0f;
        font_cfg.OversampleH = 2;
        font_cfg.OversampleV = 2;
        static ImVector<ImWchar> s_font_ranges;
        static bool              s_font_ranges_built = false;
        if (!s_font_ranges_built) {
            ImFontGlyphRangesBuilder builder;
            builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
            builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
            builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
            builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
            builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
            builder.BuildRanges(&s_font_ranges);
            s_font_ranges_built = true;
        }
        font_cfg.GlyphRanges = s_font_ranges.Data;

        ImFont *font = nullptr;

        static unsigned char *s_romfs_ttf_data = nullptr;
        static size_t         s_romfs_ttf_size = 0;
        static std::string    s_romfs_ttf_path;
        static bool           s_romfs_ttf_tried = false;

        if (!s_romfs_ttf_tried) {
            s_romfs_ttf_tried = true;

            std::string path;
            if (d3::romfs::FindFirstFileWithSuffix("romfs:/d3gui", ".ttf", path) &&
                ReadFileToImGuiBuffer(path.c_str(), &s_romfs_ttf_data, &s_romfs_ttf_size, 5 * 1024 * 1024)) {
                s_romfs_ttf_path = path;
                PRINT("[imgui_overlay] Loaded romfs font: %s (%lu bytes)", s_romfs_ttf_path.c_str(), static_cast<unsigned long>(s_romfs_ttf_size));
            } else {
                PRINT_LINE("[imgui_overlay] romfs font not found; falling back to ImGui default font");
            }
        }

        if (s_romfs_ttf_data != nullptr && s_romfs_ttf_size > 0) {
            font_cfg.FontDataOwnedByAtlas = false;
            font                          = io.Fonts->AddFontFromMemoryTTF(s_romfs_ttf_data, static_cast<int>(s_romfs_ttf_size), font_cfg.SizePixels, &font_cfg);
            if (font == nullptr) {
                PRINT_LINE("[imgui_overlay] ERROR: AddFontFromMemoryTTF failed; falling back to ImGui default font");
            }
        }

        if (font == nullptr) {
            (void)io.Fonts->AddFontDefault();
        }

        g_font_atlas_built = io.Fonts->Build();
        if (!g_font_atlas_built) {
            PRINT_LINE("[imgui_overlay] ERROR: ImFontAtlas::Build failed");
            return;
        }
        if (s_romfs_ttf_data != nullptr) {
            IM_FREE(s_romfs_ttf_data);
            s_romfs_ttf_data = nullptr;
            s_romfs_ttf_size = 0;
        }
        PRINT_LINE("[imgui_overlay] ImGui font atlas built");
    }

}  // namespace d3::imgui_overlay
