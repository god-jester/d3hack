#pragma once

#include "types.h"
#include <type_traits>

namespace nn::util {

    template<s32 size, typename T>
    struct BitFlagSet {
        using type = std::conditional_t<size <= 32, u32, u64>;
        static constexpr int storageBits  = static_cast<int>(sizeof(type)) * 8;
        static constexpr int storageCount = static_cast<int>((size + storageBits - 1)) / storageBits;

        type field[storageCount]{};

        inline bool isBitSet(T index) const {
            const auto bit = static_cast<u64>(index);
            return (field[bit / storageBits] & (static_cast<type>(1) << (bit % storageBits))) != 0;
        }
    };

}  // namespace nn::util
