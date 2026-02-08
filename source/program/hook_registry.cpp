#include "program/hook_registry.hpp"

// Avoid including hook headers here: some hook headers provide non-inline
// function definitions and will cause multiple-definition link failures if
// pulled into multiple .cpp TUs.
namespace d3 {
    void SetupUtilityHooks();
    void SetupResolutionHooks();
    void SetupDebuggingHooks();
    void SetupSeasonEventHooks();
    void SetupLobbyHooks();
}  // namespace d3

namespace d3::hook_registry {
    namespace {
        static auto Always(const PatchConfig & /*config*/) -> bool {
            return true;
        }

        static auto EnabledResolutionHooks(const PatchConfig &config) -> bool {
            return config.resolution_hack.active;
        }

        static auto EnabledDebuggingHooks(const PatchConfig &config) -> bool {
            if (config.challenge_rifts.active) {
                return true;
            }
            if (!config.debug.active) {
                return false;
            }
            return config.debug.enable_pubfile_dump ||
                   config.debug.enable_error_traces ||
                   config.debug.tagnx ||
                   config.debug.enable_debug_flags ||
                   config.debug.enable_exception_handler ||
                   config.debug.enable_oe_notification_hook;
        }

        static auto EnabledLobbyHooks(const PatchConfig &config) -> bool {
            return config.debug.enable_crashes;
        }

        static constexpr HookEntry k_entries[] = {
            {
                .name          = "UtilityHooks",
                .owner         = "d3/hooks/util",
                .feature       = "misc",
                .install       = &SetupUtilityHooks,
                .enabled       = &Always,
                .toggle_safety = ToggleSafety::RestartRequired,
            },
            {
                .name          = "ResolutionHooks",
                .owner         = "d3/hooks/resolution",
                .feature       = "resolution_hack.active",
                .install       = &SetupResolutionHooks,
                .enabled       = &EnabledResolutionHooks,
                .toggle_safety = ToggleSafety::RestartRequired,
            },
            {
                .name          = "DebuggingHooks",
                .owner         = "d3/hooks/debug",
                .feature       = "debug.active or challenge_rifts.active",
                .install       = &SetupDebuggingHooks,
                .enabled       = &EnabledDebuggingHooks,
                .toggle_safety = ToggleSafety::RestartRequired,
            },
            {
                .name    = "SeasonEventHooks",
                .owner   = "d3/hooks/season_events",
                .feature = "events/seasons pubfile overrides",
                .install = &SetupSeasonEventHooks,
                .enabled = &Always,
                // Hooks are installed once at boot; feature behavior is gated by config at runtime.
                .toggle_safety = ToggleSafety::RuntimeSafe,
            },
            {
                .name          = "LobbyHooks",
                .owner         = "d3/hooks/lobby",
                .feature       = "debug.enable_crashes",
                .install       = &SetupLobbyHooks,
                .enabled       = &EnabledLobbyHooks,
                .toggle_safety = ToggleSafety::RestartRequired,
            },
        };
    }  // namespace

    auto Entries() -> std::span<const HookEntry> {
        return std::span<const HookEntry>(k_entries);
    }

}  // namespace d3::hook_registry
