#pragma once

#include "program/config.hpp"

#include <span>

namespace d3::hook_registry {

    enum class ToggleSafety : u8 {
        RuntimeSafe,
        RestartRequired,
    };

    struct HookEntry {
        const char *name = nullptr;

        // "Ownership" is meant for humans: who owns this hook entry and which feature it belongs to.
        // Keep these stable so boot reports remain easy to diff.
        const char *owner   = nullptr;  // e.g. "d3/hooks/util"
        const char *feature = nullptr;  // e.g. "resolution_hack.active"

        void (*install)()                    = nullptr;
        bool (*enabled)(const PatchConfig &) = nullptr;

        ToggleSafety toggle_safety = ToggleSafety::RestartRequired;
    };

    auto Entries() -> std::span<const HookEntry>;

}  // namespace d3::hook_registry
