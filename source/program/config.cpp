#include "config.hpp"
#include "d3/setting.hpp"
#include "lib/diag/assert.hpp"
#include "nn/fs.hpp"
#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

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
        events.EthrealItems = ReadBool(*section, {"EthrealItems"}, events.EthrealItems);
        events.SoulShards = ReadBool(*section, {"SoulShards"}, events.SoulShards);
        events.SwarmRifts = ReadBool(*section, {"SwarmRifts"}, events.SwarmRifts);
        events.SanctifiedItems = ReadBool(*section, {"SanctifiedItems"}, events.SanctifiedItems);
        events.DarkAlchemy = ReadBool(*section, {"DarkAlchemy"}, events.DarkAlchemy);
        events.NestingPortals = ReadBool(*section, {"NestingPortals"}, events.NestingPortals);
    }

    if (const auto* section = FindTable(table, "rare_cheats")) {
        rare_cheats.active = ReadBool(*section, {"SectionEnabled", "Enabled", "Active"}, rare_cheats.active);
        rare_cheats.draw_fps = ReadBool(*section, {"DrawFPS"}, rare_cheats.draw_fps);
        rare_cheats.draw_var_res = ReadBool(*section, {"DrawVariableResolution"}, rare_cheats.draw_var_res);
        rare_cheats.fhd_mode = ReadBool(*section, {"Target1080pResolution"}, rare_cheats.fhd_mode);
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
        debug.enable_pubfile_dump = ReadBool(*section, {"EnablePubFileDump", "PubFileDump"}, debug.enable_pubfile_dump);
        debug.enable_error_traces = ReadBool(*section, {"EnableErrorTraces", "ErrorTraces"}, debug.enable_error_traces);
        debug.enable_debug_flags = ReadBool(*section, {"EnableDebugFlags", "DebugFlags"}, debug.enable_debug_flags);
    }

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
