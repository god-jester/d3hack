#pragma once

#include "nn/nn_common.hpp"

namespace nn::os {
    void SetDyingMessageRegion(uintptr_t address, size_t size);
}
