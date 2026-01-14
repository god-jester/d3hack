#pragma once

#include "program/config.hpp"

namespace d3 {

    struct RuntimeApplyResult {
        bool restart_required    = false;
        bool applied_enable_only = false;
        char note[256] {};
    };

    void ApplyPatchConfigRuntime(const PatchConfig &config, RuntimeApplyResult *out);

}  // namespace d3
