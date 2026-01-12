#include "MemoryPoolMaker.h"
#include "imgui_impl_nvn.hpp"

#include "program/logging.hpp"

namespace {
    size_t GetDevicePageSizeOrFallback(const nvn::Device *device) {
        int page_size = 0;
        if (device != nullptr) {
            device->GetInteger(nvn::DeviceInfo::MEMORY_POOL_PAGE_SIZE, &page_size);
        }
        const size_t page_size_u = page_size > 0 ? static_cast<size_t>(page_size) : 0;
        const bool is_power_of_two = page_size_u != 0 && ((page_size_u & (page_size_u - 1)) == 0);
        return is_power_of_two ? page_size_u : 0x1000;
    }

    size_t GetShaderCodePaddingOrZero(const nvn::Device *device) {
        int pad = 0;
        if (device != nullptr) {
            device->GetInteger(nvn::DeviceInfo::SHADER_CODE_MEMORY_POOL_PADDING_SIZE, &pad);
        }
        return pad > 0 ? static_cast<size_t>(pad) : 0;
    }
}  // namespace

bool MemoryPoolMaker::createPool(nvn::MemoryPool *result, size_t size,
                                 const nvn::MemoryPoolFlags &flags) {

    auto bd = ImguiNvnBackend::getBackendData();

    const size_t pool_alignment = GetDevicePageSizeOrFallback(bd->device);
    const bool is_shader_code = (int(flags) & int(nvn::MemoryPoolFlags::SHADER_CODE)) != 0;
    const size_t shader_pad = is_shader_code ? GetShaderCodePaddingOrZero(bd->device) : 0;

    const size_t aligned_size = ALIGN_UP(size + shader_pad, pool_alignment);
    void *rawPtr = IM_ALLOC(aligned_size + (pool_alignment - 1));
    if (rawPtr == nullptr) {
        exl::log::PrintFmt(
            "[D3Hack|exlaunch] [imgui_backend] ERROR: MemoryPoolMaker::createPool IM_ALLOC failed (size=0x%zx aligned=0x%zx)\n",
            size, aligned_size);
        return false;
    }

    void *poolPtr = reinterpret_cast<void *>(ALIGN_UP(reinterpret_cast<uintptr_t>(rawPtr), pool_alignment));

    const uintptr_t poolAddr = reinterpret_cast<uintptr_t>(poolPtr);
    const bool isAligned = ((poolAddr & (pool_alignment - 1)) == 0);
    if (!isAligned) {
        exl::log::PrintFmt(
            "[D3Hack|exlaunch] [imgui_backend] ERROR: mempool storage pointer is not 0x%zx-aligned; refusing to Initialize (storage=%p)\n",
            pool_alignment, poolPtr);
        return false;
    }

    nvn::MemoryPoolBuilder memPoolBuilder{};
    memPoolBuilder.SetDefaults().SetDevice(bd->device).SetFlags(flags).SetStorage(poolPtr, aligned_size);

    if (!result->Initialize(&memPoolBuilder)) {
        exl::log::PrintFmt("[D3Hack|exlaunch] [imgui_backend] ERROR: MemoryPoolMaker::createPool Initialize FAILED\n");
        return false;
    }

    return true;
}
