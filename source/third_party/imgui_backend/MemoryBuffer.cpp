#include "imgui_backend/MemoryBuffer.h"
#include "imgui_backend/imgui_impl_nvn.hpp"

// PRINT macros
#include "program/d3/setting.hpp"

#include <cstdarg>

namespace {
    void DebugPrint(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

    void DebugPrint(const char *fmt, ...) {
        std::va_list vl;
        va_start(vl, fmt);
        exl::log::PrintV(fmt, vl);
        va_end(vl);
    }

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
}

MemoryBuffer::MemoryBuffer(size_t size) {

    auto *bd = ImguiNvnBackend::getBackendData();

    const size_t pool_alignment = GetDevicePageSizeOrFallback(bd->device);
    const size_t alignedSize = ALIGN_UP(size, pool_alignment);

    void *rawPtr = IM_ALLOC(alignedSize + (pool_alignment - 1));
    if (rawPtr == nullptr) {
        DebugPrint("[D3Hack|exlaunch] [imgui_backend] MemoryBuffer IM_ALLOC failed (size=0x%zx)\n", alignedSize);
        return;
    }
    rawBuffer = rawPtr;
    ownsBuffer = true;
    memBuffer = reinterpret_cast<void *>(ALIGN_UP(reinterpret_cast<uintptr_t>(rawPtr), pool_alignment));
    memset(memBuffer, 0, alignedSize);

    const bool isAligned = ((reinterpret_cast<uintptr_t>(memBuffer) & (pool_alignment - 1)) == 0);
    if (!isAligned) {
        DebugPrint("[D3Hack|exlaunch] [imgui_backend] ERROR: MemoryBuffer backing is not 0x%zx-aligned (ptr=%p)\n",
                   pool_alignment, memBuffer);
        return;
    }

    bd->memPoolBuilder.SetDefaults()
            .SetDevice(bd->device)
            .SetFlags(nvn::MemoryPoolFlags::CPU_UNCACHED | nvn::MemoryPoolFlags::GPU_CACHED)
            .SetStorage(memBuffer, alignedSize);

    if (!pool.Initialize(&bd->memPoolBuilder)) {
        DebugPrint("[D3Hack|exlaunch] [imgui_backend] Failed to Create Memory Pool!\n");
        return;
    }

    bd->bufferBuilder.SetDevice(bd->device).SetDefaults().SetStorage(&pool, 0, alignedSize);

    if (!buffer.Initialize(&bd->bufferBuilder)) {
        DebugPrint("[D3Hack|exlaunch] [imgui_backend] Failed to Init Buffer!\n");
        return;
    }

    mIsReady = true;
}

MemoryBuffer::MemoryBuffer(size_t size, nvn::MemoryPoolFlags flags) {

    auto *bd = ImguiNvnBackend::getBackendData();

    const size_t pool_alignment = GetDevicePageSizeOrFallback(bd->device);
    const bool is_shader_code = (int(flags) & int(nvn::MemoryPoolFlags::SHADER_CODE)) != 0;
    const size_t shader_pad = is_shader_code ? GetShaderCodePaddingOrZero(bd->device) : 0;

    const size_t alignedSize = ALIGN_UP(size + shader_pad, pool_alignment);

    void *rawPtr = IM_ALLOC(alignedSize + (pool_alignment - 1));
    if (rawPtr == nullptr) {
        DebugPrint("[D3Hack|exlaunch] [imgui_backend] MemoryBuffer IM_ALLOC failed (size=0x%zx)\n", alignedSize);
        return;
    }
    rawBuffer = rawPtr;
    ownsBuffer = true;
    memBuffer = reinterpret_cast<void *>(ALIGN_UP(reinterpret_cast<uintptr_t>(rawPtr), pool_alignment));
    memset(memBuffer, 0, alignedSize);

    const bool isAligned = ((reinterpret_cast<uintptr_t>(memBuffer) & (pool_alignment - 1)) == 0);
    if (!isAligned) {
        DebugPrint("[D3Hack|exlaunch] [imgui_backend] ERROR: MemoryBuffer backing is not 0x%zx-aligned (ptr=%p)\n",
                   pool_alignment, memBuffer);
        return;
    }

    bd->memPoolBuilder.SetDefaults()
            .SetDevice(bd->device)
            .SetFlags(flags)
            .SetStorage(memBuffer, alignedSize);

    if (!pool.Initialize(&bd->memPoolBuilder)) {
        DebugPrint("[D3Hack|exlaunch] [imgui_backend] Failed to Create Memory Pool!\n");
        return;
    }

    bd->bufferBuilder.SetDevice(bd->device).SetDefaults().SetStorage(&pool, 0, alignedSize);

    if (!buffer.Initialize(&bd->bufferBuilder)) {
        DebugPrint("[D3Hack|exlaunch] [imgui_backend] Failed to Init Buffer!\n");
        return;
    }

    mIsReady = true;
}

MemoryBuffer::MemoryBuffer(size_t size, void *bufferPtr, nvn::MemoryPoolFlags flags) {

    auto *bd = ImguiNvnBackend::getBackendData();

    memBuffer = bufferPtr;
    rawBuffer = bufferPtr;
    ownsBuffer = false;

    bd->memPoolBuilder.SetDefaults()
            .SetDevice(bd->device)
            .SetFlags(flags)
            .SetStorage(memBuffer, size);

    if (!pool.Initialize(&bd->memPoolBuilder)) {
        DebugPrint("[imgui_backend] Failed to Create Memory Pool!\n");
        return;
    }

    bd->bufferBuilder.SetDevice(bd->device).SetDefaults().SetStorage(&pool, 0, size);

    if (!buffer.Initialize(&bd->bufferBuilder)) {
        DebugPrint("[imgui_backend] Failed to Init Buffer!\n");
        return;
    }

    mIsReady = true;
}

void MemoryBuffer::Finalize() {
    if (ownsBuffer && rawBuffer != nullptr) {
        IM_FREE(rawBuffer);
    }
    pool.Finalize();
    buffer.Finalize();
}

void MemoryBuffer::ClearBuffer() {
    memset(memBuffer, 0, pool.GetSize());
}
