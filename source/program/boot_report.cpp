#include "program/boot_report.hpp"

#include "program/config.hpp"
#include "program/hook_registry.hpp"
#include "program/d3/setting.hpp"

#include <cstddef>

namespace d3::boot_report {
    namespace {
        constexpr size_t     kMaxEntries   = 64;
        constexpr const char kConfigPath[] = "sd:/config/d3hack-nx/config.toml";

        const char *g_hooks[kMaxEntries]   = {};
        const char *g_patches[kMaxEntries] = {};
        size_t      g_hook_count           = 0;
        size_t      g_patch_count          = 0;
        bool        g_printed              = false;

        static void Record(const char *name, const char **arr, size_t &count) {
            if (name == nullptr || name[0] == '\0') {
                return;
            }
            if (count >= kMaxEntries) {
                return;
            }
            arr[count++] = name;
        }

        static void PrintConfigSummary() {
            PRINT("d3hack: ver=%s client=%s", D3HACK_VER, D3CLIENT_VER)
            PRINT("build: git=%s compiled=%s %s", D3HACK_GIT_DESCRIBE, __DATE__, __TIME__)
            PRINT(
                "config: path=%s initialized=%u defaults_only=%u",
                kConfigPath,
                static_cast<u32>(global_config.initialized),
                static_cast<u32>(global_config.defaults_only)
            )
            PRINT(
                "gui: enabled=%u visible=%u left_stick_passthrough=%u lang_override=%s",
                static_cast<u32>(global_config.gui.enabled),
                static_cast<u32>(global_config.gui.visible),
                static_cast<u32>(global_config.gui.allow_left_stick_passthrough),
                global_config.gui.language_override.empty() ? "(none)" : global_config.gui.language_override.c_str()
            )
            PRINT(
                "features: reshack=%u overlays=%u seasons=%u events=%u rare_cheats=%u loot_modifiers=%u debug=%u tagnx=%u",
                static_cast<u32>(global_config.resolution_hack.active),
                static_cast<u32>(global_config.overlays.active),
                static_cast<u32>(global_config.seasons.active),
                static_cast<u32>(global_config.events.active),
                static_cast<u32>(global_config.rare_cheats.active),
                static_cast<u32>(global_config.loot_modifiers.active),
                static_cast<u32>(global_config.debug.active),
                static_cast<u32>(global_config.debug.tagnx)
            )

            PRINT(
                "overlays: buildlocker=%u ddm_labels=%u fps_label=%u var_res_label=%u",
                static_cast<u32>(global_config.overlays.buildlocker_watermark),
                static_cast<u32>(global_config.overlays.ddm_labels),
                static_cast<u32>(global_config.overlays.fps_label),
                static_cast<u32>(global_config.overlays.var_res_label)
            )
            PRINT(
                "debug: crashes=%u pubfile_dump=%u error_traces=%u debug_flags=%u exception_handler=%u oe_notify=%u oe_notify_log=%u",
                static_cast<u32>(global_config.debug.enable_crashes),
                static_cast<u32>(global_config.debug.enable_pubfile_dump),
                static_cast<u32>(global_config.debug.enable_error_traces),
                static_cast<u32>(global_config.debug.enable_debug_flags),
                static_cast<u32>(global_config.debug.enable_exception_handler),
                static_cast<u32>(global_config.debug.enable_oe_notification_hook),
                static_cast<u32>(global_config.debug.log_oe_notification_messages)
            )
            PRINT("seasons: active=%u season=%u allow_online=%u spoof_ptr=%u", static_cast<u32>(global_config.seasons.active), global_config.seasons.current_season, static_cast<u32>(global_config.seasons.allow_online), static_cast<u32>(global_config.seasons.spoof_ptr))
            PRINT("events: active=%u map_mode=%u", static_cast<u32>(global_config.events.active), static_cast<u32>(global_config.events.SeasonMapMode))
            PRINT("challenge_rifts: active=%u random=%u range=[%u,%u]", static_cast<u32>(global_config.challenge_rifts.active), static_cast<u32>(global_config.challenge_rifts.random), global_config.challenge_rifts.range_start, global_config.challenge_rifts.range_end)
        }

        static const char *ToggleSafetyName(hook_registry::ToggleSafety safety) {
            switch (safety) {
            case hook_registry::ToggleSafety::RuntimeSafe:
                return "runtime_safe";
            case hook_registry::ToggleSafety::RestartRequired:
                return "restart_required";
            }
            return "unknown";
        }

        static void PrintHookPlan() {
            PRINT_LINE("hook plan (config -> installed at boot):");
            for (const auto &entry : hook_registry::Entries()) {
                if (entry.name == nullptr || entry.name[0] == '\0') {
                    continue;
                }
                const bool enabled = (entry.enabled == nullptr) ? true : entry.enabled(global_config);
                PRINT(
                    "  - %s: enabled=%u safety=%s owner=%s feature=%s",
                    entry.name,
                    static_cast<u32>(enabled),
                    ToggleSafetyName(entry.toggle_safety),
                    (entry.owner != nullptr) ? entry.owner : "(unknown)",
                    (entry.feature != nullptr) ? entry.feature : "(unknown)"
                )
            }
        }

        static void PrintList(const char *label, const char **arr, size_t count) {
            PRINT("%s (%u):", label, static_cast<u32>(count))
            for (size_t i = 0; i < count; ++i) {
                const char *name = arr[i];
                if (name != nullptr && name[0] != '\0') {
                    PRINT("  - %s", name)
                }
            }
        }
    }  // namespace

    void RecordHook(const char *name) {
        Record(name, g_hooks, g_hook_count);
    }

    void RecordPatch(const char *name) {
        Record(name, g_patches, g_patch_count);
    }

    void PrintOnce() {
        if (g_printed) {
            return;
        }
        g_printed = true;

        PRINT_LINE("\n--- boot report ---");
        PrintConfigSummary();
        PrintHookPlan();
        PrintList("hooks installed", g_hooks, g_hook_count);
        PrintList("patches applied", g_patches, g_patch_count);
    }

}  // namespace d3::boot_report
