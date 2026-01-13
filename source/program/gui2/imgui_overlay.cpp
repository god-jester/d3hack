#include "program/gui2/imgui_overlay.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "lib/hook/trampoline.hpp"
#include "nn/hid.hpp"
#include "program/config.hpp"
#include "program/d3/setting.hpp"
#include "program/runtime_apply.hpp"

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
    extern ::RWindow* g_ptMainRWindow;
}

namespace d3::imgui_overlay {
namespace {

using NvnDeviceGetProcAddressFn = PFNNVNDEVICEGETPROCADDRESSPROC;
using NvnQueuePresentTextureFn = PFNNVNQUEUEPRESENTTEXTUREPROC;
using NvnCommandBufferInitializeFn = PFNNVNCOMMANDBUFFERINITIALIZEPROC;
using NvnWindowBuilderSetTexturesFn = PFNNVNWINDOWBUILDERSETTEXTURESPROC;
using NvnWindowSetCropFn = PFNNVNWINDOWSETCROPPROC;

constexpr bool kImGuiBringup_DrawText = true;
constexpr bool kImGuiBringup_AlwaysSubmitProofOfLife = true;

NvnDeviceGetProcAddressFn g_orig_get_proc = nullptr;
NvnQueuePresentTextureFn g_orig_present = nullptr;
NvnCommandBufferInitializeFn g_orig_cmd_buf_initialize = nullptr;
NVNdevice *g_device = nullptr;
NVNqueue *g_queue = nullptr;

// Captured game command buffer (preferred vs creating our own).
NVNcommandBuffer *g_cmd_buf = nullptr;

constexpr int kMaxSwapchainTextures = 8;
const nvn::Texture *g_swapchain_textures[kMaxSwapchainTextures]{};
int g_swapchain_texture_count = 0;

NvnWindowBuilderSetTexturesFn g_orig_window_builder_set_textures = nullptr;
NvnWindowSetCropFn g_orig_window_set_crop = nullptr;

int g_crop_w = 0;
int g_crop_h = 0;

bool g_imgui_render_enabled = true;
bool g_show_demo_window = false;
bool g_show_metrics = false;

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

bool g_imgui_ctx_initialized = false;
bool g_backend_initialized = false;
bool g_font_atlas_built = false;
bool g_font_uploaded = false;
bool g_overlay_visible = true;

// NOTE: The backend expects NVN C++ wrapper types (nvn::Device/Queue/CommandBuffer).
// We will wire those up once we hook stable NVN init entrypoints.
bool g_hooks_installed = false;

bool g_font_build_attempted = false;

float g_overlay_toggle_hold_s = 0.0f;
bool g_overlay_toggle_armed = true;
bool g_overlay_request_focus = false;
bool g_block_gamepad_input_to_game = false;
bool g_hid_passthrough_for_overlay = false;
bool g_hid_hooks_installed = false;

struct ScopedHidPassthroughForOverlay {
    ScopedHidPassthroughForOverlay() { g_hid_passthrough_for_overlay = true; }
    ~ScopedHidPassthroughForOverlay() { g_hid_passthrough_for_overlay = false; }
};

template <typename TState>
static void ClearNpadButtonsAndSticks(TState* st) {
    if (st == nullptr) {
        return;
    }
    st->mButtons.field[0] = 0;
    st->mAnalogStickL.X = 0;
    st->mAnalogStickL.Y = 0;
    st->mAnalogStickR.X = 0;
    st->mAnalogStickR.Y = 0;
}

struct NpadCombinedState {
    u64 buttons = 0;
    nn::hid::AnalogStickState stick_l{};
    nn::hid::AnalogStickState stick_r{};
    bool have_stick_l = false;
    bool have_stick_r = false;
};

static float NormalizeStickComponent(s32 v) {
    constexpr float kMax = 32767.0f;
    float out = static_cast<float>(v) / kMax;
    if (out < -1.0f)
        out = -1.0f;
    if (out > 1.0f)
        out = 1.0f;
    return out;
}

static bool CombineNpadState(NpadCombinedState& out, uint port) {
    out = {};

    ScopedHidPassthroughForOverlay passthrough_guard;

    nn::hid::NpadFullKeyState full{};
    nn::hid::NpadHandheldState handheld{};
    nn::hid::NpadJoyDualState joy_dual{};
    nn::hid::NpadJoyLeftState joy_left{};
    nn::hid::NpadJoyRightState joy_right{};

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

    auto consider_stick = [](bool& have, nn::hid::AnalogStickState& dst, const nn::hid::AnalogStickState& src) {
        const auto mag = static_cast<u64>(std::abs(src.X)) + static_cast<u64>(std::abs(src.Y));
        const auto dst_mag = static_cast<u64>(std::abs(dst.X)) + static_cast<u64>(std::abs(dst.Y));
        if (!have || mag > dst_mag) {
            dst = src;
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

static bool g_ui_config_initialized = false;
static PatchConfig g_ui_config{};
static bool g_ui_dirty = false;
static char g_ui_status[256] = {};
static float g_ui_status_ttl_s = 0.0f;
static ImVec4 g_ui_status_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
static NpadCombinedState g_last_npad{};
static bool g_last_npad_valid = false;

static void PushImGuiGamepadInputs(float dt_s) {
    if (!g_imgui_ctx_initialized) {
        return;
    }

    // Avoid calling into hid early; wait until the game has completed shell initialization.
    if (d3::g_ptMainRWindow == nullptr) {
        return;
    }

    NpadCombinedState st{};
    g_last_npad_valid = CombineNpadState(st, 0);
    g_last_npad = st;

    const bool plus  = NpadButtonDown(st.buttons, nn::hid::NpadButton::Plus);
    const bool minus = NpadButtonDown(st.buttons, nn::hid::NpadButton::Minus);

        if (plus && minus) {
            g_overlay_toggle_hold_s += dt_s;
            if (g_overlay_toggle_hold_s >= 0.5f && g_overlay_toggle_armed) {
                const bool was_visible = g_overlay_visible;
                g_overlay_visible = !g_overlay_visible;
                if (!was_visible && g_overlay_visible) {
                    g_overlay_request_focus = true;
                }
                if (g_ui_config_initialized) {
                    g_ui_config.gui.visible = g_overlay_visible;
                    g_ui_dirty = true;
                }
                PRINT("[imgui_overlay] overlay visible=%d", g_overlay_visible ? 1 : 0);
                g_overlay_toggle_armed = false;
            }
        } else {
            g_overlay_toggle_hold_s = 0.0f;
            g_overlay_toggle_armed = true;
        }

    ImGuiIO& io = ImGui::GetIO();
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
        const float x = NormalizeStickComponent(st.stick_l.X);
        const float y = NormalizeStickComponent(st.stick_l.Y);
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

using GetNpadStateFullKeyFn = void (*)(nn::hid::NpadFullKeyState*, uint const&);
using GetNpadStateHandheldFn = void (*)(nn::hid::NpadHandheldState*, uint const&);
using GetNpadStateJoyDualFn = void (*)(nn::hid::NpadJoyDualState*, uint const&);
using GetNpadStateJoyLeftFn = void (*)(nn::hid::NpadJoyLeftState*, uint const&);
using GetNpadStateJoyRightFn = void (*)(nn::hid::NpadJoyRightState*, uint const&);

using GetNpadStatesFullKeyFn = void (*)(nn::hid::NpadFullKeyState*, int, uint const&);
using GetNpadStatesHandheldFn = void (*)(nn::hid::NpadHandheldState*, int, uint const&);
using GetNpadStatesJoyDualFn = void (*)(nn::hid::NpadJoyDualState*, int, uint const&);
using GetNpadStatesJoyLeftFn = void (*)(nn::hid::NpadJoyLeftState*, int, uint const&);
using GetNpadStatesJoyRightFn = void (*)(nn::hid::NpadJoyRightState*, int, uint const&);

HOOK_DEFINE_TRAMPOLINE(HidGetNpadStateFullKeyHook) {
    static void Callback(nn::hid::NpadFullKeyState* out, uint const& port) {
        Orig(out, port);
        if (g_block_gamepad_input_to_game && !g_hid_passthrough_for_overlay) {
            ClearNpadButtonsAndSticks(out);
        }
    }
};

HOOK_DEFINE_TRAMPOLINE(HidGetNpadStateHandheldHook) {
    static void Callback(nn::hid::NpadHandheldState* out, uint const& port) {
        Orig(out, port);
        if (g_block_gamepad_input_to_game && !g_hid_passthrough_for_overlay) {
            ClearNpadButtonsAndSticks(out);
        }
    }
};

HOOK_DEFINE_TRAMPOLINE(HidGetNpadStateJoyDualHook) {
    static void Callback(nn::hid::NpadJoyDualState* out, uint const& port) {
        Orig(out, port);
        if (g_block_gamepad_input_to_game && !g_hid_passthrough_for_overlay) {
            ClearNpadButtonsAndSticks(out);
        }
    }
};

HOOK_DEFINE_TRAMPOLINE(HidGetNpadStateJoyLeftHook) {
    static void Callback(nn::hid::NpadJoyLeftState* out, uint const& port) {
        Orig(out, port);
        if (g_block_gamepad_input_to_game && !g_hid_passthrough_for_overlay) {
            ClearNpadButtonsAndSticks(out);
        }
    }
};

HOOK_DEFINE_TRAMPOLINE(HidGetNpadStateJoyRightHook) {
    static void Callback(nn::hid::NpadJoyRightState* out, uint const& port) {
        Orig(out, port);
        if (g_block_gamepad_input_to_game && !g_hid_passthrough_for_overlay) {
            ClearNpadButtonsAndSticks(out);
        }
    }
};

HOOK_DEFINE_TRAMPOLINE(HidGetNpadStatesFullKeyHook) {
    static void Callback(nn::hid::NpadFullKeyState* out, int count, uint const& port) {
        Orig(out, count, port);
        if (g_block_gamepad_input_to_game && !g_hid_passthrough_for_overlay) {
            for (int i = 0; i < count; ++i) {
                ClearNpadButtonsAndSticks(out + i);
            }
        }
    }
};

HOOK_DEFINE_TRAMPOLINE(HidGetNpadStatesHandheldHook) {
    static void Callback(nn::hid::NpadHandheldState* out, int count, uint const& port) {
        Orig(out, count, port);
        if (g_block_gamepad_input_to_game && !g_hid_passthrough_for_overlay) {
            for (int i = 0; i < count; ++i) {
                ClearNpadButtonsAndSticks(out + i);
            }
        }
    }
};

HOOK_DEFINE_TRAMPOLINE(HidGetNpadStatesJoyDualHook) {
    static void Callback(nn::hid::NpadJoyDualState* out, int count, uint const& port) {
        Orig(out, count, port);
        if (g_block_gamepad_input_to_game && !g_hid_passthrough_for_overlay) {
            for (int i = 0; i < count; ++i) {
                ClearNpadButtonsAndSticks(out + i);
            }
        }
    }
};

HOOK_DEFINE_TRAMPOLINE(HidGetNpadStatesJoyLeftHook) {
    static void Callback(nn::hid::NpadJoyLeftState* out, int count, uint const& port) {
        Orig(out, count, port);
        if (g_block_gamepad_input_to_game && !g_hid_passthrough_for_overlay) {
            for (int i = 0; i < count; ++i) {
                ClearNpadButtonsAndSticks(out + i);
            }
        }
    }
};

HOOK_DEFINE_TRAMPOLINE(HidGetNpadStatesJoyRightHook) {
    static void Callback(nn::hid::NpadJoyRightState* out, int count, uint const& port) {
        Orig(out, count, port);
        if (g_block_gamepad_input_to_game && !g_hid_passthrough_for_overlay) {
            for (int i = 0; i < count; ++i) {
                ClearNpadButtonsAndSticks(out + i);
            }
        }
    }
};

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

    g_hid_hooks_installed = true;
    PRINT_LINE("[imgui_overlay] nn::hid input hooks installed");
}

static void EnsureUiConfigLoaded() {
    if (g_ui_config_initialized) {
        return;
    }
    if (!global_config.initialized) {
        return;
    }
    g_ui_config = global_config;
    g_imgui_render_enabled = global_config.gui.enabled;
    g_overlay_visible = global_config.gui.visible;
    g_ui_config_initialized = true;
    g_ui_dirty = false;
}

static void SetUiStatus(const ImVec4& color, float ttl_s, const char* fmt, ...) {
    g_ui_status_color = color;
    g_ui_status_ttl_s = ttl_s;
    g_ui_status[0] = '\0';

    va_list vl;
    va_start(vl, fmt);
    vsnprintf(g_ui_status, sizeof(g_ui_status), fmt, vl);
    va_end(vl);
}

static const char* SeasonMapModeToString(PatchConfig::SeasonEventMapMode mode) {
    switch (mode) {
    case PatchConfig::SeasonEventMapMode::MapOnly:
        return "MapOnly";
    case PatchConfig::SeasonEventMapMode::OverlayConfig:
        return "OverlayConfig";
    case PatchConfig::SeasonEventMapMode::Disabled:
        return "Disabled";
    default:
        return "Disabled";
    }
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
    if (g_queue == nullptr && queue != nullptr) {
        g_queue = queue;
    }

    if (!g_backend_initialized && g_device && g_queue) {
        if (g_orig_get_proc != nullptr) {
            // Seed NVN C++ proc table using the real function pointer captured from the game.
            nvnLoadCPPProcs(reinterpret_cast<nvn::Device *>(g_device),
                            reinterpret_cast<nvn::DeviceGetProcAddressFunc>(g_orig_get_proc));
        }

        if (g_orig_get_proc != nullptr && g_cmd_buf != nullptr) {
            ImguiNvnBackend::NvnBackendInitInfo init_info{};
            init_info.device = reinterpret_cast<nvn::Device *>(g_device);
            init_info.queue = reinterpret_cast<nvn::Queue *>(g_queue);
            init_info.cmdBuf = reinterpret_cast<nvn::CommandBuffer *>(g_cmd_buf);
            ImguiNvnBackend::InitBackend(init_info);
            g_backend_initialized = true;
        }
    }

    if (g_backend_initialized) {
        if (kImGuiBringup_DrawText && !g_font_uploaded && g_font_atlas_built) {
            if (ImguiNvnBackend::setupFont()) {
                g_font_uploaded = true;
                PRINT_LINE("[imgui_overlay] ImGui font uploaded");
            } else {
                PRINT_LINE("[imgui_overlay] ERROR: ImGui font upload failed (fallback to proof-of-life only)");
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
            // Gate C: submit an unmistakable fontless proof-of-life draw.
            ImguiNvnBackend::SubmitProofOfLifeClear();
        }

        EnsureUiConfigLoaded();

        g_block_gamepad_input_to_game = false;

        if (kImGuiBringup_DrawText && g_imgui_ctx_initialized) {
            ImguiNvnBackend::newFrame();

            PushImGuiGamepadInputs(ImGui::GetIO().DeltaTime);

            ImGui::NewFrame();

            const bool gui_enabled = g_ui_config_initialized ? g_ui_config.gui.enabled : global_config.gui.enabled;
            g_block_gamepad_input_to_game = (gui_enabled && g_overlay_visible);
            if (gui_enabled && g_overlay_visible && g_font_uploaded) {
                ImGui::SetNextWindowPos(ImVec2(24.0f, 140.0f), ImGuiCond_Once);
                ImGui::SetNextWindowSize(ImVec2(740.0f, 480.0f), ImGuiCond_Once);
                const bool focus = g_overlay_request_focus;
                if (focus) {
                    ImGui::SetNextWindowFocus();
                }
                const bool window_visible = ImGui::Begin("d3hack config", &g_overlay_visible, ImGuiWindowFlags_NoSavedSettings);
                if (window_visible) {
                    if (focus) {
                        g_overlay_request_focus = false;
                    }

                    ImGuiIO& io = ImGui::GetIO();
                    ImGui::TextUnformatted("Hold + and - (0.5s) to toggle overlay visibility.");
                    ImGui::Text("Viewport: %.0fx%.0f | Crop: %dx%d | Swapchain: %d",
                                ImguiNvnBackend::getBackendData()->viewportSize.x,
                                ImguiNvnBackend::getBackendData()->viewportSize.y,
                                g_crop_w, g_crop_h, g_swapchain_texture_count);
                    ImGui::Text("ImGui: NavActive=%d NavVisible=%d",
                                io.NavActive ? 1 : 0,
                                io.NavVisible ? 1 : 0);
                    ImGui::Text("NPAD: ok=%d buttons=0x%llx stickL=(%d,%d) stickR=(%d,%d)",
                                g_last_npad_valid ? 1 : 0,
                                static_cast<unsigned long long>(g_last_npad.buttons),
                                g_last_npad.stick_l.X, g_last_npad.stick_l.Y,
                                g_last_npad.stick_r.X, g_last_npad.stick_r.Y);

                    ImGui::Separator();

                    if (!g_ui_config_initialized) {
                        ImGui::TextUnformatted("Config not loaded yet.");
                    } else {
                        ImGui::Text("Config source: %s", global_config.defaults_only ? "built-in defaults" : "sd:/config/d3hack-nx/config.toml");

                        if (g_ui_dirty) {
                            ImGui::TextUnformatted("UI state: UNSAVED changes");
                        } else {
                            ImGui::TextUnformatted("UI state: clean");
                        }

                        if (focus) {
                            ImGui::SetKeyboardFocusHere();
                        }

                        if (ImGui::Button("Reset UI to current config")) {
                            g_ui_config = global_config;
                            g_imgui_render_enabled = global_config.gui.enabled;
                            g_overlay_visible = global_config.gui.visible;
                            g_ui_dirty = false;
                        }

                        ImGui::SameLine();
                        if (ImGui::Checkbox("GUI enabled", &g_ui_config.gui.enabled)) {
                            g_imgui_render_enabled = g_ui_config.gui.enabled;
                            g_ui_dirty = true;
                        }

                        ImGui::SameLine();
                        if (ImGui::Button("Apply")) {
                            const PatchConfig normalized = NormalizePatchConfig(g_ui_config);
                            d3::RuntimeApplyResult apply{};
                            d3::ApplyPatchConfigRuntime(normalized, &apply);

                            g_ui_config = global_config;
                            g_imgui_render_enabled = global_config.gui.enabled;
                            g_overlay_visible = global_config.gui.visible;
                            g_ui_dirty = false;

                            if (apply.restart_required) {
                                SetUiStatus(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), 6.0f, "Applied. Restart required.");
                            } else if (apply.applied_enable_only || apply.note[0] != '\0') {
                                SetUiStatus(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, "Applied (%s).", apply.note[0] ? apply.note : "runtime");
                            } else {
                                SetUiStatus(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, "Applied.");
                            }
                        }

                        ImGui::SameLine();
                        if (ImGui::Button("Save")) {
                            std::string error;
                            g_ui_config.gui.visible = g_overlay_visible;
                            if (SavePatchConfigToPath("sd:/config/d3hack-nx/config.toml", g_ui_config, error)) {
                                SetUiStatus(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, "Saved config.toml");
                            } else {
                                SetUiStatus(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 8.0f, "Save failed: %s", error.empty() ? "unknown" : error.c_str());
                            }
                        }

                        ImGui::SameLine();
                        if (ImGui::Button("Reload")) {
                            PatchConfig loaded{};
                            std::string error;
                            if (LoadPatchConfigFromPath("sd:/config/d3hack-nx/config.toml", loaded, error)) {
                                d3::RuntimeApplyResult apply{};
                                d3::ApplyPatchConfigRuntime(loaded, &apply);
                                g_ui_config = global_config;
                                g_imgui_render_enabled = global_config.gui.enabled;
                                g_overlay_visible = global_config.gui.visible;
                                g_ui_dirty = false;
                                SetUiStatus(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, "Reloaded config.toml");
                            } else {
                                SetUiStatus(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 8.0f, "Reload failed: %s", error.empty() ? "not found" : error.c_str());
                            }
                        }

                        if (g_ui_status_ttl_s > 0.0f) {
                            g_ui_status_ttl_s -= ImGui::GetIO().DeltaTime;
                            if (g_ui_status_ttl_s < 0.0f) {
                                g_ui_status_ttl_s = 0.0f;
                            }
                        }
                        if (g_ui_status_ttl_s > 0.0f && g_ui_status[0] != '\0') {
                            ImGui::TextColored(g_ui_status_color, "%s", g_ui_status);
                        }

                        ImGui::Separator();

                        if (!g_imgui_render_enabled) {
                            ImGui::TextUnformatted("GUI is disabled (proof-of-life stays on). Enable GUI to edit config.");
                        } else if (ImGui::BeginTabBar("d3hack_cfg_tabs")) {
                            auto mark_dirty = [](bool changed) {
                                if (changed) {
                                    g_ui_dirty = true;
                                }
                            };

                            if (ImGui::BeginTabItem("Overlays")) {
                                mark_dirty(ImGui::Checkbox("Enabled##overlays", &g_ui_config.overlays.active));
                                ImGui::BeginDisabled(!g_ui_config.overlays.active);
                                mark_dirty(ImGui::Checkbox("Build locker watermark", &g_ui_config.overlays.buildlocker_watermark));
                                mark_dirty(ImGui::Checkbox("DDM labels", &g_ui_config.overlays.ddm_labels));
                                mark_dirty(ImGui::Checkbox("FPS label", &g_ui_config.overlays.fps_label));
                                mark_dirty(ImGui::Checkbox("Variable resolution label", &g_ui_config.overlays.var_res_label));
                                ImGui::EndDisabled();
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("GUI")) {
                                mark_dirty(ImGui::Checkbox("Enabled (persist)", &g_ui_config.gui.enabled));
                                mark_dirty(ImGui::Checkbox("Visible by default", &g_ui_config.gui.visible));
                                ImGui::TextUnformatted("Hotkey: hold + and - (0.5s) to toggle visibility.");
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Resolution")) {
                                mark_dirty(ImGui::Checkbox("Enabled##res", &g_ui_config.resolution_hack.active));
                                ImGui::BeginDisabled(!g_ui_config.resolution_hack.active);
                                int target = static_cast<int>(g_ui_config.resolution_hack.target_resolution);
                                if (ImGui::SliderInt("Output target (vertical)", &target, 720, 1440, "%dp")) {
                                    g_ui_config.resolution_hack.SetTargetRes(static_cast<u32>(target));
                                    g_ui_dirty = true;
                                }
                                float min_scale = g_ui_config.resolution_hack.min_res_scale;
                                if (ImGui::SliderFloat("Minimum resolution scale", &min_scale, 10.0f, 100.0f, "%.0f%%")) {
                                    g_ui_config.resolution_hack.min_res_scale = min_scale;
                                    g_ui_dirty = true;
                                }
                                mark_dirty(ImGui::Checkbox("Clamp textures to 2048", &g_ui_config.resolution_hack.clamp_textures_2048));
                                mark_dirty(ImGui::Checkbox("Experimental scheduler", &g_ui_config.resolution_hack.exp_scheduler));
                                ImGui::Text("Output size: %ux%u", g_ui_config.resolution_hack.OutputWidthPx(), g_ui_config.resolution_hack.OutputHeightPx());
                                ImGui::EndDisabled();
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Seasons")) {
                                mark_dirty(ImGui::Checkbox("Enabled##seasons", &g_ui_config.seasons.active));
                                ImGui::BeginDisabled(!g_ui_config.seasons.active);
                                mark_dirty(ImGui::Checkbox("Allow online play", &g_ui_config.seasons.allow_online));
                                int season = static_cast<int>(g_ui_config.seasons.current_season);
                                if (ImGui::SliderInt("Current season", &season, 1, 200)) {
                                    g_ui_config.seasons.current_season = static_cast<u32>(season);
                                    g_ui_dirty = true;
                                }
                                mark_dirty(ImGui::Checkbox("Spoof PTR flag", &g_ui_config.seasons.spoof_ptr));
                                ImGui::EndDisabled();
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Events")) {
                                mark_dirty(ImGui::Checkbox("Enabled##events", &g_ui_config.events.active));

                                const char* mode_label = SeasonMapModeToString(g_ui_config.events.SeasonMapMode);
                                if (ImGui::BeginCombo("Season map mode", mode_label)) {
                                    auto selectable_mode = [&](PatchConfig::SeasonEventMapMode mode) {
                                        const bool is_selected = (g_ui_config.events.SeasonMapMode == mode);
                                        if (ImGui::Selectable(SeasonMapModeToString(mode), is_selected)) {
                                            g_ui_config.events.SeasonMapMode = mode;
                                            g_ui_dirty = true;
                                        }
                                        if (is_selected) {
                                            ImGui::SetItemDefaultFocus();
                                        }
                                    };
                                    selectable_mode(PatchConfig::SeasonEventMapMode::MapOnly);
                                    selectable_mode(PatchConfig::SeasonEventMapMode::OverlayConfig);
                                    selectable_mode(PatchConfig::SeasonEventMapMode::Disabled);
                                    ImGui::EndCombo();
                                }

                                ImGui::BeginDisabled(!g_ui_config.events.active);
                                mark_dirty(ImGui::Checkbox("IGR enabled", &g_ui_config.events.IgrEnabled));
                                mark_dirty(ImGui::Checkbox("Anniversary enabled", &g_ui_config.events.AnniversaryEnabled));
                                mark_dirty(ImGui::Checkbox("Easter egg world enabled", &g_ui_config.events.EasterEggWorldEnabled));
                                mark_dirty(ImGui::Checkbox("Double rift keystones", &g_ui_config.events.DoubleRiftKeystones));
                                mark_dirty(ImGui::Checkbox("Double blood shards", &g_ui_config.events.DoubleBloodShards));
                                mark_dirty(ImGui::Checkbox("Double treasure goblins", &g_ui_config.events.DoubleTreasureGoblins));
                                mark_dirty(ImGui::Checkbox("Double bounty bags", &g_ui_config.events.DoubleBountyBags));
                                mark_dirty(ImGui::Checkbox("Royal Grandeur", &g_ui_config.events.RoyalGrandeur));
                                mark_dirty(ImGui::Checkbox("Legacy of Nightmares", &g_ui_config.events.LegacyOfNightmares));
                                mark_dirty(ImGui::Checkbox("Triune's Will", &g_ui_config.events.TriunesWill));
                                mark_dirty(ImGui::Checkbox("Pandemonium", &g_ui_config.events.Pandemonium));
                                mark_dirty(ImGui::Checkbox("Kanai powers", &g_ui_config.events.KanaiPowers));
                                mark_dirty(ImGui::Checkbox("Trials of Tempests", &g_ui_config.events.TrialsOfTempests));
                                mark_dirty(ImGui::Checkbox("Shadow clones", &g_ui_config.events.ShadowClones));
                                mark_dirty(ImGui::Checkbox("Fourth Kanai's Cube slot", &g_ui_config.events.FourthKanaisCubeSlot));
                                mark_dirty(ImGui::Checkbox("Ethereal items", &g_ui_config.events.EtherealItems));
                                mark_dirty(ImGui::Checkbox("Soul shards", &g_ui_config.events.SoulShards));
                                mark_dirty(ImGui::Checkbox("Swarm rifts", &g_ui_config.events.SwarmRifts));
                                mark_dirty(ImGui::Checkbox("Sanctified items", &g_ui_config.events.SanctifiedItems));
                                mark_dirty(ImGui::Checkbox("Dark Alchemy", &g_ui_config.events.DarkAlchemy));
                                mark_dirty(ImGui::Checkbox("Nesting portals", &g_ui_config.events.NestingPortals));
                                ImGui::EndDisabled();

                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Rare cheats")) {
                                mark_dirty(ImGui::Checkbox("Enabled##rare_cheats", &g_ui_config.rare_cheats.active));
                                ImGui::BeginDisabled(!g_ui_config.rare_cheats.active);

                                float move_speed = static_cast<float>(g_ui_config.rare_cheats.move_speed);
                                if (ImGui::SliderFloat("Move speed multiplier", &move_speed, 0.1f, 10.0f, "%.2fx")) {
                                    g_ui_config.rare_cheats.move_speed = static_cast<double>(move_speed);
                                    g_ui_dirty = true;
                                }
                                float attack_speed = static_cast<float>(g_ui_config.rare_cheats.attack_speed);
                                if (ImGui::SliderFloat("Attack speed multiplier", &attack_speed, 0.1f, 10.0f, "%.2fx")) {
                                    g_ui_config.rare_cheats.attack_speed = static_cast<double>(attack_speed);
                                    g_ui_dirty = true;
                                }

                                mark_dirty(ImGui::Checkbox("Floating damage color", &g_ui_config.rare_cheats.floating_damage_color));
                                mark_dirty(ImGui::Checkbox("Guaranteed legendaries", &g_ui_config.rare_cheats.guaranteed_legendaries));
                                mark_dirty(ImGui::Checkbox("Drop anything", &g_ui_config.rare_cheats.drop_anything));
                                mark_dirty(ImGui::Checkbox("Instant portal", &g_ui_config.rare_cheats.instant_portal));
                                mark_dirty(ImGui::Checkbox("No cooldowns", &g_ui_config.rare_cheats.no_cooldowns));
                                mark_dirty(ImGui::Checkbox("Instant craft actions", &g_ui_config.rare_cheats.instant_craft_actions));
                                mark_dirty(ImGui::Checkbox("Any gem any slot", &g_ui_config.rare_cheats.any_gem_any_slot));
                                mark_dirty(ImGui::Checkbox("Auto pickup", &g_ui_config.rare_cheats.auto_pickup));
                                mark_dirty(ImGui::Checkbox("Equip any slot", &g_ui_config.rare_cheats.equip_any_slot));
                                mark_dirty(ImGui::Checkbox("Unlock all difficulties", &g_ui_config.rare_cheats.unlock_all_difficulties));
                                mark_dirty(ImGui::Checkbox("Easy kill damage", &g_ui_config.rare_cheats.easy_kill_damage));
                                mark_dirty(ImGui::Checkbox("Cube no consume", &g_ui_config.rare_cheats.cube_no_consume));
                                mark_dirty(ImGui::Checkbox("Gem upgrade always", &g_ui_config.rare_cheats.gem_upgrade_always));
                                mark_dirty(ImGui::Checkbox("Gem upgrade speed", &g_ui_config.rare_cheats.gem_upgrade_speed));
                                mark_dirty(ImGui::Checkbox("Gem upgrade lvl150", &g_ui_config.rare_cheats.gem_upgrade_lvl150));
                                mark_dirty(ImGui::Checkbox("Equip multi legendary", &g_ui_config.rare_cheats.equip_multi_legendary));

                                ImGui::EndDisabled();
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Challenge rifts")) {
                                mark_dirty(ImGui::Checkbox("Enabled##cr", &g_ui_config.challenge_rifts.active));
                                ImGui::BeginDisabled(!g_ui_config.challenge_rifts.active);
                                mark_dirty(ImGui::Checkbox("Randomize within range", &g_ui_config.challenge_rifts.random));
                                int start = static_cast<int>(g_ui_config.challenge_rifts.range_start);
                                int end = static_cast<int>(g_ui_config.challenge_rifts.range_end);
                                if (ImGui::InputInt("Range start", &start)) {
                                    g_ui_config.challenge_rifts.range_start = static_cast<u32>(std::max(0, start));
                                    g_ui_dirty = true;
                                }
                                if (ImGui::InputInt("Range end", &end)) {
                                    g_ui_config.challenge_rifts.range_end = static_cast<u32>(std::max(0, end));
                                    g_ui_dirty = true;
                                }
                                ImGui::EndDisabled();
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Loot")) {
                                mark_dirty(ImGui::Checkbox("Enabled##loot", &g_ui_config.loot_modifiers.active));
                                ImGui::BeginDisabled(!g_ui_config.loot_modifiers.active);
                                mark_dirty(ImGui::Checkbox("Disable ancient drops", &g_ui_config.loot_modifiers.DisableAncientDrops));
                                mark_dirty(ImGui::Checkbox("Disable primal ancient drops", &g_ui_config.loot_modifiers.DisablePrimalAncientDrops));
                                mark_dirty(ImGui::Checkbox("Disable torment drops", &g_ui_config.loot_modifiers.DisableTormentDrops));
                                mark_dirty(ImGui::Checkbox("Disable torment check", &g_ui_config.loot_modifiers.DisableTormentCheck));
                                mark_dirty(ImGui::Checkbox("Suppress gift generation", &g_ui_config.loot_modifiers.SuppressGiftGeneration));

                                int forced_ilevel = static_cast<int>(g_ui_config.loot_modifiers.ForcedILevel);
                                if (ImGui::SliderInt("Forced iLevel", &forced_ilevel, 0, 70)) {
                                    g_ui_config.loot_modifiers.ForcedILevel = static_cast<u32>(forced_ilevel);
                                    g_ui_dirty = true;
                                }
                                int tiered_level = static_cast<int>(g_ui_config.loot_modifiers.TieredLootRunLevel);
                                if (ImGui::SliderInt("Tiered loot run level", &tiered_level, 0, 150)) {
                                    g_ui_config.loot_modifiers.TieredLootRunLevel = static_cast<u32>(tiered_level);
                                    g_ui_dirty = true;
                                }

                                static constexpr const char* ranks[] = {"Normal", "Ancient", "Primal"};
                                int rank_value = g_ui_config.loot_modifiers.AncientRankValue;
                                if (ImGui::Combo("Ancient rank", &rank_value, ranks, static_cast<int>(IM_ARRAYSIZE(ranks)))) {
                                    g_ui_config.loot_modifiers.AncientRankValue = rank_value;
                                    g_ui_dirty = true;
                                }

                                ImGui::EndDisabled();
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Debug")) {
                                mark_dirty(ImGui::Checkbox("Enabled##debug", &g_ui_config.debug.active));
                                ImGui::BeginDisabled(!g_ui_config.debug.active);
                                mark_dirty(ImGui::Checkbox("Enable crashes (danger)", &g_ui_config.debug.enable_crashes));
                                mark_dirty(ImGui::Checkbox("Enable pubfile dump", &g_ui_config.debug.enable_pubfile_dump));
                                mark_dirty(ImGui::Checkbox("Enable error traces", &g_ui_config.debug.enable_error_traces));
                                mark_dirty(ImGui::Checkbox("Enable debug flags", &g_ui_config.debug.enable_debug_flags));
                                ImGui::EndDisabled();

                                ImGui::Separator();
                                ImGui::Checkbox("Show ImGui demo window", &g_show_demo_window);
                                ImGui::Checkbox("Show ImGui metrics", &g_show_metrics);

                                ImGui::EndTabItem();
                            }

                            ImGui::EndTabBar();
                        }
                    }
                }

                // Always end the window even if Begin() returned false.
                ImGui::End();

                if (g_show_demo_window) {
                    ImGui::ShowDemoWindow(&g_show_demo_window);
                }
                if (g_show_metrics) {
                    ImGui::ShowMetricsWindow(&g_show_metrics);
                }
            }

            ImGui::Render();
            if (gui_enabled && g_overlay_visible && g_font_uploaded) {
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

    // Create the context once.
    if (!g_imgui_ctx_initialized) {
        PRINT_LINE("[imgui_overlay] Initialize: ImGui::CreateContext (begin)");
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        g_imgui_ctx_initialized = true;
        PRINT_LINE("[imgui_overlay] Initialize: ImGui::CreateContext (end)");

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
        io.BackendPlatformName = "d3hack";
    }

    // Build the font atlas outside NVN present/init hooks.
    // Avoid calling NewFrame/Render here; CPU-only font work is enough.
    PrepareFonts();

    PRINT_LINE("[imgui_overlay] Initialize: end");
}

void PrepareFonts() {
    if (!g_imgui_ctx_initialized) {
        PRINT_LINE("[imgui_overlay] PrepareFonts: ImGui context missing; skipping");
        return;
    }

    if (g_font_atlas_built) {
        return;
    }

    if (g_font_build_attempted) {
        return;
    }

    g_font_build_attempted = true;

    ImGuiIO &io = ImGui::GetIO();

    PRINT_LINE("[imgui_overlay] PrepareFonts: AddFontDefaultVector (begin)");
    ImFontConfig font_cfg{};
    font_cfg.Name[0] = 'd';
    font_cfg.Name[1] = '3';
    font_cfg.Name[2] = '\0';
    io.Fonts->AddFontDefaultVector(&font_cfg);
    PRINT_LINE("[imgui_overlay] PrepareFonts: AddFontDefaultVector (end)");

    PRINT_LINE("[imgui_overlay] PrepareFonts: Fonts->Build (begin)");
    g_font_atlas_built = io.Fonts->Build();
    PRINT("[imgui_overlay] PrepareFonts: Fonts->Build (end) built=%d\n", g_font_atlas_built ? 1 : 0);
}

}  // namespace d3::imgui_overlay
