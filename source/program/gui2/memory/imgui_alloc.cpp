#include "program/gui2/memory/imgui_alloc.hpp"

#include <cstdint>

#include "lib/diag/assert.hpp"
#include "nn/nn_common.hpp"
#include "program/d3/setting.hpp"
#include "program/log_once.hpp"
#include "program/system_allocator.hpp"

#include "imgui/imgui.h"

namespace nn::ro {
    void   Initialize() noexcept;
    Result LookupSymbol(uintptr_t *pOutAddress, const char *name) noexcept;
}  // namespace nn::ro

namespace d3::gui2::memory::imgui_alloc {
    namespace {
        using ImGuiMallocFn = void *(*)(size_t);
        using ImGuiFreeFn   = void (*)(void *);

        ImGuiMallocFn g_imgui_malloc   = nullptr;
        ImGuiFreeFn   g_imgui_free     = nullptr;
        bool          g_allocators_set = false;

        auto EnsureRoInitialized() -> bool {
            static bool s_ro_initialized    = false;
            static bool s_ro_init_attempted = false;

            if (!s_ro_initialized && !s_ro_init_attempted) {
                s_ro_init_attempted = true;
                nn::ro::Initialize();
                s_ro_initialized = true;
            }

            return s_ro_initialized;
        }

        void ResolveImGuiAllocators() {
            if (g_imgui_malloc == nullptr) {
                if (!EnsureRoInitialized()) {
                    return;
                }
                uintptr_t  addr = 0;
                const auto rc   = nn::ro::LookupSymbol(&addr, "malloc");
                if (R_SUCCEEDED(rc) && addr != 0) {
                    g_imgui_malloc = reinterpret_cast<ImGuiMallocFn>(addr);
                } else {
                    if (d3::log_once::ShouldLog("imgui_alloc.malloc_missing")) {
                        PRINT("[imgui_alloc] ERROR: nn::ro::LookupSymbol(malloc) failed (rc=0x%x addr=0x%lx)", rc, addr);
                    }
                }
            }
            if (g_imgui_free == nullptr) {
                if (!EnsureRoInitialized()) {
                    return;
                }
                uintptr_t  addr = 0;
                const auto rc   = nn::ro::LookupSymbol(&addr, "free");
                if (R_SUCCEEDED(rc) && addr != 0) {
                    g_imgui_free = reinterpret_cast<ImGuiFreeFn>(addr);
                } else {
                    if (d3::log_once::ShouldLog("imgui_alloc.free_missing")) {
                        PRINT("[imgui_alloc] ERROR: nn::ro::LookupSymbol(free) failed (rc=0x%x addr=0x%lx)", rc, addr);
                    }
                }
            }
        }

        auto ImGuiAlloc(size_t size, void *user_data) -> void * {
            if (user_data != nullptr) {
                auto *alloc = static_cast<nn::mem::StandardAllocator *>(user_data);
                void *ptr   = alloc->Allocate(size);
                EXL_ABORT_UNLESS(ptr != nullptr, "[imgui_alloc] ImGuiAlloc system allocator OOM (size=0x%zx)", size);
                return ptr;
            }
            EXL_ABORT_UNLESS(g_imgui_malloc != nullptr, "[imgui_alloc] ImGuiAlloc missing malloc");
            return g_imgui_malloc(size);
        }

        void ImGuiFree(void *ptr, void *user_data) {
            if (ptr == nullptr) {
                return;
            }
            if (user_data != nullptr) {
                auto *alloc = static_cast<nn::mem::StandardAllocator *>(user_data);
                alloc->Free(ptr);
                return;
            }
            EXL_ABORT_UNLESS(g_imgui_free != nullptr, "[imgui_alloc] ImGuiFree missing free");
            g_imgui_free(ptr);
        }
    }  // namespace

    auto TryConfigureImGuiAllocators() -> bool {
        if (g_allocators_set) {
            return true;
        }

        // Prefer the game's system allocator to reduce fragmentation and isolate ImGui
        // from libc malloc quirks on different HOS versions.
        if (auto *sys_alloc = d3::system_allocator::GetSystemAllocator(); sys_alloc != nullptr) {
            ImGui::SetAllocatorFunctions(ImGuiAlloc, ImGuiFree, sys_alloc);
            g_allocators_set = true;
            return true;
        }

        ResolveImGuiAllocators();
        if (g_imgui_malloc != nullptr && g_imgui_free != nullptr) {
            ImGui::SetAllocatorFunctions(ImGuiAlloc, ImGuiFree, nullptr);
            g_allocators_set = true;
            return true;
        }

        return false;
    }

}  // namespace d3::gui2::memory::imgui_alloc
