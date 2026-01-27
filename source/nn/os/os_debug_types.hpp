#pragma once

#include "nn/nn_common.hpp"

namespace nn::os {

    struct MemoryInfo {
        u64    totalAvailableMemorySize;
        size_t totalUsedMemorySize;
        size_t totalMemoryHeapSize;
        size_t allocatedMemoryHeapSize;
        size_t programSize;
        size_t totalThreadStackSize;
        int    threadCount;
    };

}  // namespace nn::os
