#include "program/runtime_apply.hpp"

#include "d3/_util.hpp"
#include "d3/patches.hpp"

#include <cstdio>
#include <cstring>

namespace d3 {
    namespace {
        struct RestartRule {
            const char *note                                          = nullptr;
            bool (*changed)(const PatchConfig &, const PatchConfig &) = nullptr;
        };

        static auto ChangedResolutionHackActive(const PatchConfig &a, const PatchConfig &b) -> bool {
            return a.resolution_hack.active != b.resolution_hack.active;
        }
        static auto ChangedResolutionHackOutputScale(const PatchConfig &a, const PatchConfig &b) -> bool {
            return a.resolution_hack.output_handheld_scale != b.resolution_hack.output_handheld_scale;
        }
        static auto ChangedResolutionHackSpoofDocked(const PatchConfig &a, const PatchConfig &b) -> bool {
            return a.resolution_hack.spoof_docked != b.resolution_hack.spoof_docked;
        }
        static auto ChangedResolutionTargets(const PatchConfig &a, const PatchConfig &b) -> bool {
            return a.resolution_hack.target_resolution != b.resolution_hack.target_resolution ||
                   a.resolution_hack.OutputHandheldHeightPx() != b.resolution_hack.OutputHandheldHeightPx();
        }

        static auto ChangedEnableCrashes(const PatchConfig &a, const PatchConfig &b) -> bool {
            return a.debug.enable_crashes != b.debug.enable_crashes;
        }
        static auto ChangedAllowOnline(const PatchConfig &a, const PatchConfig &b) -> bool {
            return a.seasons.allow_online != b.seasons.allow_online;
        }
        static auto ChangedTagNx(const PatchConfig &a, const PatchConfig &b) -> bool {
            return a.debug.tagnx != b.debug.tagnx;
        }
        static auto ChangedDebugFlags(const PatchConfig &a, const PatchConfig &b) -> bool {
            return a.debug.enable_debug_flags != b.debug.enable_debug_flags;
        }
        static auto ChangedExceptionHandler(const PatchConfig &a, const PatchConfig &b) -> bool {
            return a.debug.enable_exception_handler != b.debug.enable_exception_handler;
        }
        static auto ChangedOeNotifications(const PatchConfig &a, const PatchConfig &b) -> bool {
            return a.debug.enable_oe_notification_hook != b.debug.enable_oe_notification_hook;
        }

        static constexpr RestartRule k_restart_rules[] = {
            {.note = "Resolution hack", .changed = &ChangedResolutionHackActive},
            {.note = "Resolution hack", .changed = &ChangedResolutionHackOutputScale},
            {.note = "Spoof docked", .changed = &ChangedResolutionHackSpoofDocked},
            {.note = "Resolution targets", .changed = &ChangedResolutionTargets},

            {.note = "Crash hooks", .changed = &ChangedEnableCrashes},
            {.note = "AllowOnlinePlay", .changed = &ChangedAllowOnline},
            {.note = "SpoofNetworkFunctions", .changed = &ChangedTagNx},
            {.note = "Debug flags", .changed = &ChangedDebugFlags},
            {.note = "Exception handler", .changed = &ChangedExceptionHandler},
            {.note = "OE notifications", .changed = &ChangedOeNotifications},
        };
    }  // namespace

    void ApplyPatchConfigRuntime(const PatchConfig &config, RuntimeApplyResult *out) {
        RuntimeApplyResult result {};

        const PatchConfig prev      = global_config;
        global_config               = config;
        global_config.initialized   = true;
        global_config.defaults_only = false;

        // Safe, per-frame-gated features should apply immediately just by updating global_config.
        // For enable-only static patches, re-run patch entrypoints when turning them on.

        auto append_note = [&](const char *text) -> void {
            if (text == nullptr || text[0] == '\0') {
                return;
            }
            if (result.note[0] == '\0') {
                snprintf(result.note, sizeof(result.note), "%s", text);
                return;
            }
            const size_t len = std::strlen(result.note);
            if (len + 2 >= sizeof(result.note)) {
                return;
            }
            std::strncat(result.note, ", ", sizeof(result.note) - len - 1);
            std::strncat(result.note, text, sizeof(result.note) - std::strlen(result.note) - 1);
        };

        auto require_restart_if = [&](bool cond, const char *note) -> void {
            if (!cond) {
                return;
            }
            result.restart_required = true;
            append_note(note);
        };

        // XVars that can be updated at runtime.
        XVarBool_Set(&g_varOnlineServicePTR, global_config.seasons.spoof_ptr, 3u);
        XVarBool_Set(&g_varExperimentalScheduling, global_config.resolution_hack.exp_scheduler, 3u);

        // Enable-only patches.
        if (global_config.overlays.active && global_config.overlays.buildlocker_watermark &&
            !(prev.overlays.active && prev.overlays.buildlocker_watermark)) {
            PatchBuildlocker();
            result.applied_enable_only = true;
            append_note("BuildLocker");
        } else if ((prev.overlays.active && prev.overlays.buildlocker_watermark) &&
                   !(global_config.overlays.active && global_config.overlays.buildlocker_watermark)) {
            require_restart_if(true, "BuildLocker");
        }

        // Dynamic/runtime patches.
        if (global_config.events.active) {
            PatchDynamicEvents();
        } else if (prev.events.active) {
            require_restart_if(true, "Events");
        }

        if (global_config.seasons.active) {
            PatchDynamicSeasonal();
        } else if (prev.seasons.active) {
            require_restart_if(true, "Seasons");
        }

        for (const auto &rule : k_restart_rules) {
            if (rule.note == nullptr || rule.note[0] == '\0' || rule.changed == nullptr) {
                continue;
            }
            require_restart_if(rule.changed(prev, global_config), rule.note);
        }

        if (out != nullptr) {
            *out = result;
        }
    }

}  // namespace d3
