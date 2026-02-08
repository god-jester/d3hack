#include "program/gui2/backend/nvn_hooks.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>

#include "lib/hook/trampoline.hpp"
#include "program/d3/setting.hpp"

// Pull in only the C NVN API (avoid the NVN C++ funcptr shim headers which declare
// a conflicting `nvnBootstrapLoader` symbol).
#include <nvn/nvn.h>
// NVN C++ wrapper types.
#include <nvn/nvn_Cpp.h>
// NVN C++ proc loader (fills pfncpp_* function pointers).
#include <nvn/nvn_CppFuncPtrImpl.h>

extern "C" auto nvnBootstrapLoader(const char *) -> PFNNVNGENERICFUNCPTRPROC;

namespace d3::gui2::backend::nvn_hooks {
    namespace {
        using NvnDeviceGetProcAddressFn     = PFNNVNDEVICEGETPROCADDRESSPROC;
        using NvnQueuePresentTextureFn      = PFNNVNQUEUEPRESENTTEXTUREPROC;
        using NvnCommandBufferInitializeFn  = PFNNVNCOMMANDBUFFERINITIALIZEPROC;
        using NvnWindowBuilderSetTexturesFn = PFNNVNWINDOWBUILDERSETTEXTURESPROC;
        using NvnWindowSetCropFn            = PFNNVNWINDOWSETCROPPROC;

        PresentCallback g_on_present = nullptr;

        NvnDeviceGetProcAddressFn    g_orig_get_proc           = nullptr;
        NvnQueuePresentTextureFn     g_orig_present            = nullptr;
        NvnCommandBufferInitializeFn g_orig_cmd_buf_initialize = nullptr;
        NVNdevice                   *g_device                  = nullptr;
        NVNqueue                    *g_queue                   = nullptr;
        NVNcommandBuffer            *g_cmd_buf                 = nullptr;

        constexpr int                                           kMaxSwapchainTextures = 8;
        std::array<const nvn::Texture *, kMaxSwapchainTextures> g_swapchain_textures {};
        int                                                     g_swapchain_texture_count = 0;

        NvnWindowBuilderSetTexturesFn g_orig_window_builder_set_textures = nullptr;
        NvnWindowSetCropFn            g_orig_window_set_crop             = nullptr;

        int g_crop_w = 0;
        int g_crop_h = 0;

        bool g_hooks_installed  = false;
        bool g_cpp_procs_loaded = false;

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

        static auto CmdBufInitializeWrapper(NVNcommandBuffer *buffer, NVNdevice *device) -> NVNboolean {
            if (g_cmd_buf == nullptr && buffer != nullptr) {
                g_cmd_buf = buffer;
            }

            if (g_orig_cmd_buf_initialize == nullptr) {
                PRINT_LINE("[nvn_hooks] ERROR: CmdBufInitializeWrapper called without orig ptr");
                return 0;
            }

            return g_orig_cmd_buf_initialize(buffer, device);
        }

        static void QueuePresentTextureWrapper(NVNqueue *queue, NVNwindow *window, int texture_index) {
            static bool s_logged_present = false;
            if (!s_logged_present) {
                s_logged_present = true;
                PRINT_LINE("[nvn_hooks] Present hook entered");
            }

            if (g_queue == nullptr && queue != nullptr) {
                g_queue = queue;
            }

            if (g_on_present != nullptr) {
                g_on_present(queue, window, texture_index);
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

    auto InstallHooks(PresentCallback on_present) -> bool {
        if (g_hooks_installed) {
            if (g_on_present == nullptr) {
                g_on_present = on_present;
            }
            return true;
        }

        g_on_present = on_present;

        const auto get_proc = nvnBootstrapLoader("nvnDeviceGetProcAddress");
        if (get_proc == nullptr) {
            PRINT_LINE("[nvn_hooks] ERROR: Failed to resolve nvnDeviceGetProcAddress via nvnBootstrapLoader");
            return false;
        }

        g_orig_get_proc = reinterpret_cast<NvnDeviceGetProcAddressFn>(get_proc);
        DeviceGetProcAddressHook::InstallAtPtr(reinterpret_cast<uintptr_t>(get_proc));
        g_hooks_installed = true;
        return true;
    }

    auto LoadCppProcsOnce() -> void {
        if (g_cpp_procs_loaded) {
            return;
        }
        if (g_device == nullptr || g_orig_get_proc == nullptr) {
            return;
        }
        nvnLoadCPPProcs(
            reinterpret_cast<nvn::Device *>(g_device),
            reinterpret_cast<nvn::DeviceGetProcAddressFunc>(g_orig_get_proc)
        );
        g_cpp_procs_loaded = true;
    }

    auto GetDevice() -> NVNdevice * { return g_device; }
    auto GetQueue() -> NVNqueue * { return g_queue; }
    auto GetCommandBuffer() -> NVNcommandBuffer * { return g_cmd_buf; }
    auto GetDeviceGetProcAddress() -> PFNNVNDEVICEGETPROCADDRESSPROC { return g_orig_get_proc; }

    auto GetCropWidth() -> int { return g_crop_w; }
    auto GetCropHeight() -> int { return g_crop_h; }

    auto GetSwapchainTextureCount() -> int { return g_swapchain_texture_count; }

    auto GetSwapchainTextureForBackend(int index) -> const nvn::Texture * {
        if (index < 0 || index >= g_swapchain_texture_count) {
            return nullptr;
        }
        return g_swapchain_textures[static_cast<size_t>(index)];
    }

    auto GetSwapchainTextureSize(int index, int *out_w, int *out_h) -> bool {
        if (out_w != nullptr) {
            *out_w = 0;
        }
        if (out_h != nullptr) {
            *out_h = 0;
        }

        const nvn::Texture *tex = GetSwapchainTextureForBackend(index);
        if (tex == nullptr) {
            return false;
        }

        if (out_w != nullptr) {
            *out_w = tex->GetWidth();
        }
        if (out_h != nullptr) {
            *out_h = tex->GetHeight();
        }
        return true;
    }

}  // namespace d3::gui2::backend::nvn_hooks
