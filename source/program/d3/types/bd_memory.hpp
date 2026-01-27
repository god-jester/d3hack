#pragma once

#include <cstddef>

namespace d3 {
    using BdAllocFn = void *(*)(size_t size);
    using BdFreeFn = void (*)(void *ptr);
    using BdReallocFn = void *(*)(void *ptr, size_t size);
    using BdAlignedAllocFn = void *(*)(size_t size, size_t alignment);
    using BdAlignedFreeFn = void (*)(void *ptr);
    using BdAlignedReallocFn = void *(*)(void *ptr, size_t size, size_t alignment);
}
