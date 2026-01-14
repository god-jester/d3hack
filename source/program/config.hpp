#pragma once
#include "types.h"
#include "tomlplusplus/toml.hpp"
#include <string>

struct PatchConfig {
    bool initialized   = false;
    bool defaults_only = true;

    enum class SeasonEventMapMode : u8 {
        MapOnly,
        OverlayConfig,
        Disabled,
    };

    struct ResolutionHackConfig {
        bool  active              = true;
        u32   target_resolution   = 1080;   // boosted docked resolution
        float min_res_scale       = 85.0f;  // boosted; default is 70%
        bool  clamp_textures_2048 = false;
        bool  exp_scheduler       = false;

        static constexpr float kAspectRatio = 16.0f / 9.0f;

        static constexpr u32 WidthForHeight(u32 height) {
            return static_cast<u32>(height * kAspectRatio);
        }

        constexpr void SetTargetRes(u32 height) { target_resolution = height; }

        constexpr u32 OutputWidthPx() const { return WidthForHeight(target_resolution); }
        constexpr u32 OutputHeightPx() const { return target_resolution; }
    };

    struct {
        bool active         = true;
        bool allow_online   = false;
        u32  current_season = 30;
        bool spoof_ptr      = false;
    } seasons;

    struct {
        bool active      = false;
        bool random      = true;
        u32  range_start = 0;
        u32  range_end   = 20;
    } challenge_rifts;

    struct {
        bool               active                = true;
        SeasonEventMapMode SeasonMapMode         = SeasonEventMapMode::Disabled;
        bool               IgrEnabled            = false;
        bool               AnniversaryEnabled    = false;
        bool               EasterEggWorldEnabled = false;
        bool               DoubleRiftKeystones   = true;
        bool               DoubleBloodShards     = true;
        bool               DoubleTreasureGoblins = true;
        bool               DoubleBountyBags      = true;
        bool               RoyalGrandeur         = false;
        bool               LegacyOfNightmares    = false;
        bool               TriunesWill           = false;
        bool               Pandemonium           = false;
        bool               KanaiPowers           = false;
        bool               TrialsOfTempests      = false;
        bool               ShadowClones          = false;
        bool               FourthKanaisCubeSlot  = false;
        bool               EtherealItems         = false;
        bool               SoulShards            = false;
        bool               SwarmRifts            = false;
        bool               SanctifiedItems       = false;
        bool               DarkAlchemy           = true;
        bool               NestingPortals        = false;
    } events;

    struct {
        bool   active                  = true;
        double move_speed              = 2.5;
        double attack_speed            = 1.0;
        bool   floating_damage_color   = false;
        bool   guaranteed_legendaries  = false;
        bool   drop_anything           = false;
        bool   instant_portal          = true;
        bool   no_cooldowns            = false;
        bool   instant_craft_actions   = true;
        bool   any_gem_any_slot        = false;
        bool   auto_pickup             = false;
        bool   equip_any_slot          = false;
        bool   unlock_all_difficulties = true;
        bool   easy_kill_damage        = false;
        bool   cube_no_consume         = false;
        bool   gem_upgrade_always      = false;
        bool   gem_upgrade_speed       = true;
        bool   gem_upgrade_lvl150      = false;
        bool   equip_multi_legendary   = true;
    } rare_cheats;

    ResolutionHackConfig resolution_hack {};

    struct {
        bool active                = true;
        bool buildlocker_watermark = false;
        bool ddm_labels            = true;
        bool fps_label             = false;
        bool var_res_label         = true;
    } overlays;

    struct {
        bool        active                    = false;
        bool        DisableAncientDrops       = false;
        bool        DisablePrimalAncientDrops = false;
        bool        DisableTormentDrops       = false;
        bool        DisableTormentCheck       = false;
        bool        SuppressGiftGeneration    = true;
        u32         ForcedILevel              = 0;
        u32         TieredLootRunLevel        = 0;
        std::string AncientRank               = "Primal";
        int         AncientRankValue          = 2;
    } loot_modifiers;

    struct {
        bool active              = true;
        bool enable_crashes      = false;
        bool enable_pubfile_dump = false;
        bool enable_error_traces = true;
        bool enable_debug_flags  = false;
    } debug;

    struct {
        bool        enabled = true;        // render the ImGui UI (proof-of-life stays separate)
        bool        visible = true;        // window visible by default
        std::string language_override {};  // optional; when set, overrides game locale for GUI translations (e.g. "zh")
    } gui;

    void ApplyTable(const toml::table &table);
};

extern PatchConfig global_config;

// Loads config from TOML:
//   sd:/config/d3hack-nx/config.toml
// Logs and keeps defaults when not found/invalid.
void LoadPatchConfig();

// Normalize/clamp config using the same rules as TOML load.
PatchConfig NormalizePatchConfig(const PatchConfig &config);

// Load config from a specific path into an output struct (does not mutate global_config).
bool LoadPatchConfigFromPath(const char *path, PatchConfig &out, std::string &error_out);

// Save config to TOML (best-effort). Writes to a temp file and renames into place.
bool SavePatchConfigToPath(const char *path, const PatchConfig &config, std::string &error_out);

// Default path: sd:/config/d3hack-nx/config.toml
bool SavePatchConfig(const PatchConfig &config);
