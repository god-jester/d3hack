#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "imgui_backend/MemoryPoolMaker.h"
#include "imgui_backend/imgui_impl_nvn.hpp"
#include "imgui/imgui.h"
#include "lib/diag/assert.hpp"
#include "program/logging.hpp"

namespace ImguiNvnBackend {
namespace TextureSupport {

namespace {
    constexpr bool kLowMem_LogFontTexCreate           = true;
    // Disabled for now: ImGui may queue texture updates after initial creation (e.g. font atlas
    // rebuild/updates). Freeing Pixels triggers ImTextureData::GetPixelsAt asserts in those paths.
    constexpr bool kLowMem_FreeFontPixelsAfterUpload  = false;

    struct NvnImGuiTexture {
        nvn::Texture texture;
        nvn::Sampler sampler;
        nvn::TextureHandle handle;
        nvn::MemoryPool storage_pool;
        int texture_id = -1;
        int sampler_id = -1;
        int width = 0;
        int height = 0;
        ImTextureFormat format = ImTextureFormat_RGBA32;
    };

    struct DescriptorPair {
        int texture_id = -1;
        int sampler_id = -1;
    };

    bool g_descriptor_pools_ready = false;
    int g_next_texture_id = 1;
    int g_next_sampler_id = 1;
    int g_texture_descriptor_count = 0;
    int g_sampler_descriptor_count = 0;
    int g_texture_descriptor_first = 1;
    int g_sampler_descriptor_first = 1;
    std::vector<DescriptorPair> g_free_descriptors;

    void DestroyTextureFromImGui(ImTextureData *tex);

    size_t GetDevicePageSizeOrFallback(const nvn::Device *device) {
        int page_size = 0;
        if (device != nullptr) {
            device->GetInteger(nvn::DeviceInfo::MEMORY_POOL_PAGE_SIZE, &page_size);
        }
        const size_t page_size_u = page_size > 0 ? static_cast<size_t>(page_size) : 0;
        const bool is_power_of_two = page_size_u != 0 && ((page_size_u & (page_size_u - 1)) == 0);
        return is_power_of_two ? page_size_u : 0x1000;
    }

    bool InitDescriptorCountsInternal(const nvn::Device *device) {
        if (g_texture_descriptor_count > 0 && g_sampler_descriptor_count > 0 &&
            g_texture_descriptor_first > 0 && g_sampler_descriptor_first > 0) {
            return true;
        }

        int reserved_sampler_desc = 0;
        int reserved_texture_desc = 0;
        int max_sampler_desc = 0;
        int max_texture_desc = 0;
        if (device != nullptr) {
            device->GetInteger(nvn::DeviceInfo::RESERVED_SAMPLER_DESCRIPTORS, &reserved_sampler_desc);
            device->GetInteger(nvn::DeviceInfo::RESERVED_TEXTURE_DESCRIPTORS, &reserved_texture_desc);
            device->GetInteger(nvn::DeviceInfo::MAX_SAMPLER_POOL_SIZE, &max_sampler_desc);
            device->GetInteger(nvn::DeviceInfo::MAX_TEXTURE_POOL_SIZE, &max_texture_desc);
        }

        int sampler_desc_count = reserved_sampler_desc + MaxSampDescriptors;
        int texture_desc_count = reserved_texture_desc + MaxTexDescriptors;
        if (max_sampler_desc > 0 && sampler_desc_count > max_sampler_desc) {
            sampler_desc_count = max_sampler_desc;
        }
        if (max_texture_desc > 0 && texture_desc_count > max_texture_desc) {
            texture_desc_count = max_texture_desc;
        }

        if (sampler_desc_count < reserved_sampler_desc || texture_desc_count < reserved_texture_desc) {
            return false;
        }

        const int sampler_first = std::max(reserved_sampler_desc, 1);
        const int texture_first = std::max(reserved_texture_desc, 1);
        if (sampler_first >= sampler_desc_count || texture_first >= texture_desc_count) {
            return false;
        }

        g_sampler_descriptor_count = sampler_desc_count;
        g_texture_descriptor_count = texture_desc_count;
        g_sampler_descriptor_first = sampler_first;
        g_texture_descriptor_first = texture_first;
        return true;
    }

    bool EnsureDescriptorPoolsInternal(NvnBackendData *bd) {
        if (g_descriptor_pools_ready) {
            return true;
        }

        int sampler_desc_size = 0;
        int texture_desc_size = 0;
        bd->device->GetInteger(nvn::DeviceInfo::SAMPLER_DESCRIPTOR_SIZE, &sampler_desc_size);
        bd->device->GetInteger(nvn::DeviceInfo::TEXTURE_DESCRIPTOR_SIZE, &texture_desc_size);

        if (sampler_desc_size <= 0 || texture_desc_size <= 0) {
            return false;
        }

        if (!InitDescriptorCountsInternal(bd->device)) {
            return false;
        }

        const size_t sampler_pool_size = static_cast<size_t>(sampler_desc_size) *
                                         static_cast<size_t>(g_sampler_descriptor_count);
        const size_t texture_pool_size = static_cast<size_t>(texture_desc_size) *
                                         static_cast<size_t>(g_texture_descriptor_count);
        const size_t texture_pool_offset = ALIGN_UP(sampler_pool_size, static_cast<size_t>(texture_desc_size));
        const size_t total_pool_size = ALIGN_UP(texture_pool_offset + texture_pool_size,
                                                GetDevicePageSizeOrFallback(bd->device));

        if (!MemoryPoolMaker::createPool(&bd->sampTexMemPool, total_pool_size)) {
            return false;
        }
        if (!bd->samplerPool.Initialize(&bd->sampTexMemPool, 0, g_sampler_descriptor_count)) {
            return false;
        }
        if (!bd->texPool.Initialize(&bd->sampTexMemPool, texture_pool_offset, g_texture_descriptor_count)) {
            return false;
        }

        g_next_texture_id = g_texture_descriptor_first;
        g_next_sampler_id = g_sampler_descriptor_first;
        g_free_descriptors.clear();
        g_descriptor_pools_ready = true;
        return true;
    }

    void AdoptDescriptorPoolsInternal(const nvn::Device *device, int next_texture_id, int next_sampler_id) {
        if (!InitDescriptorCountsInternal(device)) {
            return;
        }
        const int safe_texture_next = std::max(next_texture_id, g_texture_descriptor_first);
        const int safe_sampler_next = std::max(next_sampler_id, g_sampler_descriptor_first);
        if (safe_texture_next > g_next_texture_id) {
            g_next_texture_id = safe_texture_next;
        }
        if (safe_sampler_next > g_next_sampler_id) {
            g_next_sampler_id = safe_sampler_next;
        }
        g_free_descriptors.clear();
        g_descriptor_pools_ready = true;
    }

    bool AllocateDescriptorIdsInternal(int *out_texture_id, int *out_sampler_id) {
        EXL_ABORT_UNLESS(out_texture_id != nullptr, "Texture ID out pointer is null");
        EXL_ABORT_UNLESS(out_sampler_id != nullptr, "Sampler ID out pointer is null");

        if (!g_free_descriptors.empty()) {
            const DescriptorPair pair = g_free_descriptors.back();
            g_free_descriptors.pop_back();
            *out_texture_id = pair.texture_id;
            *out_sampler_id = pair.sampler_id;
            return true;
        }

        if (g_texture_descriptor_count <= 0 || g_sampler_descriptor_count <= 0) {
            return false;
        }
        if (g_next_texture_id >= g_texture_descriptor_count || g_next_sampler_id >= g_sampler_descriptor_count) {
            return false;
        }

        *out_texture_id = g_next_texture_id++;
        *out_sampler_id = g_next_sampler_id++;
        return true;
    }

    nvn::Format MapFormat(ImTextureFormat format) {
        switch (format) {
        case ImTextureFormat_RGBA32:
            return nvn::Format::RGBA8;
        case ImTextureFormat_Alpha8:
            // Keep the font atlas in a single-channel texture to avoid 4x GPU memory expansion.
            // Use swizzle (set at texture creation) so shaders see (1,1,1,alpha).
            return nvn::Format::R8;
        default:
            return nvn::Format::RGBA8;
        }
    }

    bool UploadTextureRegion(ImTextureData *tex, NvnImGuiTexture *backend,
                             const ImTextureRect &rect) {
        if (rect.w == 0 || rect.h == 0) {
            return true;
        }

        nvn::CopyRegion region = {
            .xoffset = rect.x,
            .yoffset = rect.y,
            .zoffset = 0,
            .width = rect.w,
            .height = rect.h,
            .depth = 1,
        };

        const unsigned char *src =
            static_cast<const unsigned char *>(tex->GetPixelsAt(rect.x, rect.y));
        const int src_pitch = tex->GetPitch();

        backend->texture.WriteTexelsStrided(nullptr, &region, src, src_pitch, 0);
        backend->texture.FlushTexels(nullptr, &region);
        return true;
    }

    bool CreateTextureFromImGui(NvnBackendData *bd, ImTextureData *tex) {
        if (!EnsureDescriptorPoolsInternal(bd)) {
            return false;
        }

        const nvn::Format format = MapFormat(tex->Format);

        NvnImGuiTexture *backend = IM_NEW(NvnImGuiTexture)();
        backend->width = tex->Width;
        backend->height = tex->Height;
        backend->format = tex->Format;

        nvn::TextureBuilder tex_builder;
        tex_builder.SetDefaults()
            .SetDevice(bd->device)
            .SetTarget(nvn::TextureTarget::TARGET_2D)
            .SetFormat(format)
            .SetSize2D(tex->Width, tex->Height);
        if (tex->Format == ImTextureFormat_Alpha8) {
            tex_builder.SetSwizzle(
                nvn::TextureSwizzle::ONE,
                nvn::TextureSwizzle::ONE,
                nvn::TextureSwizzle::ONE,
                nvn::TextureSwizzle::R
            );
        }

        const size_t storage_size = tex_builder.GetStorageSize();
        const size_t storage_align = tex_builder.GetStorageAlignment();
        if (ImGui::GetCurrentContext() != nullptr) {
            const ImGuiIO &io = ImGui::GetIO();
            if (io.Fonts != nullptr && io.Fonts->TexData == tex) {
                static bool s_logged_font_alloc = false;
                if (kLowMem_LogFontTexCreate && !s_logged_font_alloc) {
                    s_logged_font_alloc = true;
                    exl::log::PrintFmt(
                        "[D3Hack|exlaunch] [imgui_backend] Font tex create: w=%d h=%d fmt=%d bpp=%d storage=0x%zx align=0x%zx\n",
                        tex->Width,
                        tex->Height,
                        static_cast<int>(tex->Format),
                        tex->BytesPerPixel,
                        storage_size,
                        storage_align
                    );
                }
            }
        }
        if (!MemoryPoolMaker::createPoolAligned(&backend->storage_pool, storage_size, storage_align,
                                         nvn::MemoryPoolFlags::CPU_UNCACHED |
                                             nvn::MemoryPoolFlags::GPU_CACHED)) {
            IM_DELETE(backend);
            return false;
        }

        tex_builder.SetStorage(&backend->storage_pool, 0);
        if (!backend->texture.Initialize(&tex_builder)) {
            MemoryPoolMaker::destroyPool(&backend->storage_pool);
            IM_DELETE(backend);
            return false;
        }

        nvn::SamplerBuilder sampler_builder;
        sampler_builder.SetDefaults()
            .SetDevice(bd->device)
            .SetMinMagFilter(nvn::MinFilter::LINEAR, nvn::MagFilter::LINEAR)
            .SetWrapMode(nvn::WrapMode::CLAMP, nvn::WrapMode::CLAMP, nvn::WrapMode::CLAMP);

        if (!backend->sampler.Initialize(&sampler_builder)) {
            backend->texture.Finalize();
            MemoryPoolMaker::destroyPool(&backend->storage_pool);
            IM_DELETE(backend);
            return false;
        }

        if (!AllocateDescriptorIdsInternal(&backend->texture_id, &backend->sampler_id)) {
            backend->sampler.Finalize();
            backend->texture.Finalize();
            MemoryPoolMaker::destroyPool(&backend->storage_pool);
            IM_DELETE(backend);
            return false;
        }

        bd->texPool.RegisterTexture(backend->texture_id, &backend->texture, nullptr);
        bd->samplerPool.RegisterSampler(backend->sampler_id, &backend->sampler);
        backend->handle = bd->device->GetTextureHandle(backend->texture_id, backend->sampler_id);

        tex->BackendUserData = backend;
        tex->SetTexID(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(&backend->handle)));

        const ImTextureRect full_rect = {
            static_cast<unsigned short>(0),
            static_cast<unsigned short>(0),
            static_cast<unsigned short>(tex->Width),
            static_cast<unsigned short>(tex->Height),
        };
        if (!UploadTextureRegion(tex, backend, full_rect)) {
            DestroyTextureFromImGui(tex);
            return false;
        }

        tex->SetStatus(ImTextureStatus_OK);

        // NOTE: Do not free tex->Pixels here. ImGui may request updates later and will expect
        // Pixels to be valid for GetPixelsAt() during those uploads.
        return true;
    }

    bool UpdateTextureFromImGui(ImTextureData *tex) {
        auto *backend = static_cast<NvnImGuiTexture *>(tex->BackendUserData);
        if (backend == nullptr) {
            return false;
        }

        if (!UploadTextureRegion(tex, backend, tex->UpdateRect)) {
            return false;
        }

        tex->SetStatus(ImTextureStatus_OK);
        return true;
    }

    void DestroyTextureFromImGui(ImTextureData *tex) {
        auto *backend = static_cast<NvnImGuiTexture *>(tex->BackendUserData);
        if (backend == nullptr) {
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
            return;
        }

        backend->sampler.Finalize();
        backend->texture.Finalize();
        MemoryPoolMaker::destroyPool(&backend->storage_pool);
        if (backend->texture_id >= 0 && backend->sampler_id >= 0) {
            g_free_descriptors.push_back({backend->texture_id, backend->sampler_id});
        }
        IM_DELETE(backend);

        tex->BackendUserData = nullptr;
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}  // namespace

void ProcessTextures(ImDrawData *draw_data) {
    if (draw_data == nullptr || draw_data->Textures == nullptr) {
        return;
    }

    NvnBackendData *bd = getBackendData();
    if (bd == nullptr || !bd->isInitialized) {
        return;
    }
    for (ImTextureData *tex : *draw_data->Textures) {
        if (tex == nullptr) {
            continue;
        }
        switch (tex->Status) {
        case ImTextureStatus_WantCreate:
            CreateTextureFromImGui(bd, tex);
            break;
        case ImTextureStatus_WantUpdates:
            UpdateTextureFromImGui(tex);
            break;
        case ImTextureStatus_WantDestroy:
            if (tex->UnusedFrames > 0) {
                DestroyTextureFromImGui(tex);
            }
            break;
        case ImTextureStatus_OK:
        case ImTextureStatus_Destroyed:
        default:
            break;
        }
    }
}

void ShutdownTextures() {
    ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
    for (ImTextureData *tex : platform_io.Textures) {
        if (tex != nullptr && tex->BackendUserData != nullptr) {
            DestroyTextureFromImGui(tex);
        }
    }
}

void ShutdownDescriptorPools() {
    if (!g_descriptor_pools_ready) {
        return;
    }
    if (!ImGui::GetCurrentContext()) {
        g_descriptor_pools_ready = false;
        g_next_texture_id = 1;
        g_next_sampler_id = 1;
        g_texture_descriptor_count = 0;
        g_sampler_descriptor_count = 0;
        g_texture_descriptor_first = 1;
        g_sampler_descriptor_first = 1;
        g_free_descriptors.clear();
        return;
    }

    NvnBackendData *bd = static_cast<NvnBackendData *>(ImGui::GetIO().BackendRendererUserData);
    if (bd == nullptr) {
        g_descriptor_pools_ready = false;
        g_next_texture_id = 1;
        g_next_sampler_id = 1;
        g_texture_descriptor_count = 0;
        g_sampler_descriptor_count = 0;
        g_texture_descriptor_first = 1;
        g_sampler_descriptor_first = 1;
        g_free_descriptors.clear();
        return;
    }

    bd->samplerPool.Finalize();
    bd->texPool.Finalize();
    MemoryPoolMaker::destroyPool(&bd->sampTexMemPool);

    g_descriptor_pools_ready = false;
    g_next_texture_id = 1;
    g_next_sampler_id = 1;
    g_texture_descriptor_count = 0;
    g_sampler_descriptor_count = 0;
    g_texture_descriptor_first = 1;
    g_sampler_descriptor_first = 1;
    g_free_descriptors.clear();
}

bool AreDescriptorPoolsReady() {
    return g_descriptor_pools_ready;
}

bool EnsureDescriptorPools(NvnBackendData *bd) {
    return EnsureDescriptorPoolsInternal(bd);
}

bool AllocateDescriptorIds(int *out_texture_id, int *out_sampler_id) {
    NvnBackendData *bd = getBackendData();
    if (bd == nullptr || !EnsureDescriptorPoolsInternal(bd)) {
        return false;
    }
    return AllocateDescriptorIdsInternal(out_texture_id, out_sampler_id);
}

void AdoptDescriptorPools(int next_texture_id, int next_sampler_id) {
    const NvnBackendData *bd = getBackendData();
    AdoptDescriptorPoolsInternal(bd ? bd->device : nullptr, next_texture_id, next_sampler_id);
}

}  // namespace TextureSupport
}  // namespace ImguiNvnBackend
