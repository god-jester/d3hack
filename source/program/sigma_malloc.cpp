#include <atomic>
#include <cstddef>
#include <cstring>
#include <limits>

#include "symbols/common.hpp"

namespace {

    constexpr size_t kSigmaMinAlign = 0x20;
    constexpr size_t kBootAllocSize = 0x4000;

    alignas(kSigmaMinAlign) unsigned char s_boot_alloc[kBootAllocSize];
    std::atomic<size_t> s_boot_offset {0};

    auto AlignUp(size_t value, size_t alignment) -> size_t {
        return (value + (alignment - 1)) & ~(alignment - 1);
    }

    auto NormalizeAlignment(size_t alignment) -> size_t {
        if (alignment < kSigmaMinAlign) {
            alignment = kSigmaMinAlign;
        }
        if ((alignment & (alignment - 1)) != 0) {
            return 0;
        }
        return alignment;
    }

    auto BootAlloc(size_t size, size_t alignment) -> void * {
        if (size == 0) {
            return nullptr;
        }

        alignment = NormalizeAlignment(alignment);
        if (alignment == 0) {
            return nullptr;
        }

        if (size > (std::numeric_limits<size_t>::max() - (alignment - 1))) {
            return nullptr;
        }

        size = AlignUp(size, alignment);

        size_t offset = s_boot_offset.load(std::memory_order_relaxed);
        while (true) {
            size_t const aligned_offset = AlignUp(offset, alignment);
            size_t const next_offset    = aligned_offset + size;
            if (next_offset > kBootAllocSize) {
                return nullptr;
            }
            if (s_boot_offset.compare_exchange_weak(offset, next_offset, std::memory_order_acq_rel)) {
                return s_boot_alloc + aligned_offset;
            }
        }
    }

    auto IsBootPtr(const void *ptr) -> bool {
        auto p = reinterpret_cast<const unsigned char *>(ptr);
        return p >= s_boot_alloc && p < (s_boot_alloc + kBootAllocSize);
    }

    auto SigmaAlloc(size_t size, size_t alignment, bool clear) -> void * {
        if (size == 0) {
            return nullptr;
        }

        alignment = NormalizeAlignment(alignment);
        if (alignment == 0) {
            return nullptr;
        }

        if (size > (std::numeric_limits<size_t>::max() - (alignment - 1))) {
            return nullptr;
        }

        size = AlignUp(size, alignment);

        if (SigmaMemoryNew != nullptr) {
            return SigmaMemoryNew(size, alignment, nullptr, clear ? 1 : 0);
        }

        void *ptr = BootAlloc(size, alignment);
        if (ptr != nullptr && clear) {
            std::memset(ptr, 0, size);
        }
        return ptr;
    }

}  // namespace

extern "C" auto malloc(size_t size) -> void * {
    return SigmaAlloc(size, kSigmaMinAlign, false);
}

extern "C" auto aligned_alloc(size_t alignment, size_t size) -> void * {
    return SigmaAlloc(size, alignment, false);
}

extern "C" auto calloc(size_t count, size_t size) -> void * {
    if (count == 0 || size == 0) {
        return nullptr;
    }
    if (count > (std::numeric_limits<size_t>::max() / size)) {
        return nullptr;
    }
    return SigmaAlloc(count * size, kSigmaMinAlign, true);
}

extern "C" void free(void *ptr) {
    if (ptr == nullptr) {
        return;
    }
    if (IsBootPtr(ptr)) {
        return;
    }
    if (SigmaMemoryFree != nullptr) {
        SigmaMemoryFree(ptr, nullptr);
    }
}
