#pragma once

namespace d3::log_once {

    // Returns true the first time a key is seen (best-effort). Intended for
    // "log once" gates without heap allocation.
    auto ShouldLog(const char *key) -> bool;

}  // namespace d3::log_once
