#include "program/config_schema.hpp"
#include "program/d3/resolution_util.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <charconv>
#include <limits>
#include <optional>
#include <string>

namespace d3::config_schema {
    namespace {
        static auto NormalizeKey(std::string_view input) -> std::string {
            std::string out;
            out.reserve(input.size());
            for (char const ch : input) {
                auto const uc = static_cast<unsigned char>(ch);
                if (std::isspace(uc) || ch == '_' || ch == '-') {
                    continue;
                }
                out.push_back(static_cast<char>(std::tolower(uc)));
            }
            return out;
        }

        static auto ParseResolutionHackOutputTarget(std::string_view input, u32 fallback) -> u32 {
            const auto normalized = NormalizeKey(input);
            if (normalized.empty()) {
                return fallback;
            }
            if (normalized == "default" || normalized == "unchanged") {
                return 900u;
            }
            if (normalized == "720" || normalized == "720p") {
                return 720u;
            }
            if (normalized == "900" || normalized == "900p") {
                return 900u;
            }
            if (normalized == "1080" || normalized == "1080p" || normalized == "fhd") {
                return 1080u;
            }
            if (normalized == "1440" || normalized == "1440p" || normalized == "2k" || normalized == "qhd") {
                return 1440u;
            }
            if (normalized == "2160" || normalized == "2160p" || normalized == "4k" || normalized == "uhd") {
                return 2160u;
            }

            u32         value = 0;
            const auto *begin = normalized.data();
            const auto *end   = normalized.data() + normalized.size();
            auto [ptr, ec]    = std::from_chars(begin, end, value);
            if (ec == std::errc() && ptr == end && value > 0u) {
                return value;
            }
            return fallback;
        }

        static auto FindSection(const toml::table &root, std::string_view key) -> const toml::table * {
            if (auto node = root.get(key); node && node->is_table()) {
                return node->as_table();
            }
            return nullptr;
        }

        template<typename T>
        static auto ReadValue(const toml::table &table, std::span<const std::string_view> keys) -> std::optional<T> {
            for (auto key : keys) {
                if (auto node = table.get(key)) {
                    if (auto value = node->value<T>()) {
                        return value;
                    }
                }
            }
            return std::nullopt;
        }

        static auto ReadNumber(const toml::table &table, std::span<const std::string_view> keys) -> std::optional<double> {
            for (auto key : keys) {
                if (auto node = table.get(key)) {
                    if (auto value = node->value<double>()) {
                        return value;
                    }
                    if (auto value = node->value<s64>()) {
                        return static_cast<double>(*value);
                    }
                }
            }
            return std::nullopt;
        }

        // Key aliases for reads.
        static constexpr std::array<std::string_view, 3> kKeysSectionEnabled       = {"SectionEnabled", "Enabled", "Active"};
        static constexpr std::array<std::string_view, 2> kKeysBuildLockerWatermark = {"BuildLockerWatermark", "BuildlockerWatermark"};
        static constexpr std::array<std::string_view, 2> kKeysDdmLabels            = {"DDMLabels", "DebugLabels"};
        static constexpr std::array<std::string_view, 2> kKeysFpsLabel             = {"FPSLabel", "DrawFPSLabel"};
        static constexpr std::array<std::string_view, 3> kKeysVarResLabel          = {"VariableResLabel", "VarResLabel", "DrawVariableResolutionLabel"};
        static constexpr std::array<std::string_view, 3> kKeysTagNx                = {"SpoofNetworkFunctions", "SpoofNetwork", "TagNX"};
        static constexpr std::array<std::string_view, 3> kKeysLanguage             = {"Language", "Lang", "Locale"};
        static constexpr std::array<std::string_view, 1> kKeysAcknowledgeJester    = {"Acknowledge_god_jester"};
        static constexpr std::array<std::string_view, 2> kKeysPubfileDump          = {"EnablePubFileDump", "PubFileDump"};
        static constexpr std::array<std::string_view, 2> kKeysErrorTraces          = {"EnableErrorTraces", "ErrorTraces"};
        static constexpr std::array<std::string_view, 2> kKeysDebugFlags           = {"EnableDebugFlags", "DebugFlags"};
        static constexpr std::array<std::string_view, 2> kKeysExceptionHandler     = {"EnableExceptionHandler", "ExceptionHandler"};
        static constexpr std::array<std::string_view, 2> kKeysOeNotificationHook   = {"EnableOeNotificationHook", "OeNotificationHook"};
        static constexpr std::array<std::string_view, 2> kKeysLogOeNotifications   = {"LogOeNotificationMessages", "LogNotificationMessages"};

        static constexpr std::array<std::string_view, 3> kKeysGuiEnabled   = {"Enabled", "SectionEnabled", "Active"};
        static constexpr std::array<std::string_view, 3> kKeysGuiVisible   = {"Visible", "Show", "WindowVisible"};
        static constexpr std::array<std::string_view, 3> kKeysGuiLeftStick = {"AllowLeftStickPassthrough", "LeftStickPassthrough", "AllowLeftStickThrough"};

        static constexpr std::array<std::string_view, 3> kKeysReshackSpoofDocked  = {"SpoofDocked", "SpoofDock", "DockedSpoof"};
        static constexpr std::array<std::string_view, 4> kKeysReshackExpScheduler = {"ExperimentalScheduler", "ExpScheduler", "ExperimentalScheduling", "ExpScheduling"};
        static constexpr std::array<std::string_view, 9> kKeysReshackOutputTarget = {
            "OutputTarget",
            "OutputHeight",
            "Height",
            "OutputVertical",
            "Vertical",
            "OutputRes",
            "OutputResolution",
            "OutputWidth",
            "Width",
        };
        static constexpr std::array<std::string_view, 6> kKeysReshackMinResScale = {
            "MinScale",
            "MinimumScale",
            "MinResScale",
            "MinimumResScale",
            "MinResolutionScale",
            "MinimumResolutionScale",
        };
        static constexpr std::array<std::string_view, 6> kKeysReshackMaxResScale = {
            "MaxScale",
            "MaximumScale",
            "MaxResScale",
            "MaximumResScale",
            "MaxResolutionScale",
            "MaximumResolutionScale",
        };
        static constexpr std::array<std::string_view, 3> kKeysReshackOutputHandheldScale = {
            "OutputHandheldScale",
            "HandheldScale",
            "HandheldOutputScale",
        };
        static constexpr std::array<std::string_view, 4> kKeysReshackClampTextureResolution = {
            "ClampTextureResolution",
            "ClampTextureHeight",
            "ClampTexture",
            "ClampTextureRes",
        };

        // Accessors. Keep these tiny and explicit (no templates) so it's easy to read in crash logs.
        static auto GetOverlaysActive(const PatchConfig &cfg) -> bool { return cfg.overlays.active; }
        static void SetOverlaysActive(PatchConfig &cfg, bool v) { cfg.overlays.active = v; }
        static auto GetOverlaysBuildLocker(const PatchConfig &cfg) -> bool { return cfg.overlays.buildlocker_watermark; }
        static void SetOverlaysBuildLocker(PatchConfig &cfg, bool v) { cfg.overlays.buildlocker_watermark = v; }
        static auto GetOverlaysDdm(const PatchConfig &cfg) -> bool { return cfg.overlays.ddm_labels; }
        static void SetOverlaysDdm(PatchConfig &cfg, bool v) { cfg.overlays.ddm_labels = v; }
        static auto GetOverlaysFps(const PatchConfig &cfg) -> bool { return cfg.overlays.fps_label; }
        static void SetOverlaysFps(PatchConfig &cfg, bool v) { cfg.overlays.fps_label = v; }
        static auto GetOverlaysVarRes(const PatchConfig &cfg) -> bool { return cfg.overlays.var_res_label; }
        static void SetOverlaysVarRes(PatchConfig &cfg, bool v) { cfg.overlays.var_res_label = v; }

        static auto GetGuiEnabled(const PatchConfig &cfg) -> bool { return cfg.gui.enabled; }
        static void SetGuiEnabled(PatchConfig &cfg, bool v) { cfg.gui.enabled = v; }
        static auto GetGuiVisible(const PatchConfig &cfg) -> bool { return cfg.gui.visible; }
        static void SetGuiVisible(PatchConfig &cfg, bool v) { cfg.gui.visible = v; }
        static auto GetGuiLeftStickPass(const PatchConfig &cfg) -> bool { return cfg.gui.allow_left_stick_passthrough; }
        static void SetGuiLeftStickPass(PatchConfig &cfg, bool v) { cfg.gui.allow_left_stick_passthrough = v; }
        static auto GetGuiLanguage(const PatchConfig &cfg) -> const std::string * { return &cfg.gui.language_override; }
        static void SetGuiLanguage(PatchConfig &cfg, std::string_view v) { cfg.gui.language_override = std::string(v); }

        static auto GetDebugActive(const PatchConfig &cfg) -> bool { return cfg.debug.active; }
        static void SetDebugActive(PatchConfig &cfg, bool v) { cfg.debug.active = v; }
        static auto GetDebugEnableCrashes(const PatchConfig &cfg) -> bool { return cfg.debug.enable_crashes; }
        static void SetDebugEnableCrashes(PatchConfig &cfg, bool v) { cfg.debug.enable_crashes = v; }
        static auto GetDebugPubfileDump(const PatchConfig &cfg) -> bool { return cfg.debug.enable_pubfile_dump; }
        static void SetDebugPubfileDump(PatchConfig &cfg, bool v) { cfg.debug.enable_pubfile_dump = v; }
        static auto GetDebugErrorTraces(const PatchConfig &cfg) -> bool { return cfg.debug.enable_error_traces; }
        static void SetDebugErrorTraces(PatchConfig &cfg, bool v) { cfg.debug.enable_error_traces = v; }
        static auto GetDebugDebugFlags(const PatchConfig &cfg) -> bool { return cfg.debug.enable_debug_flags; }
        static void SetDebugDebugFlags(PatchConfig &cfg, bool v) { cfg.debug.enable_debug_flags = v; }
        static auto GetDebugTagNx(const PatchConfig &cfg) -> bool { return cfg.debug.tagnx; }
        static void SetDebugTagNx(PatchConfig &cfg, bool v) { cfg.debug.tagnx = v; }
        static auto GetDebugExceptionHandler(const PatchConfig &cfg) -> bool { return cfg.debug.enable_exception_handler; }
        static void SetDebugExceptionHandler(PatchConfig &cfg, bool v) { cfg.debug.enable_exception_handler = v; }
        static auto GetDebugOeNotificationHook(const PatchConfig &cfg) -> bool { return cfg.debug.enable_oe_notification_hook; }
        static void SetDebugOeNotificationHook(PatchConfig &cfg, bool v) { cfg.debug.enable_oe_notification_hook = v; }
        static auto GetDebugLogOeNotifications(const PatchConfig &cfg) -> bool { return cfg.debug.log_oe_notification_messages; }
        static void SetDebugLogOeNotifications(PatchConfig &cfg, bool v) { cfg.debug.log_oe_notification_messages = v; }

        static auto GetReshackActive(const PatchConfig &cfg) -> bool { return cfg.resolution_hack.active; }
        static void SetReshackActive(PatchConfig &cfg, bool v) { cfg.resolution_hack.active = v; }
        static auto GetReshackSpoofDocked(const PatchConfig &cfg) -> bool { return cfg.resolution_hack.spoof_docked; }
        static void SetReshackSpoofDocked(PatchConfig &cfg, bool v) { cfg.resolution_hack.spoof_docked = v; }
        static auto GetReshackExpScheduler(const PatchConfig &cfg) -> bool { return cfg.resolution_hack.exp_scheduler; }
        static void SetReshackExpScheduler(PatchConfig &cfg, bool v) { cfg.resolution_hack.exp_scheduler = v; }
        static auto GetReshackOutputTarget(const PatchConfig &cfg) -> u32 { return cfg.resolution_hack.target_resolution; }
        static void SetReshackOutputTarget(PatchConfig &cfg, u32 v) {
            const u32 max_target = d3::MaxResolutionHackOutputTarget();
            cfg.resolution_hack.SetTargetRes(std::min(v, max_target));
        }
        static auto GetReshackOutputHandheldScale(const PatchConfig &cfg) -> float { return cfg.resolution_hack.output_handheld_scale; }
        static void SetReshackOutputHandheldScale(PatchConfig &cfg, float v) {
            if (v > 0.0f && v <= 1.0f) {
                v *= 100.0f;
            }
            cfg.resolution_hack.output_handheld_scale = PatchConfig::ResolutionHackConfig::NormalizeHandheldScale(v);
        }
        static auto GetReshackMinResScale(const PatchConfig &cfg) -> float { return cfg.resolution_hack.min_res_scale; }
        static void SetReshackMinResScale(PatchConfig &cfg, float v) {
            cfg.resolution_hack.min_res_scale = std::clamp(v, 10.0f, 100.0f);
            if (cfg.resolution_hack.max_res_scale < cfg.resolution_hack.min_res_scale) {
                cfg.resolution_hack.max_res_scale = cfg.resolution_hack.min_res_scale;
            }
        }
        static auto GetReshackMaxResScale(const PatchConfig &cfg) -> float { return cfg.resolution_hack.max_res_scale; }
        static void SetReshackMaxResScale(PatchConfig &cfg, float v) {
            cfg.resolution_hack.max_res_scale = std::clamp(v, 10.0f, 100.0f);
            if (cfg.resolution_hack.max_res_scale < cfg.resolution_hack.min_res_scale) {
                cfg.resolution_hack.min_res_scale = cfg.resolution_hack.max_res_scale;
            }
        }
        static auto GetReshackClampTextureResolution(const PatchConfig &cfg) -> u32 { return cfg.resolution_hack.clamp_texture_resolution; }
        static void SetReshackClampTextureResolution(PatchConfig &cfg, u32 v) {
            if (v == 0u) {
                cfg.resolution_hack.clamp_texture_resolution = 0u;
                return;
            }
            cfg.resolution_hack.clamp_texture_resolution = std::clamp(
                v,
                PatchConfig::ResolutionHackConfig::kClampTextureResolutionMin,
                PatchConfig::ResolutionHackConfig::kClampTextureResolutionMax
            );
        }

        static constexpr std::array<Entry, 26> k_entries = {{
            // overlays
            {.section = "overlays", .key = "SectionEnabled", .keys = kKeysSectionEnabled, .kind = ValueKind::Bool, .restart = RestartPolicy::RuntimeSafe, .tr_label = "gui.overlays_enabled", .label_fallback = "Enabled##overlays", .get_bool = &GetOverlaysActive, .set_bool = &SetOverlaysActive},
            {.section = "overlays", .key = "BuildLockerWatermark", .keys = kKeysBuildLockerWatermark, .kind = ValueKind::Bool, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.overlays_buildlocker", .label_fallback = "Build locker watermark", .get_bool = &GetOverlaysBuildLocker, .set_bool = &SetOverlaysBuildLocker},
            {.section = "overlays", .key = "DDMLabels", .keys = kKeysDdmLabels, .kind = ValueKind::Bool, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.overlays_ddm", .label_fallback = "DDM labels", .get_bool = &GetOverlaysDdm, .set_bool = &SetOverlaysDdm},
            {.section = "overlays", .key = "FPSLabel", .keys = kKeysFpsLabel, .kind = ValueKind::Bool, .restart = RestartPolicy::RuntimeSafe, .tr_label = "gui.overlays_fps", .label_fallback = "FPS label", .get_bool = &GetOverlaysFps, .set_bool = &SetOverlaysFps},
            {.section = "overlays", .key = "VariableResLabel", .keys = kKeysVarResLabel, .kind = ValueKind::Bool, .restart = RestartPolicy::RuntimeSafe, .tr_label = "gui.overlays_var_res", .label_fallback = "Variable resolution label", .get_bool = &GetOverlaysVarRes, .set_bool = &SetOverlaysVarRes},

            // gui
            {.section = "gui", .key = "Enabled", .keys = kKeysGuiEnabled, .kind = ValueKind::Bool, .restart = RestartPolicy::RuntimeSafe, .tr_label = "gui.enabled_persist", .label_fallback = "Enabled (persist)", .get_bool = &GetGuiEnabled, .set_bool = &SetGuiEnabled},
            {.section = "gui", .key = "Visible", .keys = kKeysGuiVisible, .kind = ValueKind::Bool, .restart = RestartPolicy::RuntimeSafe, .tr_label = "gui.visible_persist", .label_fallback = "Visible (persist)", .get_bool = &GetGuiVisible, .set_bool = &SetGuiVisible},
            {.section = "gui", .key = "AllowLeftStickPassthrough", .keys = kKeysGuiLeftStick, .kind = ValueKind::Bool, .restart = RestartPolicy::RuntimeSafe, .tr_label = "gui.left_stick_passthrough", .label_fallback = "Allow left stick passthrough", .get_bool = &GetGuiLeftStickPass, .set_bool = &SetGuiLeftStickPass},
            {.section = "gui", .key = "Language", .keys = kKeysLanguage, .kind = ValueKind::String, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.language", .label_fallback = "Language", .omit_if_empty = true, .get_string = &GetGuiLanguage, .set_string = &SetGuiLanguage},

            // debug
            {.section = "debug", .key = "SectionEnabled", .keys = kKeysSectionEnabled, .kind = ValueKind::Bool, .restart = RestartPolicy::RuntimeSafe, .tr_label = "gui.debug_enabled", .label_fallback = "Enabled##debug", .get_bool = &GetDebugActive, .set_bool = &SetDebugActive},
            {.section = "debug", .key = "Acknowledge_god_jester", .keys = kKeysAcknowledgeJester, .kind = ValueKind::Bool, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.debug_enable_crashes", .label_fallback = "Enable crashes (danger)", .get_bool = &GetDebugEnableCrashes, .set_bool = &SetDebugEnableCrashes},
            {.section = "debug", .key = "EnablePubFileDump", .keys = kKeysPubfileDump, .kind = ValueKind::Bool, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.debug_pubfile_dump", .label_fallback = "Enable pubfile dump", .get_bool = &GetDebugPubfileDump, .set_bool = &SetDebugPubfileDump},
            {.section = "debug", .key = "EnableErrorTraces", .keys = kKeysErrorTraces, .kind = ValueKind::Bool, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.debug_error_traces", .label_fallback = "Enable error traces", .get_bool = &GetDebugErrorTraces, .set_bool = &SetDebugErrorTraces},
            {.section = "debug", .key = "EnableDebugFlags", .keys = kKeysDebugFlags, .kind = ValueKind::Bool, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.debug_debug_flags", .label_fallback = "Enable debug flags", .get_bool = &GetDebugDebugFlags, .set_bool = &SetDebugDebugFlags},
            {.section = "debug", .key = "SpoofNetworkFunctions", .keys = kKeysTagNx, .kind = ValueKind::Bool, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.debug_spoof_network", .label_fallback = "Spoof Network Functions", .get_bool = &GetDebugTagNx, .set_bool = &SetDebugTagNx},
            {.section = "debug", .key = "EnableExceptionHandler", .keys = kKeysExceptionHandler, .kind = ValueKind::Bool, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.debug_exception_handler", .label_fallback = "Enable exception handler", .get_bool = &GetDebugExceptionHandler, .set_bool = &SetDebugExceptionHandler},
            {.section = "debug", .key = "EnableOeNotificationHook", .keys = kKeysOeNotificationHook, .kind = ValueKind::Bool, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.debug_oe_notifications", .label_fallback = "Hook OE notifications", .get_bool = &GetDebugOeNotificationHook, .set_bool = &SetDebugOeNotificationHook},
            {.section = "debug", .key = "LogOeNotificationMessages", .keys = kKeysLogOeNotifications, .kind = ValueKind::Bool, .restart = RestartPolicy::RuntimeSafe, .tr_label = "gui.debug_log_oe_notifications", .label_fallback = "Log OE notification messages", .get_bool = &GetDebugLogOeNotifications, .set_bool = &SetDebugLogOeNotifications},

            // resolution_hack (subset)
            {.section = "resolution_hack", .key = "SectionEnabled", .keys = kKeysSectionEnabled, .kind = ValueKind::Bool, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.resolution_enabled", .label_fallback = "Enabled##res", .get_bool = &GetReshackActive, .set_bool = &SetReshackActive},
            {.section = "resolution_hack", .key = "OutputTarget", .keys = kKeysReshackOutputTarget, .kind = ValueKind::U32, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.resolution_output_target", .label_fallback = "Output target (vertical)", .min_u = 720, .max_u = 2160, .step_u = 1, .get_u32 = &GetReshackOutputTarget, .set_u32 = &SetReshackOutputTarget, .parse_u32_str = &ParseResolutionHackOutputTarget},
            {.section = "resolution_hack", .key = "OutputHandheldScale", .keys = kKeysReshackOutputHandheldScale, .kind = ValueKind::Float, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.resolution_output_handheld_scale", .label_fallback = "Handheld output scale (%)", .min_f = 0.0f, .max_f = PatchConfig::ResolutionHackConfig::kHandheldScaleMax, .step_f = PatchConfig::ResolutionHackConfig::kHandheldScaleStep, .get_float = &GetReshackOutputHandheldScale, .set_float = &SetReshackOutputHandheldScale},
            {.section = "resolution_hack", .key = "MinResScale", .keys = kKeysReshackMinResScale, .kind = ValueKind::Float, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.resolution_min_scale", .label_fallback = "Minimum resolution scale", .min_f = 10.0f, .max_f = 100.0f, .step_f = 1.0f, .get_float = &GetReshackMinResScale, .set_float = &SetReshackMinResScale},
            {.section = "resolution_hack", .key = "MaxResScale", .keys = kKeysReshackMaxResScale, .kind = ValueKind::Float, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.resolution_max_scale", .label_fallback = "Maximum resolution scale", .min_f = 10.0f, .max_f = 100.0f, .step_f = 1.0f, .get_float = &GetReshackMaxResScale, .set_float = &SetReshackMaxResScale},
            {.section = "resolution_hack", .key = "ClampTextureResolution", .keys = kKeysReshackClampTextureResolution, .kind = ValueKind::U32, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.resolution_clamp_texture", .label_fallback = "Clamp texture height (0=off, 100-9999)", .min_u = 0, .max_u = PatchConfig::ResolutionHackConfig::kClampTextureResolutionMax, .step_u = 1, .get_u32 = &GetReshackClampTextureResolution, .set_u32 = &SetReshackClampTextureResolution},
            {.section = "resolution_hack", .key = "SpoofDocked", .keys = kKeysReshackSpoofDocked, .kind = ValueKind::Bool, .restart = RestartPolicy::RestartRequired, .tr_label = "gui.resolution_spoof_docked", .label_fallback = "Spoof docked", .get_bool = &GetReshackSpoofDocked, .set_bool = &SetReshackSpoofDocked},
            {.section = "resolution_hack", .key = "ExperimentalScheduler", .keys = kKeysReshackExpScheduler, .kind = ValueKind::Bool, .restart = RestartPolicy::RuntimeSafe, .tr_label = "gui.resolution_exp_scheduler", .label_fallback = "Experimental scheduler", .get_bool = &GetReshackExpScheduler, .set_bool = &SetReshackExpScheduler},
        }};

        static_assert(!k_entries.empty());

        static auto EntriesImpl() -> std::span<const Entry> {
            return std::span<const Entry>(k_entries);
        }

        static auto EnsureSectionTable(toml::table &root, std::string_view section) -> toml::table & {
            if (auto node = root.get(section); node && node->is_table()) {
                return *node->as_table();
            }
            toml::table t;
            root.insert(section, std::move(t));
            return *root.get(section)->as_table();
        }

    }  // namespace

    auto Entries() -> std::span<const Entry> {
        return EntriesImpl();
    }

    auto EntriesForSection(std::string_view section) -> std::span<const Entry> {
        const auto all   = EntriesImpl();
        size_t     start = 0;
        while (start < all.size() && all[start].section != section) {
            ++start;
        }
        size_t end = start;
        while (end < all.size() && all[end].section == section) {
            ++end;
        }
        return all.subspan(start, end - start);
    }

    auto FindEntry(std::string_view section, std::string_view key) -> const Entry * {
        for (const auto &e : EntriesImpl()) {
            if (e.section == section && e.key == key) {
                return &e;
            }
        }
        return nullptr;
    }

    void ApplyTomlTable(PatchConfig &config, const toml::table &root) {
        for (const auto &e : EntriesImpl()) {
            const auto *section = FindSection(root, e.section);
            if (section == nullptr) {
                continue;
            }
            switch (e.kind) {
            case ValueKind::Bool: {
                if (e.set_bool == nullptr) {
                    break;
                }
                if (auto v = ReadValue<bool>(*section, e.keys)) {
                    e.set_bool(config, *v);
                }
                break;
            }
            case ValueKind::U32: {
                if (e.set_u32 == nullptr) {
                    break;
                }
                std::optional<double> v = ReadNumber(*section, e.keys);
                if (!v && e.parse_u32_str != nullptr) {
                    if (auto s = ReadValue<std::string>(*section, e.keys)) {
                        const u32 parsed = e.parse_u32_str(*s, e.get_u32 != nullptr ? e.get_u32(config) : 0u);
                        e.set_u32(config, parsed);
                        break;
                    }
                }
                if (v) {
                    const double clamped = (e.max_u > e.min_u)
                                               ? std::clamp(*v, static_cast<double>(e.min_u), static_cast<double>(e.max_u))
                                               : *v;
                    if (clamped >= 0.0 && std::isfinite(clamped) &&
                        clamped <= static_cast<double>(std::numeric_limits<u32>::max())) {
                        e.set_u32(config, static_cast<u32>(clamped));
                    }
                }
                break;
            }
            case ValueKind::Float: {
                if (e.set_float == nullptr) {
                    break;
                }
                std::optional<double> v = ReadNumber(*section, e.keys);
                if (!v && e.parse_float_str != nullptr) {
                    if (auto s = ReadValue<std::string>(*section, e.keys)) {
                        const float parsed = e.parse_float_str(*s, e.get_float != nullptr ? e.get_float(config) : 0.0f);
                        e.set_float(config, parsed);
                        break;
                    }
                }
                if (v) {
                    const double clamped = (e.max_f > e.min_f)
                                               ? std::clamp(*v, static_cast<double>(e.min_f), static_cast<double>(e.max_f))
                                               : *v;
                    if (std::isfinite(clamped)) {
                        e.set_float(config, static_cast<float>(clamped));
                    }
                }
                break;
            }
            case ValueKind::String: {
                if (e.set_string == nullptr) {
                    break;
                }
                if (auto v = ReadValue<std::string>(*section, e.keys)) {
                    e.set_string(config, *v);
                }
                break;
            }
            }
        }
    }

    void InsertIntoToml(toml::table &root, const PatchConfig &config) {
        for (const auto &e : EntriesImpl()) {
            toml::table &section = EnsureSectionTable(root, e.section);
            switch (e.kind) {
            case ValueKind::Bool: {
                if (e.get_bool != nullptr) {
                    section.insert(e.key, e.get_bool(config));
                }
                break;
            }
            case ValueKind::U32: {
                if (e.get_u32 != nullptr) {
                    section.insert(e.key, static_cast<s64>(e.get_u32(config)));
                }
                break;
            }
            case ValueKind::Float: {
                if (e.get_float != nullptr) {
                    section.insert(e.key, static_cast<double>(e.get_float(config)));
                }
                break;
            }
            case ValueKind::String: {
                if (e.get_string != nullptr) {
                    const std::string *s = e.get_string(config);
                    if (s != nullptr) {
                        if (e.omit_if_empty && s->empty()) {
                            break;
                        }
                        section.insert(e.key, *s);
                    }
                }
                break;
            }
            }
        }
    }

}  // namespace d3::config_schema
