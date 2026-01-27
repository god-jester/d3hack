#pragma once

#include "d3/types/common.hpp"
#include "d3/types/bd_memory.hpp"
#include "symbols/refstring.hpp"

#define _FUNC_PREFIX(name) dut::func_ptrs::name

#define _FUNC_TYPE(name)   APPEND(_FUNC_PREFIX(name), _t)

#define _FUNC_KEY(name)    "sym_" #name

#define FUNC_PTR(name, ...)                   \
    namespace dut::func_ptrs {                \
        using APPEND(name, _t) = __VA_ARGS__; \
    }                                         \
    extern _FUNC_TYPE(name) name

#define SETUP_FUNC_PTR(name) \
    _FUNC_TYPE(name)         \
    name = reinterpret_cast<_FUNC_TYPE(name)>(GameOffsetFromTable(_FUNC_KEY(name)))

template<typename T, size_t Size = sizeof(T)>
std::array<std::byte, Size> XorEncrypt(T value, std::byte key) {  // NOLINT
    auto bytes = std::bit_cast<std::array<std::byte, Size>>(value);

    for (auto &b : bytes) {
        b ^= key;
    }

    return bytes;
}

template<typename T, size_t Size = sizeof(T)>
auto XorDecrypt(const std::array<std::byte, Size> &encryptedBytes, std::byte key) -> T {
    std::array<std::byte, Size> decryptedBytes;

    for (size_t i = 0; i < Size; ++i) {
        decryptedBytes[i] = encryptedBytes[i] ^ key;
    }

    return std::bit_cast<T>(decryptedBytes);
}
