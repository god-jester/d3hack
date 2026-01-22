#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "imgui_backend/MemoryPoolMaker.h"
#include "imgui_backend/imgui_impl_nvn.hpp"
#include "imgui/imgui.h"
#include "lib/diag/assert.hpp"

namespace ImguiNvnBackend {
namespace TextureSupport {

namespace {
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
        bool expanded_alpha = false;
    };

    struct DescriptorPair {
        int texture_id = -1;
        int sampler_id = -1;
    };

    bool g_descriptor_pools_ready = false;
    int g_next_texture_id = 1;
    int g_next_sampler_id = 1;
    std::vector<DescriptorPair> g_free_descriptors;

    bool EnsureDescriptorPoolsInternal(NvnBackendData *bd) {
        if (g_descriptor_pools_ready) {
            return true;
        }

        int sampler_desc_size = 0;
        int texture_desc_size = 0;
        bd->device->GetInteger(nvn::DeviceInfo::SAMPLER_DESCRIPTOR_SIZE, &sampler_desc_size);
        bd->device->GetInteger(nvn::DeviceInfo::TEXTURE_DESCRIPTOR_SIZE, &texture_desc_size);

        const int sampler_pool_size = sampler_desc_size * MaxSampDescriptors;
        const int texture_pool_size = texture_desc_size * MaxTexDescriptors;
        const int total_pool_size = ALIGN_UP(sampler_pool_size + texture_pool_size, 0x1000);

        if (!MemoryPoolMaker::createPool(&bd->sampTexMemPool, total_pool_size)) {
            return false;
        }
        if (!bd->samplerPool.Initialize(&bd->sampTexMemPool, 0, MaxSampDescriptors)) {
            return false;
        }
        if (!bd->texPool.Initialize(&bd->sampTexMemPool, sampler_pool_size, MaxTexDescriptors)) {
            return false;
        }

        g_next_texture_id = 1;
        g_next_sampler_id = 1;
        g_free_descriptors.clear();
        g_descriptor_pools_ready = true;
        return true;
    }

    void AdoptDescriptorPoolsInternal(int next_texture_id, int next_sampler_id) {
        if (next_texture_id > g_next_texture_id) {
            g_next_texture_id = next_texture_id;
        }
        if (next_sampler_id > g_next_sampler_id) {
            g_next_sampler_id = next_sampler_id;
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

        if (g_next_texture_id >= MaxTexDescriptors || g_next_sampler_id >= MaxSampDescriptors) {
            return false;
        }

        *out_texture_id = g_next_texture_id++;
        *out_sampler_id = g_next_sampler_id++;
        return true;
    }

    nvn::Format MapFormat(ImTextureFormat format, bool *out_expand_alpha) {
        EXL_ABORT_UNLESS(out_expand_alpha != nullptr, "expand_alpha pointer is null");

        switch (format) {
        case ImTextureFormat_RGBA32:
            *out_expand_alpha = false;
            return nvn::Format::RGBA8;
        case ImTextureFormat_Alpha8:
            *out_expand_alpha = true;
            return nvn::Format::RGBA8;
        default:
            *out_expand_alpha = false;
            return nvn::Format::RGBA8;
        }
    }

    void ExpandAlphaToRgba(const unsigned char *src, int src_pitch, int width, int height,
                           std::vector<unsigned char> *out_rgba) {
        EXL_ABORT_UNLESS(src != nullptr, "Alpha source is null");
        EXL_ABORT_UNLESS(out_rgba != nullptr, "RGBA buffer is null");

        const size_t out_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
        out_rgba->assign(out_size, 0);

        for (int y = 0; y < height; ++y) {
            const unsigned char *row_src = src + y * src_pitch;
            unsigned char *row_dst = out_rgba->data() + (static_cast<size_t>(y) * width * 4u);
            for (int x = 0; x < width; ++x) {
                const unsigned char alpha = row_src[x];
                row_dst[x * 4 + 0] = 0xFF;
                row_dst[x * 4 + 1] = 0xFF;
                row_dst[x * 4 + 2] = 0xFF;
                row_dst[x * 4 + 3] = alpha;
            }
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

        if (!backend->expanded_alpha) {
            backend->texture.WriteTexelsStrided(nullptr, &region, src, src_pitch, 0);
            backend->texture.FlushTexels(nullptr, &region);
            return true;
        }

        std::vector<unsigned char> rgba;
        ExpandAlphaToRgba(src, src_pitch, rect.w, rect.h, &rgba);
        const ptrdiff_t rgba_pitch = static_cast<ptrdiff_t>(rect.w) * 4;
        backend->texture.WriteTexelsStrided(nullptr, &region, rgba.data(), rgba_pitch, 0);
        backend->texture.FlushTexels(nullptr, &region);
        return true;
    }

    bool CreateTextureFromImGui(NvnBackendData *bd, ImTextureData *tex) {
        if (!EnsureDescriptorPoolsInternal(bd)) {
            return false;
        }

        bool expand_alpha = false;
        const nvn::Format format = MapFormat(tex->Format, &expand_alpha);

        NvnImGuiTexture *backend = IM_NEW(NvnImGuiTexture)();
        backend->width = tex->Width;
        backend->height = tex->Height;
        backend->format = tex->Format;
        backend->expanded_alpha = expand_alpha;

        nvn::TextureBuilder tex_builder;
        tex_builder.SetDefaults()
            .SetDevice(bd->device)
            .SetTarget(nvn::TextureTarget::TARGET_2D)
            .SetFormat(format)
            .SetSize2D(tex->Width, tex->Height);

        const size_t storage_size = tex_builder.GetStorageSize();
        const size_t storage_align = tex_builder.GetStorageAlignment();
        const size_t pool_align = std::max<size_t>(0x1000, storage_align);
        const size_t pool_size = ALIGN_UP(storage_size, pool_align);
        if (!MemoryPoolMaker::createPool(&backend->storage_pool, pool_size,
                                         nvn::MemoryPoolFlags::CPU_UNCACHED |
                                             nvn::MemoryPoolFlags::GPU_CACHED)) {
            IM_DELETE(backend);
            return false;
        }

        tex_builder.SetStorage(&backend->storage_pool, 0);
        if (!backend->texture.Initialize(&tex_builder)) {
            backend->storage_pool.Finalize();
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
            backend->storage_pool.Finalize();
            IM_DELETE(backend);
            return false;
        }

        if (!AllocateDescriptorIdsInternal(&backend->texture_id, &backend->sampler_id)) {
            backend->sampler.Finalize();
            backend->texture.Finalize();
            backend->storage_pool.Finalize();
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
            return false;
        }

        tex->SetStatus(ImTextureStatus_OK);
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
        backend->storage_pool.Finalize();
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
        g_free_descriptors.clear();
        return;
    }

    NvnBackendData *bd = static_cast<NvnBackendData *>(ImGui::GetIO().BackendRendererUserData);
    if (bd == nullptr) {
        g_descriptor_pools_ready = false;
        g_next_texture_id = 1;
        g_next_sampler_id = 1;
        g_free_descriptors.clear();
        return;
    }

    bd->samplerPool.Finalize();
    bd->texPool.Finalize();
    bd->sampTexMemPool.Finalize();

    g_descriptor_pools_ready = false;
    g_next_texture_id = 1;
    g_next_sampler_id = 1;
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
    AdoptDescriptorPoolsInternal(next_texture_id, next_sampler_id);
}

}  // namespace TextureSupport
}  // namespace ImguiNvnBackend
