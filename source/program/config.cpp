#include "config.hpp"
#include "d3/setting.hpp"
#include "lib/diag/assert.hpp"
#include "nn/fs.hpp"

PatchConfig global_config{};

namespace {
    const toml::table* FindTable(const toml::table& root, std::string_view key) {
        if (auto node = root.get(key); node && node->is_table())
            return node->as_table();
        return nullptr;
    }

    template<typename T>
    std::optional<T> ReadValue(const toml::table& table, std::initializer_list<std::string_view> keys) {
        for (auto key : keys) {
            if (auto node = table.get(key)) {
                if (auto value = node->value<T>())
                    return value;
            }
        }
        return std::nullopt;
    }

    bool ReadBool(const toml::table& table, std::initializer_list<std::string_view> keys, bool fallback) {
        if (auto value = ReadValue<bool>(table, keys))
            return *value;
        return fallback;
    }

    std::optional<double> ReadNumber(const toml::table& table, std::initializer_list<std::string_view> keys) {
        for (auto key : keys) {
            if (auto node = table.get(key)) {
                if (auto value = node->value<double>())
                    return value;
                if (auto value = node->value<int64_t>())
                    return static_cast<double>(*value);
            }
        }
        return std::nullopt;
    }

    u32 ReadU32(const toml::table& table, std::initializer_list<std::string_view> keys, u32 fallback, u32 min_value, u32 max_value) {
        if (auto value = ReadNumber(table, keys)) {
            auto clamped = std::clamp(*value, static_cast<double>(min_value), static_cast<double>(max_value));
            return static_cast<u32>(clamped);
        }
        return fallback;
    }

    double ReadDouble(const toml::table& table, std::initializer_list<std::string_view> keys, double fallback, double min_value, double max_value) {
        if (auto value = ReadNumber(table, keys)) {
            return std::clamp(*value, min_value, max_value);
        }
        return fallback;
    }

    std::string ReadString(const toml::table& table, std::initializer_list<std::string_view> keys, std::string fallback) {
        if (auto value = ReadValue<std::string>(table, keys))
            return *value;
        return fallback;
    }

    std::string NormalizeKey(std::string_view input) {
        std::string out;
        out.reserve(input.size());
        for (char ch : input) {
            unsigned char uc = static_cast<unsigned char>(ch);
            if (std::isspace(uc) || ch == '_' || ch == '-')
                continue;
            out.push_back(static_cast<char>(std::tolower(uc)));
        }
        return out;
    }

    PatchConfig::SeasonEventMapMode ParseSeasonEventMapMode(std::string_view input, PatchConfig::SeasonEventMapMode fallback) {
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

    PatchConfig::SeasonEventMapMode ReadSeasonEventMapMode(const toml::table& table, std::initializer_list<std::string_view> keys, PatchConfig::SeasonEventMapMode fallback) {
        if (auto value = ReadValue<std::string>(table, keys))
            return ParseSeasonEventMapMode(*value, fallback);
        if (auto value = ReadValue<bool>(table, keys))
            return *value ? PatchConfig::SeasonEventMapMode::MapOnly : PatchConfig::SeasonEventMapMode::Disabled;
        return fallback;
    }

    u32 ParseResolutionHackOutputTarget(std::string_view input, u32 fallback) {
        const auto normalized = NormalizeKey(input);
        if (normalized.empty())
            return fallback;
        if (normalized == "default" || normalized == "unchanged")
            return 900;
        if (normalized == "720" || normalized == "720p")
            return 720;
        if (normalized == "900" || normalized == "900p")
            return 900;
        if (normalized == "1080" || normalized == "1080p" || normalized == "fhd")
            return 1080;
        if (normalized == "1440" || normalized == "1440p" || normalized == "2k" || normalized == "qhd")
            return 1440;
        if (normalized == "2160willcrash")
            return 2160;

        u32         value = 0;
        const auto *begin = normalized.data();
        const auto *end   = normalized.data() + normalized.size();
        auto [ptr, ec]    = std::from_chars(begin, end, value);
        if (ec == std::errc() && ptr == end && value > 0)
            return value;
        return fallback;
    }

    u32 ReadResolutionHackOutputTarget(const toml::table &table, std::initializer_list<std::string_view> keys, u32 fallback) {
        if (auto value = ReadValue<int64_t>(table, keys)) {
            if (*value > 0 && *value <= std::numeric_limits<u32>::max())
                return static_cast<u32>(*value);
            return fallback;
        }
        if (auto value = ReadValue<std::string>(table, keys))
            return ParseResolutionHackOutputTarget(*value, fallback);
        return fallback;
    }

    struct SeasonEventFlags {
        bool IgrEnabled = false;
        bool AnniversaryEnabled = false;
        bool EasterEggWorldEnabled = false;
        bool DoubleRiftKeystones = false;
        bool DoubleBloodShards = false;
        bool DoubleTreasureGoblins = false;
        bool DoubleBountyBags = false;
        bool RoyalGrandeur = false;
        bool LegacyOfNightmares = false;
        bool TriunesWill = false;
        bool Pandemonium = false;
        bool KanaiPowers = false;
        bool TrialsOfTempests = false;
        bool ShadowClones = false;
        bool FourthKanaisCubeSlot = false;
        bool EtherealItems = false;
        bool SoulShards = false;
        bool SwarmRifts = false;
        bool SanctifiedItems = false;
        bool DarkAlchemy = false;
        bool NestingPortals = false;
    };

    auto CaptureSeasonEventFlags(const PatchConfig& config) -> SeasonEventFlags {
        return {
            config.events.IgrEnabled,
            config.events.AnniversaryEnabled,
            config.events.EasterEggWorldEnabled,
            config.events.DoubleRiftKeystones,
            config.events.DoubleBloodShards,
            config.events.DoubleTreasureGoblins,
            config.events.DoubleBountyBags,
            config.events.RoyalGrandeur,
            config.events.LegacyOfNightmares,
            config.events.TriunesWill,
            config.events.Pandemonium,
            config.events.KanaiPowers,
            config.events.TrialsOfTempests,
            config.events.ShadowClones,
            config.events.FourthKanaisCubeSlot,
            config.events.EtherealItems,
            config.events.SoulShards,
            config.events.SwarmRifts,
            config.events.SanctifiedItems,
            config.events.DarkAlchemy,
            config.events.NestingPortals,
        };
    }

    void ApplySeasonEventFlags(PatchConfig& config, const SeasonEventFlags& flags) {
        config.events.IgrEnabled = flags.IgrEnabled;
        config.events.AnniversaryEnabled = flags.AnniversaryEnabled;
        config.events.EasterEggWorldEnabled = flags.EasterEggWorldEnabled;
        config.events.DoubleRiftKeystones = flags.DoubleRiftKeystones;
        config.events.DoubleBloodShards = flags.DoubleBloodShards;
        config.events.DoubleTreasureGoblins = flags.DoubleTreasureGoblins;
        config.events.DoubleBountyBags = flags.DoubleBountyBags;
        config.events.RoyalGrandeur = flags.RoyalGrandeur;
        config.events.LegacyOfNightmares = flags.LegacyOfNightmares;
        config.events.TriunesWill = flags.TriunesWill;
        config.events.Pandemonium = flags.Pandemonium;
        config.events.KanaiPowers = flags.KanaiPowers;
        config.events.TrialsOfTempests = flags.TrialsOfTempests;
        config.events.ShadowClones = flags.ShadowClones;
        config.events.FourthKanaisCubeSlot = flags.FourthKanaisCubeSlot;
        config.events.EtherealItems = flags.EtherealItems;
        config.events.SoulShards = flags.SoulShards;
        config.events.SwarmRifts = flags.SwarmRifts;
        config.events.SanctifiedItems = flags.SanctifiedItems;
        config.events.DarkAlchemy = flags.DarkAlchemy;
        config.events.NestingPortals = flags.NestingPortals;
    }

    auto OverlaySeasonEventFlags(const SeasonEventFlags& map, const SeasonEventFlags& extra) -> SeasonEventFlags {
        SeasonEventFlags out = map;
        out.IgrEnabled |= extra.IgrEnabled;
        out.AnniversaryEnabled |= extra.AnniversaryEnabled;
        out.EasterEggWorldEnabled |= extra.EasterEggWorldEnabled;
        out.DoubleRiftKeystones |= extra.DoubleRiftKeystones;
        out.DoubleBloodShards |= extra.DoubleBloodShards;
        out.DoubleTreasureGoblins |= extra.DoubleTreasureGoblins;
        out.DoubleBountyBags |= extra.DoubleBountyBags;
        out.RoyalGrandeur |= extra.RoyalGrandeur;
        out.LegacyOfNightmares |= extra.LegacyOfNightmares;
        out.TriunesWill |= extra.TriunesWill;
        out.Pandemonium |= extra.Pandemonium;
        out.KanaiPowers |= extra.KanaiPowers;
        out.TrialsOfTempests |= extra.TrialsOfTempests;
        out.ShadowClones |= extra.ShadowClones;
        out.FourthKanaisCubeSlot |= extra.FourthKanaisCubeSlot;
        out.EtherealItems |= extra.EtherealItems;
        out.SoulShards |= extra.SoulShards;
        out.SwarmRifts |= extra.SwarmRifts;
        out.SanctifiedItems |= extra.SanctifiedItems;
        out.DarkAlchemy |= extra.DarkAlchemy;
        out.NestingPortals |= extra.NestingPortals;
        return out;
    }

    bool BuildSeasonEventMap(u32 season, SeasonEventFlags& out) {
        out = SeasonEventFlags{};
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
            out.ShadowClones = true;
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
            out.ShadowClones = true;
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

    void ApplySeasonEventMapIfNeeded(PatchConfig& config) {
        if (config.events.SeasonMapMode == PatchConfig::SeasonEventMapMode::Disabled)
            return;
        if (!config.seasons.active)
            return;
        SeasonEventFlags map{};
        if (!BuildSeasonEventMap(config.seasons.number, map))
            return;
        auto current = CaptureSeasonEventFlags(config);
        auto merged = config.events.SeasonMapMode == PatchConfig::SeasonEventMapMode::OverlayConfig
            ? OverlaySeasonEventFlags(map, current)
            : map;
        ApplySeasonEventFlags(config, merged);
    }

    int AncientRankToValue(std::string_view input, int fallback) {
        auto normalized = NormalizeKey(input);
        if (normalized == "0" || normalized == "normal" || normalized == "none")
            return 0;
        if (normalized == "1" || normalized == "ancient")
            return 1;
        if (normalized == "2" || normalized == "primal" || normalized == "primalancient")
            return 2;
        return fallback;
    }

    std::string AncientRankCanonical(int value, std::string_view fallback) {
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

    bool DoesFileExist(const char* path) {
        nn::fs::DirectoryEntryType type{};
        auto rc = nn::fs::GetEntryType(&type, path);
        return R_SUCCEEDED(rc) && type == nn::fs::DirectoryEntryType_File;
    }

    bool ReadAll(const char* path, std::string& out) {
        if (!DoesFileExist(path))
            return false;
        nn::fs::FileHandle fh{};
        auto rc = nn::fs::OpenFile(&fh, path, nn::fs::OpenMode_Read);
        EXL_ASSERT(R_SUCCEEDED(rc), "Failed to open file: %s", path);

        s64 size = 0;
        rc = nn::fs::GetFileSize(&size, fh);
        if (R_FAILED(rc) || size <= 0) {
            nn::fs::CloseFile(fh);
            return false;
        }

        out.resize(static_cast<size_t>(size));
        rc = nn::fs::ReadFile(fh, 0, out.data(), static_cast<u64>(size));
        EXL_ASSERT(R_SUCCEEDED(rc), "Failed to read file: %s", path);
        nn::fs::CloseFile(fh);
        return true;
    }

    bool LoadFromPath(const char* path, std::string& error_out) {
        std::string text;
        if (!ReadAll(path, text))
            return false;
        auto result = toml::parse(text, std::string_view{path});
        if (!result) {
            const auto& err = result.error();
            auto pos = err.source().begin;
            error_out = std::string(err.description());
            if (pos) {
                error_out += " (line ";
                error_out += std::to_string(pos.line);
                error_out += ", col ";
                error_out += std::to_string(pos.column);
                error_out += ")";
            }
            return false;
        }
        global_config.ApplyTable(result.table());
        return true;
    }
}

void PatchConfig::ApplyTable(const toml::table& table) {
    if (const auto* section = FindTable(table, "seasons")) {
        seasons.active = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, seasons.active);
        seasons.allow_online = ReadBool(*section, {"AllowOnlinePlay", "AllowOnline"}, seasons.allow_online);
        seasons.number = ReadU32(*section, {"SeasonNumber", "Number", "Season"}, seasons.number, 1, 200);
    }

    if (const auto* section = FindTable(table, "challenge_rifts")) {
        challenge_rifts.active = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, challenge_rifts.active);
        challenge_rifts.random = ReadBool(*section, {"MakeRiftsRandom", "Random"}, challenge_rifts.random);
        challenge_rifts.range_start = ReadU32(*section, {"RiftRangeStart", "RangeStart", "RangeMin"}, challenge_rifts.range_start, 0, 999);
        challenge_rifts.range_end = ReadU32(*section, {"RiftRangeEnd", "RangeEnd", "RangeMax"}, challenge_rifts.range_end, 0, 999);
        if (challenge_rifts.range_start > challenge_rifts.range_end)
            std::swap(challenge_rifts.range_start, challenge_rifts.range_end);
    }

    if (const auto* section = FindTable(table, "events")) {
        events.active = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, events.active);
        events.SeasonMapMode = ReadSeasonEventMapMode(*section, {"SeasonMapMode", "SeasonEventMapMode", "SeasonEventMap", "SeasonMap"}, events.SeasonMapMode);
        events.IgrEnabled = ReadBool(*section, {"IgrEnabled"}, events.IgrEnabled);
        events.AnniversaryEnabled = ReadBool(*section, {"AnniversaryEnabled"}, events.AnniversaryEnabled);
        events.EasterEggWorldEnabled = ReadBool(*section, {"EasterEggWorldEnabled"}, events.EasterEggWorldEnabled);
        events.DoubleRiftKeystones = ReadBool(*section, {"DoubleRiftKeystones"}, events.DoubleRiftKeystones);
        events.DoubleBloodShards = ReadBool(*section, {"DoubleBloodShards"}, events.DoubleBloodShards);
        events.DoubleTreasureGoblins = ReadBool(*section, {"DoubleTreasureGoblins"}, events.DoubleTreasureGoblins);
        events.DoubleBountyBags = ReadBool(*section, {"DoubleBountyBags"}, events.DoubleBountyBags);
        events.RoyalGrandeur = ReadBool(*section, {"RoyalGrandeur"}, events.RoyalGrandeur);
        events.LegacyOfNightmares = ReadBool(*section, {"LegacyOfNightmares"}, events.LegacyOfNightmares);
        events.TriunesWill = ReadBool(*section, {"TriunesWill"}, events.TriunesWill);
        events.Pandemonium = ReadBool(*section, {"Pandemonium"}, events.Pandemonium);
        events.KanaiPowers = ReadBool(*section, {"KanaiPowers"}, events.KanaiPowers);
        events.TrialsOfTempests = ReadBool(*section, {"TrialsOfTempests"}, events.TrialsOfTempests);
        events.ShadowClones = ReadBool(*section, {"ShadowClones"}, events.ShadowClones);
        events.FourthKanaisCubeSlot = ReadBool(*section, {"FourthKanaisCubeSlot"}, events.FourthKanaisCubeSlot);
        events.EtherealItems = ReadBool(*section, {"EtherealItems", "EthrealItems"}, events.EtherealItems);
        events.SoulShards = ReadBool(*section, {"SoulShards"}, events.SoulShards);
        events.SwarmRifts = ReadBool(*section, {"SwarmRifts"}, events.SwarmRifts);
        events.SanctifiedItems = ReadBool(*section, {"SanctifiedItems"}, events.SanctifiedItems);
        events.DarkAlchemy = ReadBool(*section, {"DarkAlchemy"}, events.DarkAlchemy);
        events.NestingPortals = ReadBool(*section, {"NestingPortals"}, events.NestingPortals);
    }

    const auto *resolution_section = FindTable(table, "resolution_hack");
    if (resolution_section) {
        resolution_hack.active = ReadBool(*resolution_section, {"SectionEnabled", "Enabled", "Active"}, resolution_hack.active);
        const auto target_resolution = ReadResolutionHackOutputTarget(
            *resolution_section,
            {"OutputTarget", "OutputHeight", "Height", "OutputVertical", "Vertical", "OutputRes", "OutputResolution", "OutputWidth", "Width"},
            resolution_hack.target_resolution
        );
        resolution_hack.SetTargetRes(target_resolution);
        resolution_hack.min_res_scale = ReadDouble(
            *resolution_section,
            {"MinScale", "MinimumScale", "MinResScale", "MinimumResScale", "MinResolutionScale", "MinimumResolutionScale"},
            resolution_hack.min_res_scale, 10.0, 100.0
        );
        resolution_hack.clamp_textures_2048 = ReadBool(
            *resolution_section,
            {"ClampTextures2048", "ClampTexturesTo2048", "ClampTextures", "ClampTex2048", "ClampTex"},
            resolution_hack.clamp_textures_2048
        );
    } else {
        resolution_hack.SetTargetRes(resolution_hack.target_resolution);
    }

    if (const auto* section = FindTable(table, "rare_cheats")) {
        rare_cheats.active = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, rare_cheats.active);
        rare_cheats.move_speed = ReadDouble(*section, {"MovementSpeedMultiplier", "MoveSpeedMultiplier"}, rare_cheats.move_speed, 0.1, 10.0);
        rare_cheats.guaranteed_legendaries = ReadBool(*section, {"GuaranteedLegendaryChance"}, rare_cheats.guaranteed_legendaries);
        rare_cheats.drop_anything = ReadBool(*section, {"DropAnyItems"}, rare_cheats.drop_anything);
        rare_cheats.instant_portal = ReadBool(*section, {"InstantPortalAndBookOfCain"}, rare_cheats.instant_portal);
        rare_cheats.no_cooldowns = ReadBool(*section, {"NoCooldowns"}, rare_cheats.no_cooldowns);
        rare_cheats.instant_craft_actions = ReadBool(*section, {"InstantCubeAndCraftsAndEnchants"}, rare_cheats.instant_craft_actions);
        rare_cheats.any_gem_any_slot = ReadBool(*section, {"SocketGemsToAnySlot"}, rare_cheats.any_gem_any_slot);
        rare_cheats.auto_pickup = ReadBool(*section, {"AutoPickup"}, rare_cheats.auto_pickup);
        rare_cheats.equip_any_slot = ReadBool(*section, {"EquipAnySlot", "EquipAnyItem"}, rare_cheats.equip_any_slot);
    }

    if (const auto* section = FindTable(table, "overlays")) {
        overlays.active = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, overlays.active);
        overlays.buildlocker_watermark = ReadBool(*section, {"BuildlockerWatermark", "BuildLockerWatermark"}, overlays.buildlocker_watermark);
        overlays.ddm_labels = ReadBool(*section, {"DDMLabels", "DebugLabels"}, overlays.ddm_labels);
        overlays.fps_label = ReadBool(*section, {"FPSLabel", "DrawFPSLabel"}, overlays.fps_label);
        overlays.var_res_label = ReadBool(*section, {"VariableResLabel", "VarResLabel", "DrawVariableResolutionLabel"}, overlays.var_res_label);
    }

    if (const auto* section = FindTable(table, "loot_modifiers")) {
        loot_modifiers.active = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, loot_modifiers.active);
        loot_modifiers.DisableAncientDrops = ReadBool(*section, {"DisableAncientDrops"}, loot_modifiers.DisableAncientDrops);
        loot_modifiers.DisablePrimalAncientDrops = ReadBool(*section, {"DisablePrimalAncientDrops"}, loot_modifiers.DisablePrimalAncientDrops);
        loot_modifiers.DisableTormentDrops = ReadBool(*section, {"DisableTormentDrops"}, loot_modifiers.DisableTormentDrops);
        loot_modifiers.DisableTormentCheck = ReadBool(*section, {"DisableTormentCheck"}, loot_modifiers.DisableTormentCheck);
        loot_modifiers.SuppressGiftGeneration = ReadBool(*section, {"SuppressGiftGeneration"}, loot_modifiers.SuppressGiftGeneration);
        loot_modifiers.ForcedILevel = ReadU32(*section, {"ForcedILevel"}, loot_modifiers.ForcedILevel, 1, 70);
        loot_modifiers.TieredLootRunLevel = ReadU32(*section, {"TieredLootRunLevel"}, loot_modifiers.TieredLootRunLevel, 1, 150);
        loot_modifiers.AncientRank = ReadString(*section, {"AncientRank"}, loot_modifiers.AncientRank);
        loot_modifiers.AncientRankValue = AncientRankToValue(loot_modifiers.AncientRank, loot_modifiers.AncientRankValue);
        loot_modifiers.AncientRank = AncientRankCanonical(loot_modifiers.AncientRankValue, loot_modifiers.AncientRank);
    }

    if (const auto* section = FindTable(table, "debug")) {
        debug.active = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, debug.active);
        debug.enable_crashes = ReadBool(*section, {"Acknowledge_god_jester"}, debug.enable_crashes);
        debug.enable_pubfile_dump = ReadBool(*section, {"EnablePubFileDump", "PubFileDump"}, debug.enable_pubfile_dump);
        debug.enable_error_traces = ReadBool(*section, {"EnableErrorTraces", "ErrorTraces"}, debug.enable_error_traces);
        debug.enable_debug_flags = ReadBool(*section, {"EnableDebugFlags", "DebugFlags"}, debug.enable_debug_flags);
    }

    ApplySeasonEventMapIfNeeded(*this);

    initialized = true;
    defaults_only = false;
}

void LoadPatchConfig() {
    global_config = PatchConfig{};
    const char* path = "sd:/config/d3hack-nx/config.toml";
    std::string error;
    if (LoadFromPath(path, error)) {
        PRINT("Loaded config: %s", path);
        global_config.initialized = true;
        global_config.defaults_only = false;
        return;
    }
    if (!error.empty())
        PRINT("Config parse failed for %s: %s", path, error.c_str());
    global_config.initialized = true;
    global_config.defaults_only = true;
    PRINT("Config not found; using built-in defaults%s", ".");
}
