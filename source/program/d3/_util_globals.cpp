#include "d3/_util.hpp"
#include "d3/types/gfx.hpp"

namespace d3 {
    // Keep runtime/relocation-backed globals in a single translation unit to avoid
    // ODR violations and dynamic initialization in headers.

    bool sg_bServerCode = true;

    SigmaThreadLocalStorage &g_tThreadData     = *reinterpret_cast<SigmaThreadLocalStorage *const>(GameOffsetFromTable("sigma_thread_data"));
    AttribDef *const         g_arAttribDefs    = reinterpret_cast<AttribDef *const>(GameOffsetFromTable("attrib_defs"));
    AppGlobals              &g_tAppGlobals     = *reinterpret_cast<AppGlobals *const>(GameOffsetFromTable("app_globals"));
    OnlineService::ItemId   &g_itemInvalid     = *reinterpret_cast<OnlineService::ItemId *const>(GameOffsetFromTable("item_invalid"));
    WorldPlace              &g_cPlaceNull      = *reinterpret_cast<WorldPlace *const>(GameOffsetFromTable("world_place_null"));
    SigmaGlobals            &g_tSigmaGlobals   = *reinterpret_cast<SigmaGlobals *const>(GameOffsetFromTable("sigma_globals"));
    u32                     *g_ptDevMemMode    = *reinterpret_cast<u32 **const>(GameOffsetFromTable("gb_dev_mem_mode_ptr"));
    GFXNX64NVN::Globals     *g_ptGfxNVNGlobals = *reinterpret_cast<GFXNX64NVN::Globals *const *>(GameOffsetFromTable("gfx_nvn_globals_ptr"));

    GameCommonData         *g_ptGCData               = nullptr;
    GfxInternalData        *g_ptGfxData              = nullptr;
    RWindow                *g_ptMainRWindow          = nullptr;
    VariableResRWindowData *g_ptRWindowData          = nullptr;
    void                   *g_ptLobbyServiceInternal = nullptr;
    GameConnectionID        g_idGameConnection       = {};
    bool                    g_requestSeasonsLoad     = false;

    uintptr_t &g_varLocalLoggingEnable     = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_local_logging_enable_ptr"));
    uintptr_t &g_varOnlineServicePTR       = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_online_service_ptr"));
    uintptr_t &g_varFreeToPlay             = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_free_to_play_ptr"));
    uintptr_t &g_varSeasonsOverrideEnabled = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_seasons_override_enabled_ptr"));
    uintptr_t &g_varChallengeRiftEnabled   = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_challengerift_enabled_ptr"));
    uintptr_t &g_varSeasonNum              = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_season_num_ptr"));
    uintptr_t &g_varSeasonState            = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_season_state_ptr"));
    uintptr_t &g_varMaxParagonLevel        = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_max_paragon_level_ptr"));
    uintptr_t &g_varExperimentalScheduling = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_experimental_scheduling_ptr"));

    uintptr_t &g_varDoubleRiftKeystones   = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_double_keystones"));
    uintptr_t &g_varDoubleBloodShards     = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_double_blood_shards"));
    uintptr_t &g_varDoubleTreasureGoblins = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_double_goblins"));
    uintptr_t &g_varDoubleBountyBags      = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_double_bounty_bags"));
    uintptr_t &g_varRoyalGrandeur         = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_royal_grandeur"));
    uintptr_t &g_varLegacyOfNightmares    = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_legacy_of_nightmares"));
    uintptr_t &g_varTriunesWill           = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_triunes_will"));
    uintptr_t &g_varPandemonium           = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_pandemonium"));
    uintptr_t &g_varKanaiPowers           = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_kanai_powers"));
    uintptr_t &g_varTrialsOfTempests      = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_trials_of_tempests"));
    uintptr_t &g_varShadowClones          = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_shadow_clones"));
    uintptr_t &g_varFourthKanaisCubeSlot  = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_fourth_cube_slot"));
    uintptr_t &g_varEtherealItems         = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_ethereal_items"));
    uintptr_t &g_varSoulShards            = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_soul_shards"));
    uintptr_t &g_varSwarmRifts            = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_swarm_rifts"));
    uintptr_t &g_varSanctifiedItems       = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_sanctified_items"));
    uintptr_t &g_varDarkAlchemy           = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_dark_alchemy"));
    uintptr_t &g_varNestingPortals        = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_nesting_portals"));
    uintptr_t &g_varParagonCap            = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_paragon_cap"));

    std::unordered_map<ACDID, bool>   g_mapLobby {};
    std::unordered_map<Attrib, int32> g_mapCheckedAttribs {};
}  // namespace d3
