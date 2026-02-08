#include "program/log_once.hpp"

#include "types.h"

#include <array>
#include <atomic>
#include <cstddef>

namespace d3::log_once {
    namespace {
        constexpr size_t kMaxKeys = 128;

        alignas(8) std::array<std::atomic<u64>, kMaxKeys> g_keys {};

        static auto Fnv1a64(const char *s) -> u64 {
            // 64-bit FNV-1a.
            u64 hash = 14695981039346656037ull;
            for (const char *p = s; p != nullptr && *p != '\0'; ++p) {
                hash ^= static_cast<unsigned char>(*p);
                hash *= 1099511628211ull;
            }
            return hash;
        }
    }  // namespace

    auto ShouldLog(const char *key) -> bool {
        if (key == nullptr || key[0] == '\0') {
            return true;
        }

        u64 hash = Fnv1a64(key);
        if (hash == 0) {
            hash = 1;
        }

        const size_t start = static_cast<size_t>(hash % kMaxKeys);
        for (size_t step = 0; step < kMaxKeys; ++step) {
            const size_t idx = (start + step) % kMaxKeys;

            u64 expected = 0;
            if (g_keys[idx].compare_exchange_strong(expected, hash, std::memory_order_acq_rel)) {
                return true;
            }
            if (expected == hash) {
                return false;
            }
        }

        // Table full; best-effort behavior is to allow the log.
        return true;
    }

}  // namespace d3::log_once
