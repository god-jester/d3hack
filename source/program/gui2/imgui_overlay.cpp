#include "program/gui2/imgui_overlay.hpp"

#include <algorithm>
#include <cstring>

#include "lib/hook/trampoline.hpp"
#include "program/d3/setting.hpp"

#include "imgui/imgui.h"
#include "imgui_backend/imgui_impl_nvn.hpp"

// Pull in only the C NVN API (avoid the NVN C++ funcptr shim headers which declare
// a conflicting `nvnBootstrapLoader` symbol).
#include "nvn/nvn.h"
// NVN C++ proc loader (fills pfncpp_* function pointers).
#include "nvn/nvn_CppFuncPtrImpl.h"

extern "C" PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *);

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

        if (g_overlay_visible) {
            if (kImGuiBringup_AlwaysSubmitProofOfLife) {
                // Gate C: submit an unmistakable fontless proof-of-life draw.
                ImguiNvnBackend::SubmitProofOfLifeClear();
            }

            if (kImGuiBringup_DrawText && g_font_uploaded) {
                ImguiNvnBackend::newFrame();
                ImGui::NewFrame();

                ImGui::GetForegroundDrawList()->AddText(
                    ImVec2(160.0f, 40.0f),
                    IM_COL32(255, 255, 255, 255),
                    "ImGui OK");

                ImGui::Render();
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
