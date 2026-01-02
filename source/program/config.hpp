#pragma once
#include "types.h"
#include "tomlplusplus/toml.hpp"
#include <string>

struct PatchConfig {
    bool initialized = false;
    bool defaults_only = true;

    enum class SeasonEventMapMode : u8 {
        MapOnly,
        OverlayConfig,
        Disabled,
    };

    struct {
        bool active = true;
        bool allow_online = false;
        u32  number = 30;
    } seasons;

    struct {
        bool active = false;
        bool random = true;
        u32  range_start = 0;
        u32  range_end = 20;
    } challenge_rifts;

    struct {
        bool active = true;
        SeasonEventMapMode SeasonMapMode = SeasonEventMapMode::Disabled;
        bool IgrEnabled = false;
        bool AnniversaryEnabled = false;
        bool EasterEggWorldEnabled = false;
        bool DoubleRiftKeystones = true;
        bool DoubleBloodShards = true;
        bool DoubleTreasureGoblins = true;
        bool DoubleBountyBags = true;
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
        bool DarkAlchemy = true;
        bool NestingPortals = false;
    } events;

    struct {
        bool active = true;
        bool draw_fps = true;
        bool draw_var_res = true;
        bool fhd_mode = false;
        double move_speed = 2.5;
        double attack_speed = 1.0;
        bool floating_damage_color = false;
        bool font_hooks = false;
        bool guaranteed_legendaries = false;
        bool drop_anything = false;
        bool instant_portal = true;
        bool no_cooldowns = false;
        bool instant_craft_actions = true;
        bool any_gem_any_slot = false;
        bool auto_pickup = false;
        bool equip_any_slot = false;
    } rare_cheats;

    struct {
        bool active = true;
        bool buildlocker_watermark = true;
        bool ddm_labels = true;
        bool fps_label = true;
        bool var_res_label = true;
    } overlays;

    struct {
        bool        active = false;
        bool        DisableAncientDrops = false;
        bool        DisablePrimalAncientDrops = false;
        bool        DisableTormentDrops = false;
        bool        DisableTormentCheck = false;
        bool        SuppressGiftGeneration = true;
        u32         ForcedILevel = 70;
        u32         TieredLootRunLevel = 150;
        std::string AncientRank = "Primal";
        int         AncientRankValue = 2;
    } loot_modifiers;

    struct {
        bool active = true;
        bool enable_pubfile_dump = false;
        bool enable_error_traces = true;
        bool enable_debug_flags = false;
    } debug;

    void ApplyTable(const toml::table& table);
};

extern PatchConfig global_config;

// Loads config from TOML:
//   sd:/config/d3hack-nx/config.toml
// Logs and keeps defaults when not found/invalid.
void LoadPatchConfig();
