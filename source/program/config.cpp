#include "program/config.hpp"
#include "d3/setting.hpp"
#include "d3/resolution_util.hpp"
#include "lib/diag/assert.hpp"
#include "nn/fs.hpp"  // IWYU pragma: keep
#include "program/system_allocator.hpp"
#include "program/fs_util.hpp"
#include "program/config_schema.hpp"

#include <array>
#include <ostream>
#include <span>
#include <string_view>
#include <utility>

PatchConfig global_config {};

namespace d3 {
    float g_rt_scale             = PatchConfig::ResolutionHackConfig::kHandheldScaleDefault * 0.01f;
    bool  g_rt_scale_initialized = false;
}  // namespace d3

namespace {
    auto FindTable(const toml::table &root, std::string_view key) -> const toml::table * {
        if (auto node = root.get(key); node && node->is_table())
            return node->as_table();
        return nullptr;
    }

    template<typename T>
    auto ReadValue(const toml::table &table, std::span<const std::string_view> keys) -> std::optional<T> {
        for (auto key : keys) {
            if (auto node = table.get(key)) {
                if (auto value = node->value<T>())
                    return value;
            }
        }
        return std::nullopt;
    }

    template<typename T>
    auto ReadValue(const toml::table &table, std::initializer_list<std::string_view> keys) -> std::optional<T> {
        return ReadValue<T>(table, std::span<const std::string_view>(keys.begin(), keys.size()));
    }

    auto ReadBool(const toml::table &table, std::span<const std::string_view> keys, bool fallback) -> bool {
        if (auto value = ReadValue<bool>(table, keys))
            return *value;
        return fallback;
    }

    auto ReadBool(const toml::table &table, std::initializer_list<std::string_view> keys, bool fallback) -> bool {
        return ReadBool(table, std::span<const std::string_view>(keys.begin(), keys.size()), fallback);
    }

    auto ReadNumber(const toml::table &table, std::span<const std::string_view> keys) -> std::optional<double> {
        for (auto key : keys) {
            if (auto node = table.get(key)) {
                if (auto value = node->value<double>())
                    return value;
                if (auto value = node->value<s64>())
                    return static_cast<double>(*value);
            }
        }
        return std::nullopt;
    }

    auto ReadU32(const toml::table &table, std::span<const std::string_view> keys, u32 fallback, u32 min_value, u32 max_value) -> u32 {
        if (auto value = ReadNumber(table, keys)) {
            auto clamped = std::clamp(*value, static_cast<double>(min_value), static_cast<double>(max_value));
            return static_cast<u32>(clamped);
        }
        return fallback;
    }

    auto ReadU32(const toml::table &table, std::initializer_list<std::string_view> keys, u32 fallback, u32 min_value, u32 max_value) -> u32 {
        return ReadU32(table, std::span<const std::string_view>(keys.begin(), keys.size()), fallback, min_value, max_value);
    }

    auto ReadI32(const toml::table &table, std::span<const std::string_view> keys, s32 fallback, s32 min_value, s32 max_value) -> s32 {
        if (auto value = ReadNumber(table, keys)) {
            auto clamped = std::clamp(*value, static_cast<double>(min_value), static_cast<double>(max_value));
            return static_cast<s32>(clamped);
        }
        return fallback;
    }

    auto ReadI32(const toml::table &table, std::initializer_list<std::string_view> keys, s32 fallback, s32 min_value, s32 max_value) -> s32 {
        return ReadI32(table, std::span<const std::string_view>(keys.begin(), keys.size()), fallback, min_value, max_value);
    }

    auto ReadDouble(const toml::table &table, std::span<const std::string_view> keys, double fallback, double min_value, double max_value) -> double {
        if (auto value = ReadNumber(table, keys)) {
            return std::clamp(*value, min_value, max_value);
        }
        return fallback;
    }

    auto ReadDouble(const toml::table &table, std::initializer_list<std::string_view> keys, double fallback, double min_value, double max_value) -> double {
        return ReadDouble(table, std::span<const std::string_view>(keys.begin(), keys.size()), fallback, min_value, max_value);
    }

    auto Round1Decimal(double value) -> double {
        return std::round(value * 10.0) / 10.0;
    }

    auto ReadString(const toml::table &table, std::initializer_list<std::string_view> keys, std::string fallback) -> std::string {
        if (auto value = ReadValue<std::string>(table, keys))
            return *value;
        return fallback;
    }

    auto NormalizeKey(std::string_view input) -> std::string {
        std::string out;
        out.reserve(input.size());
        for (char const ch : input) {
            auto const uc = static_cast<unsigned char>(ch);
            if (std::isspace(uc) || ch == '_' || ch == '-')
                continue;
            out.push_back(static_cast<char>(std::tolower(uc)));
        }
        return out;
    }

    auto ParseSeasonEventMapMode(std::string_view input, PatchConfig::SeasonEventMapMode fallback) -> PatchConfig::SeasonEventMapMode {
        auto normalized = NormalizeKey(input);
        if (normalized.empty())
            return fallback;
        if (normalized == "maponly" || normalized == "map" || normalized == "seasonmap" || normalized == "simulate")
            return PatchConfig::SeasonEventMapMode::MapOnly;
        if (normalized == "overlay" || normalized == "overlayconfig" || normalized == "add" || normalized == "additive" || normalized == "merge")
            return PatchConfig::SeasonEventMapMode::OverlayConfig;
        if (normalized == "disabled" || normalized == "off" || normalized == "none" || normalized == "false")
            return PatchConfig::SeasonEventMapMode::Disabled;
        return fallback;
    }

    auto ReadSeasonEventMapMode(const toml::table &table, std::initializer_list<std::string_view> keys, PatchConfig::SeasonEventMapMode fallback) -> PatchConfig::SeasonEventMapMode {
        if (auto value = ReadValue<std::string>(table, keys))
            return ParseSeasonEventMapMode(*value, fallback);
        if (auto value = ReadValue<bool>(table, keys))
            return *value ? PatchConfig::SeasonEventMapMode::MapOnly : PatchConfig::SeasonEventMapMode::Disabled;
        return fallback;
    }

    auto ReadSeasonEventFlagBool(const toml::table &table, const char *key, const char *legacy_key, bool fallback) -> bool {
        if (legacy_key != nullptr && legacy_key[0] != '\0') {
            return ReadBool(table, {key, legacy_key}, fallback);
        }
        return ReadBool(table, {key}, fallback);
    }

    struct SeasonEventFlags {
#define D3HACK_SEASON_EVENT_FIELD(name, default_config, default_map, legacy_key) bool name = default_map;
        D3HACK_SEASON_EVENT_FLAGS(D3HACK_SEASON_EVENT_FIELD)
#undef D3HACK_SEASON_EVENT_FIELD
    };

    auto CaptureSeasonEventFlags(const PatchConfig &config) -> SeasonEventFlags {
        SeasonEventFlags out {};
#define D3HACK_SEASON_EVENT_CAPTURE(name, default_config, default_map, legacy_key) out.name = config.events.name;
        D3HACK_SEASON_EVENT_FLAGS(D3HACK_SEASON_EVENT_CAPTURE)
#undef D3HACK_SEASON_EVENT_CAPTURE
        return out;
    }

    void ApplySeasonEventFlags(PatchConfig &config, const SeasonEventFlags &flags) {
#define D3HACK_SEASON_EVENT_APPLY(name, default_config, default_map, legacy_key) config.events.name = flags.name;
        D3HACK_SEASON_EVENT_FLAGS(D3HACK_SEASON_EVENT_APPLY)
#undef D3HACK_SEASON_EVENT_APPLY
    }

    auto OverlaySeasonEventFlags(const SeasonEventFlags &map, const SeasonEventFlags &extra) -> SeasonEventFlags {
        SeasonEventFlags out = map;
#define D3HACK_SEASON_EVENT_OVERLAY(name, default_config, default_map, legacy_key) out.name |= extra.name;
        D3HACK_SEASON_EVENT_FLAGS(D3HACK_SEASON_EVENT_OVERLAY)
#undef D3HACK_SEASON_EVENT_OVERLAY
        return out;
    }

    auto BuildSeasonEventMap(u32 season, SeasonEventFlags &out) -> bool {
        out = SeasonEventFlags {};
        switch (season) {
        case 14:
            out.DoubleTreasureGoblins = true;
            return true;
        case 15:
            out.DoubleBountyBags = true;
            return true;
        case 16:
            out.RoyalGrandeur = true;
            return true;
        case 17:
            out.LegacyOfNightmares = true;
            return true;
        case 18:
            out.TriunesWill = true;
            return true;
        case 19:
            out.Pandemonium = true;
            return true;
        case 20:
            out.KanaiPowers = true;
            return true;
        case 21:
            out.TrialsOfTempests = true;
            return true;
        case 22:
            out.ShadowClones         = true;
            out.FourthKanaisCubeSlot = true;
            return true;
        case 24:
            out.EtherealItems = true;
            return true;
        case 25:
            out.SoulShards = true;
            return true;
        case 26:
            out.SwarmRifts = true;
            return true;
        case 27:
            out.SanctifiedItems = true;
            return true;
        case 28:
            out.DarkAlchemy = true;
            return true;
        case 29:
            out.NestingPortals = true;
            return true;
        case 30:
            out.SoulShards = true;
            return true;
        case 31:
            out.KanaiPowers = true;
            return true;
        case 32:
            out.EtherealItems = true;
            return true;
        case 33:
            out.ShadowClones         = true;
            out.FourthKanaisCubeSlot = true;
            return true;
        case 34:
            out.SanctifiedItems = true;
            return true;
        case 35:
            out.Pandemonium = true;
            return true;
        case 36:
            out.SoulShards = true;
            return true;
        case 37:
            out.KanaiPowers = true;
            return true;
        default:
            return false;
        }
    }

    void ApplySeasonEventMapIfNeeded(PatchConfig &config) {
        if (config.events.SeasonMapMode == PatchConfig::SeasonEventMapMode::Disabled)
            return;
        if (!config.seasons.active)
            return;
        SeasonEventFlags map {};
        if (!BuildSeasonEventMap(config.seasons.current_season, map))
            return;
        auto current = CaptureSeasonEventFlags(config);
        auto merged  = config.events.SeasonMapMode == PatchConfig::SeasonEventMapMode::OverlayConfig
                           ? OverlaySeasonEventFlags(map, current)
                           : map;
        ApplySeasonEventFlags(config, merged);
    }

    auto AncientRankToValue(std::string_view input, int fallback) -> int {
        auto normalized = NormalizeKey(input);
        if (normalized == "0" || normalized == "normal" || normalized == "none")
            return 0;
        if (normalized == "1" || normalized == "ancient")
            return 1;
        if (normalized == "2" || normalized == "primal" || normalized == "primalancient")
            return 2;
        return fallback;
    }

    auto AncientRankCanonical(int value, std::string_view fallback) -> std::string {
        switch (value) {
        case 0:
            return "Normal";
        case 1:
            return "Ancient";
        case 2:
            return "Primal";
        default:
            return std::string(fallback);
        }
    }

    auto ReadAll(const char *path, d3::system_allocator::Buffer &buffer, std::string_view &out, std::string &error_out)
        -> bool {
        if (!d3::fs_util::DoesFileExist(path))
            return false;
        auto *allocator = d3::system_allocator::GetSystemAllocator();
        if (allocator == nullptr) {
            error_out = "System allocator unavailable";
            return false;
        }
        nn::fs::FileHandle fh {};
        auto               rc = nn::fs::OpenFile(&fh, path, nn::fs::OpenMode_Read);
        EXL_ASSERT(R_SUCCEEDED(rc), "Failed to open file: %s", path);

        s64 size = 0;
        rc       = nn::fs::GetFileSize(&size, fh);
        if (R_FAILED(rc) || size <= 0) {
            nn::fs::CloseFile(fh);
            return false;
        }

        if (!buffer.Resize(static_cast<size_t>(size))) {
            nn::fs::CloseFile(fh);
            error_out = "Failed to allocate config buffer";
            return false;
        }
        rc = nn::fs::ReadFile(fh, 0, buffer.data(), static_cast<u64>(size));
        EXL_ASSERT(R_SUCCEEDED(rc), "Failed to read file: %s", path);
        nn::fs::CloseFile(fh);
        out = buffer.view();
        return true;
    }

    auto LoadFromPath(const char *path, PatchConfig &out, std::string &error_out) -> bool {
        std::string_view             text;
        d3::system_allocator::Buffer buffer(d3::system_allocator::GetSystemAllocator());
        if (!ReadAll(path, buffer, text, error_out))
            return false;
        auto result = toml::parse(text, std::string_view {path});
        if (!result) {
            const auto &err = result.error();
            auto        pos = err.source().begin;
            error_out       = std::string(err.description());
            if (pos) {
                error_out += " (line ";
                error_out += std::to_string(pos.line);
                error_out += ", col ";
                error_out += std::to_string(pos.column);
                error_out += ")";
            }
            return false;
        }
        out.ApplyTable(result.table());
        return true;
    }

    auto SeasonMapModeToString(PatchConfig::SeasonEventMapMode mode) -> const char * {
        switch (mode) {
        case PatchConfig::SeasonEventMapMode::MapOnly:
            return "MapOnly";
        case PatchConfig::SeasonEventMapMode::OverlayConfig:
            return "OverlayConfig";
        case PatchConfig::SeasonEventMapMode::Disabled:
            return "Disabled";
        }
        return "Disabled";
    }

    auto BuildPatchConfigTable(const PatchConfig &config) -> toml::table {
        toml::table root;

        {
            toml::table t;
            toml::table extra;
            extra.insert("WindowLeft", static_cast<s64>(config.resolution_hack.extra.window_left));
            extra.insert("WindowTop", static_cast<s64>(config.resolution_hack.extra.window_top));
            extra.insert("WindowWidth", static_cast<s64>(config.resolution_hack.extra.window_width));
            extra.insert("WindowHeight", static_cast<s64>(config.resolution_hack.extra.window_height));
            extra.insert("UIOptWidth", static_cast<s64>(config.resolution_hack.extra.ui_opt_width));
            extra.insert("UIOptHeight", static_cast<s64>(config.resolution_hack.extra.ui_opt_height));
            extra.insert("RenderWidth", static_cast<s64>(config.resolution_hack.extra.render_width));
            extra.insert("RenderHeight", static_cast<s64>(config.resolution_hack.extra.render_height));
            extra.insert("RefreshRate", static_cast<s64>(config.resolution_hack.extra.refresh_rate));
            extra.insert("BitDepth", static_cast<s64>(config.resolution_hack.extra.bit_depth));
            extra.insert("MSAALevel", static_cast<s64>(config.resolution_hack.extra.msaa_level));
            t.insert("extra", std::move(extra));
            root.insert("resolution_hack", std::move(t));
        }

        {
            toml::table t;
            t.insert("SectionEnabled", config.seasons.active);
            t.insert("AllowOnlinePlay", config.seasons.allow_online);
            t.insert("SeasonNumber", static_cast<s64>(config.seasons.current_season));
            t.insert("SpoofPtr", config.seasons.spoof_ptr);
            root.insert("seasons", std::move(t));
        }

        {
            toml::table t;
            t.insert("SectionEnabled", config.challenge_rifts.active);
            t.insert("MakeRiftsRandom", config.challenge_rifts.random);
            t.insert("RiftRangeStart", static_cast<s64>(config.challenge_rifts.range_start));
            t.insert("RiftRangeEnd", static_cast<s64>(config.challenge_rifts.range_end));
            root.insert("challenge_rifts", std::move(t));
        }

        {
            toml::table t;
            t.insert("SectionEnabled", config.events.active);
            t.insert("SeasonMapMode", std::string(SeasonMapModeToString(config.events.SeasonMapMode)));
#define D3HACK_SEASON_EVENT_TOML_WRITE(name, default_config, default_map, legacy_key) t.insert(#name, config.events.name);
            D3HACK_SEASON_EVENT_FLAGS(D3HACK_SEASON_EVENT_TOML_WRITE)
#undef D3HACK_SEASON_EVENT_TOML_WRITE
            root.insert("events", std::move(t));
        }

        {
            toml::table t;
            t.insert("SectionEnabled", config.rare_cheats.active);
            t.insert("MovementSpeedMultiplier", Round1Decimal(config.rare_cheats.move_speed));
            t.insert("AttackSpeedMultiplier", Round1Decimal(config.rare_cheats.attack_speed));
            t.insert("FloatingDamageColor", config.rare_cheats.floating_damage_color);
            t.insert("GuaranteedLegendaryChance", config.rare_cheats.guaranteed_legendaries);
            t.insert("DropAnyItems", config.rare_cheats.drop_anything);
            t.insert("InstantPortalAndBookOfCain", config.rare_cheats.instant_portal);
            t.insert("NoCooldowns", config.rare_cheats.no_cooldowns);
            t.insert("InstantCubeAndCraftsAndEnchants", config.rare_cheats.instant_craft_actions);
            t.insert("SocketGemsToAnySlot", config.rare_cheats.any_gem_any_slot);
            t.insert("AutoPickup", config.rare_cheats.auto_pickup);
            t.insert("EquipAnySlot", config.rare_cheats.equip_any_slot);
            t.insert("UnlockAllDifficulties", config.rare_cheats.unlock_all_difficulties);
            t.insert("EasyKillDamage", config.rare_cheats.easy_kill_damage);
            t.insert("InfiniteMP", config.rare_cheats.infinite_mp);
            t.insert("CubeNoConsumeMaterials", config.rare_cheats.cube_no_consume);
            t.insert("LegendaryGemUpgradeAlways", config.rare_cheats.gem_upgrade_always);
            t.insert("LegendaryGemUpgradeSpeed", config.rare_cheats.gem_upgrade_speed);
            t.insert("LegendaryGemLevel150", config.rare_cheats.gem_upgrade_lvl150);
            t.insert("EquipMultipleLegendaries", config.rare_cheats.equip_multi_legendary);
            t.insert("SuperGodMode", config.rare_cheats.super_god_mode);
            t.insert("ExtraGreaterRiftOrbsOnEliteKill", config.rare_cheats.extra_gr_orbs_elites);
            root.insert("rare_cheats", std::move(t));
        }

        {
            toml::table t;
            t.insert("SectionEnabled", config.loot_modifiers.active);
            t.insert("DisableAncientDrops", config.loot_modifiers.DisableAncientDrops);
            t.insert("DisablePrimalAncientDrops", config.loot_modifiers.DisablePrimalAncientDrops);
            t.insert("DisableTormentDrops", config.loot_modifiers.DisableTormentDrops);
            t.insert("DisableTormentCheck", config.loot_modifiers.DisableTormentCheck);
            t.insert("SuppressGiftGeneration", config.loot_modifiers.SuppressGiftGeneration);
            t.insert("ForcedILevel", static_cast<s64>(config.loot_modifiers.ForcedILevel));
            t.insert("TieredLootRunLevel", static_cast<s64>(config.loot_modifiers.TieredLootRunLevel));
            t.insert("AncientRank", std::string(AncientRankCanonical(config.loot_modifiers.AncientRankValue, config.loot_modifiers.AncientRank)));
            root.insert("loot_modifiers", std::move(t));
        }

        d3::config_schema::InsertIntoToml(root, config);

        return root;
    }

}  // namespace

void PatchConfig::ApplyTable(const toml::table &table) {
    // Apply schema-backed settings first so the rest of the parser can assume a consistent baseline.
    d3::config_schema::ApplyTomlTable(*this, table);

    if (const auto *section = FindTable(table, "seasons")) {
        seasons.active         = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, seasons.active);
        seasons.allow_online   = ReadBool(*section, {"AllowOnlinePlay", "AllowOnline"}, seasons.allow_online);
        seasons.current_season = ReadU32(*section, {"SeasonNumber", "Number", "Season"}, seasons.current_season, 1, 200);
        seasons.spoof_ptr      = ReadBool(*section, {"SpoofPtr", "SpoofPTR", "PtrSpoof"}, seasons.spoof_ptr);
    }

    if (const auto *section = FindTable(table, "challenge_rifts")) {
        challenge_rifts.active      = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, challenge_rifts.active);
        challenge_rifts.random      = ReadBool(*section, {"MakeRiftsRandom", "Random"}, challenge_rifts.random);
        challenge_rifts.range_start = ReadU32(*section, {"RiftRangeStart", "RangeStart", "RangeMin"}, challenge_rifts.range_start, 0, 999);
        challenge_rifts.range_end   = ReadU32(*section, {"RiftRangeEnd", "RangeEnd", "RangeMax"}, challenge_rifts.range_end, 0, 999);
        if (challenge_rifts.range_start > challenge_rifts.range_end)
            std::swap(challenge_rifts.range_start, challenge_rifts.range_end);
    }

    if (const auto *section = FindTable(table, "events")) {
        events.active        = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, events.active);
        events.SeasonMapMode = ReadSeasonEventMapMode(*section, {"SeasonMapMode", "SeasonEventMapMode", "SeasonEventMap", "SeasonMap"}, events.SeasonMapMode);
#define D3HACK_SEASON_EVENT_TOML_READ(name, default_config, default_map, legacy_key) \
    events.name = ReadSeasonEventFlagBool(*section, #name, legacy_key, events.name);
        D3HACK_SEASON_EVENT_FLAGS(D3HACK_SEASON_EVENT_TOML_READ)
#undef D3HACK_SEASON_EVENT_TOML_READ
    }

    const auto *resolution_section = FindTable(table, "resolution_hack");
    if (resolution_section) {
        if (const auto *extra = FindTable(*resolution_section, "extra")) {
            using ExtraConfig   = PatchConfig::ResolutionHackConfig::ExtraConfig;
            const s32 min_value = ExtraConfig::kUnset;
            const s32 max_dim   = ExtraConfig::kMaxDimension;

            resolution_hack.extra.window_left = ReadI32(
                *extra, {"WindowLeft", "WinLeft", "Left"}, resolution_hack.extra.window_left, min_value, max_dim
            );
            resolution_hack.extra.window_top = ReadI32(
                *extra, {"WindowTop", "WinTop", "Top"}, resolution_hack.extra.window_top, min_value, max_dim
            );
            resolution_hack.extra.window_width = ReadI32(
                *extra, {"WindowWidth", "WinWidth", "Width"}, resolution_hack.extra.window_width, min_value, max_dim
            );
            resolution_hack.extra.window_height = ReadI32(
                *extra, {"WindowHeight", "WinHeight", "Height"}, resolution_hack.extra.window_height, min_value, max_dim
            );
            resolution_hack.extra.ui_opt_width = ReadI32(
                *extra, {"UIOptWidth", "UIWidth", "UiWidth"}, resolution_hack.extra.ui_opt_width, min_value, max_dim
            );
            resolution_hack.extra.ui_opt_height = ReadI32(
                *extra, {"UIOptHeight", "UIHeight", "UiHeight"}, resolution_hack.extra.ui_opt_height, min_value, max_dim
            );
            resolution_hack.extra.render_width = ReadI32(
                *extra, {"RenderWidth", "DisplayWidth", "BackbufferWidth"}, resolution_hack.extra.render_width, min_value, max_dim
            );
            resolution_hack.extra.render_height = ReadI32(
                *extra, {"RenderHeight", "DisplayHeight", "BackbufferHeight"}, resolution_hack.extra.render_height, min_value, max_dim
            );
            resolution_hack.extra.refresh_rate = ReadI32(
                *extra, {"RefreshRate", "Hz"}, resolution_hack.extra.refresh_rate, min_value, ExtraConfig::kMaxRefreshRate
            );
            resolution_hack.extra.bit_depth = ReadI32(
                *extra, {"BitDepth", "Bpp"}, resolution_hack.extra.bit_depth, min_value, ExtraConfig::kMaxBitDepth
            );
            resolution_hack.extra.msaa_level = ReadI32(
                *extra, {"MSAALevel", "MSAA", "Msaa"}, resolution_hack.extra.msaa_level, min_value, ExtraConfig::kMaxMsaaLevel
            );
        }
    }

    if (const auto *section = FindTable(table, "rare_cheats")) {
        rare_cheats.active                  = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, rare_cheats.active);
        rare_cheats.move_speed              = ReadDouble(*section, {"MovementSpeedMultiplier", "MoveSpeedMultiplier"}, rare_cheats.move_speed, 0.1, 10.0);
        rare_cheats.attack_speed            = ReadDouble(*section, {"AttackSpeedMultiplier", "AtkSpeedMultiplier", "AttackSpeed"}, rare_cheats.attack_speed, 0.1, 10.0);
        rare_cheats.floating_damage_color   = ReadBool(*section, {"FloatingDamageColor", "FloatingDamageColors", "FloatingDamage"}, rare_cheats.floating_damage_color);
        rare_cheats.guaranteed_legendaries  = ReadBool(*section, {"GuaranteedLegendaryChance"}, rare_cheats.guaranteed_legendaries);
        rare_cheats.drop_anything           = ReadBool(*section, {"DropAnyItems"}, rare_cheats.drop_anything);
        rare_cheats.instant_portal          = ReadBool(*section, {"InstantPortalAndBookOfCain"}, rare_cheats.instant_portal);
        rare_cheats.no_cooldowns            = ReadBool(*section, {"NoCooldowns"}, rare_cheats.no_cooldowns);
        rare_cheats.instant_craft_actions   = ReadBool(*section, {"InstantCubeAndCraftsAndEnchants"}, rare_cheats.instant_craft_actions);
        rare_cheats.any_gem_any_slot        = ReadBool(*section, {"SocketGemsToAnySlot"}, rare_cheats.any_gem_any_slot);
        rare_cheats.auto_pickup             = ReadBool(*section, {"AutoPickup"}, rare_cheats.auto_pickup);
        rare_cheats.equip_any_slot          = ReadBool(*section, {"EquipAnySlot", "EquipAnyItem"}, rare_cheats.equip_any_slot);
        rare_cheats.unlock_all_difficulties = ReadBool(*section, {"UnlockAllDifficulties", "UnlockDifficulties"}, rare_cheats.unlock_all_difficulties);
        rare_cheats.easy_kill_damage        = ReadBool(*section, {"EasyKillDamage", "EasyKillBoost"}, rare_cheats.easy_kill_damage);
        rare_cheats.infinite_mp             = ReadBool(*section, {"InfiniteMP", "InfiniteMana", "InfiniteResource"}, rare_cheats.infinite_mp);
        rare_cheats.cube_no_consume         = ReadBool(*section, {"CubeNoConsumeMaterials", "CubeNoConsume", "KanaiCubeNoConsume"}, rare_cheats.cube_no_consume);
        rare_cheats.gem_upgrade_always      = ReadBool(*section, {"LegendaryGemUpgradeAlways", "GemUpgradeAlways", "LegendaryGemUpgradeChance"}, rare_cheats.gem_upgrade_always);
        rare_cheats.gem_upgrade_speed       = ReadBool(*section, {"LegendaryGemUpgradeSpeed", "GemUpgradeSpeed", "GemUpgradeFast"}, rare_cheats.gem_upgrade_speed);
        rare_cheats.gem_upgrade_lvl150      = ReadBool(*section, {"LegendaryGemLevel150", "GemUpgradeLevel150", "GemLevel150"}, rare_cheats.gem_upgrade_lvl150);
        rare_cheats.equip_multi_legendary   = ReadBool(*section, {"EquipMultipleLegendaries", "EquipMultipleLegendaryItems", "EquipMultiLegendary"}, rare_cheats.equip_multi_legendary);
        rare_cheats.super_god_mode          = ReadBool(*section, {"SuperGodMode", "EnableSuperGodMode"}, rare_cheats.super_god_mode);
        rare_cheats.extra_gr_orbs_elites    = ReadBool(*section, {"ExtraGreaterRiftOrbsOnEliteKill", "ExtraGROrbsOnEliteKill", "ExtraGROrbsElites"}, rare_cheats.extra_gr_orbs_elites);
    }

    if (const auto *section = FindTable(table, "loot_modifiers")) {
        loot_modifiers.active                    = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, loot_modifiers.active);
        loot_modifiers.DisableAncientDrops       = ReadBool(*section, {"DisableAncientDrops"}, loot_modifiers.DisableAncientDrops);
        loot_modifiers.DisablePrimalAncientDrops = ReadBool(*section, {"DisablePrimalAncientDrops"}, loot_modifiers.DisablePrimalAncientDrops);
        loot_modifiers.DisableTormentDrops       = ReadBool(*section, {"DisableTormentDrops"}, loot_modifiers.DisableTormentDrops);
        loot_modifiers.DisableTormentCheck       = ReadBool(*section, {"DisableTormentCheck"}, loot_modifiers.DisableTormentCheck);
        loot_modifiers.SuppressGiftGeneration    = ReadBool(*section, {"SuppressGiftGeneration"}, loot_modifiers.SuppressGiftGeneration);
        loot_modifiers.ForcedILevel              = ReadI32(*section, {"ForcedILevel"}, loot_modifiers.ForcedILevel, 0, 70);
        loot_modifiers.TieredLootRunLevel        = ReadI32(*section, {"TieredLootRunLevel"}, loot_modifiers.TieredLootRunLevel, 0, 150);
        loot_modifiers.AncientRank               = ReadString(*section, {"AncientRank"}, loot_modifiers.AncientRank);
        loot_modifiers.AncientRankValue          = AncientRankToValue(loot_modifiers.AncientRank, loot_modifiers.AncientRankValue);
        loot_modifiers.AncientRank               = AncientRankCanonical(loot_modifiers.AncientRankValue, loot_modifiers.AncientRank);
    }

    ApplySeasonEventMapIfNeeded(*this);

    initialized   = true;
    defaults_only = false;
}

void LoadPatchConfig() {
    global_config    = PatchConfig {};
    const char *path = "sd:/config/d3hack-nx/config.toml";
    std::string error;
    if (LoadFromPath(path, global_config, error)) {
        PRINT("Loaded config: %s", path);
        global_config.initialized   = true;
        global_config.defaults_only = false;
        if (!d3::g_rt_scale_initialized) {
            d3::g_rt_scale             = global_config.resolution_hack.HandheldScaleFraction();
            d3::g_rt_scale_initialized = true;
        }
        return;
    }
    if (!error.empty())
        PRINT("Config parse failed for %s: %s", path, error.c_str());
    global_config.initialized   = true;
    global_config.defaults_only = true;
    PRINT("Config not found; using built-in defaults%s", ".");
    if (!d3::g_rt_scale_initialized) {
        d3::g_rt_scale             = global_config.resolution_hack.HandheldScaleFraction();
        d3::g_rt_scale_initialized = true;
    }
}

auto NormalizePatchConfig(const PatchConfig &config) -> PatchConfig {
    const toml::table root = BuildPatchConfigTable(config);
    PatchConfig       out {};
    out.ApplyTable(root);
    return out;
}

auto LoadPatchConfigFromPath(const char *path, PatchConfig &out, std::string &error_out) -> bool {
    out = PatchConfig {};
    if (LoadFromPath(path, out, error_out)) {
        return true;
    }
    return false;
}

auto SavePatchConfigToPath(const char *path, const PatchConfig &config, std::string &error_out) -> bool {
    const PatchConfig normalized = NormalizePatchConfig(config);
    const toml::table root       = BuildPatchConfigTable(normalized);
    auto             *allocator  = d3::system_allocator::GetSystemAllocator();
    if (allocator == nullptr) {
        error_out = "System allocator unavailable";
        return false;
    }

    d3::system_allocator::Buffer          buffer(allocator);
    d3::system_allocator::BufferStreamBuf streambuf(&buffer);
    std::ostream                          out(&streambuf);
    out << toml::toml_formatter {root};
    if (!buffer.ok()) {
        error_out = "Failed to allocate config buffer";
        return false;
    }
    return d3::fs_util::WriteAllAtomic(path, buffer.view(), "config file", error_out);
}

auto SavePatchConfig(const PatchConfig &config) -> bool {
    const char *path = "sd:/config/d3hack-nx/config.toml";
    std::string error;
    if (!SavePatchConfigToPath(path, config, error)) {
        if (!error.empty()) {
            PRINT("Config save failed for %s: %s", path, error.c_str());
        } else {
            PRINT("Config save failed for %s", path);
        }
        return false;
    }
    PRINT("Saved config: %s", path);
    return true;
}
