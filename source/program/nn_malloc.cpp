#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include "program/system_allocator.hpp"

namespace {
    constexpr size_t kSystemMinAlign = 0x20;

    struct AllocHeader {
        void *raw;
    };

    auto IsPowerOfTwo(size_t value) -> bool {
        return value != 0 && (value & (value - 1)) == 0;
    }

    auto AlignUp(uintptr_t value, size_t alignment) -> uintptr_t {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    auto AllocateAligned(nn::mem::StandardAllocator *allocator, size_t size, size_t alignment, bool clear) -> void * {
        if (allocator == nullptr) {
            return nullptr;
        }
        if (!IsPowerOfTwo(alignment) || alignment < alignof(void *)) {
            return nullptr;
        }

        const size_t header_size = sizeof(AllocHeader);
        const size_t max_size    = std::numeric_limits<size_t>::max();
        if (size > max_size - alignment - header_size) {
            return nullptr;
        }

        const size_t total = size + alignment + header_size;
        void        *raw   = allocator->Allocate(total);
        if (raw == nullptr) {
            return nullptr;
        }

        const uintptr_t start   = reinterpret_cast<uintptr_t>(raw) + header_size;
        const uintptr_t aligned = AlignUp(start, alignment);
        auto           *header  = reinterpret_cast<AllocHeader *>(aligned - header_size);
        header->raw             = raw;

        void *out = reinterpret_cast<void *>(aligned);
        if (clear && size > 0) {
            std::memset(out, 0, size);
        }
        return out;
    }

    auto FreeAligned(nn::mem::StandardAllocator *allocator, void *ptr) -> void {
        if (allocator == nullptr || ptr == nullptr) {
            return;
        }
        const uintptr_t aligned = reinterpret_cast<uintptr_t>(ptr);
        auto           *header  = reinterpret_cast<AllocHeader *>(aligned - sizeof(AllocHeader));
        allocator->Free(header->raw);
    }
}  // namespace

extern "C" auto malloc(size_t size) -> void * {
    return AllocateAligned(d3::system_allocator::GetSystemAllocator(), size, kSystemMinAlign, false);
}

extern "C" auto aligned_alloc(size_t alignment, size_t size) -> void * {
    if (alignment == 0) {
        return nullptr;
    }
    return AllocateAligned(d3::system_allocator::GetSystemAllocator(), size, alignment, false);
}

extern "C" auto calloc(size_t count, size_t size) -> void * {
    if (count == 0 || size == 0) {
        return nullptr;
    }
    if (count > (std::numeric_limits<size_t>::max() / size)) {
        return nullptr;
    }
    return AllocateAligned(d3::system_allocator::GetSystemAllocator(), count * size, kSystemMinAlign, true);
}

extern "C" void free(void *ptr) {
    FreeAligned(d3::system_allocator::GetSystemAllocator(), ptr);
}
