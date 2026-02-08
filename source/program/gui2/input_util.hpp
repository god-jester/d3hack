#pragma once

#include "types.h"

namespace d3::gui2 {

    inline auto NormalizeStickComponentS16(s32 v) -> float {
        constexpr float kMax = 32767.0f;
        float           out  = static_cast<float>(v) / kMax;
        if (out < -1.0f) {
            out = -1.0f;
        }
        if (out > 1.0f) {
            out = 1.0f;
        }
        return out;
    }

}  // namespace d3::gui2
