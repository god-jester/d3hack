#pragma once

#include "d3/types/error_manager.hpp"
#include "d3/types/enums.hpp"
#include "symbols/common.hpp"

#include <unordered_map>

namespace GFXNX64NVN {
    struct Globals;
}

namespace d3 {

    // Runtime globals are declared here and defined once in a .cpp file.
    // This prevents ODR/multiple-definition issues when including this header from multiple TUs.
    extern bool                     sg_bServerCode;
    extern SigmaThreadLocalStorage &g_tThreadData;
    extern AttribDef *const         g_arAttribDefs;
    extern AppGlobals              &g_tAppGlobals;
    extern OnlineService::ItemId   &g_itemInvalid;
    extern WorldPlace              &g_cPlaceNull;
    extern SigmaGlobals            &g_tSigmaGlobals;
    extern u32                     *g_ptDevMemMode;
    extern ::GFXNX64NVN::Globals   *g_ptGfxNVNGlobals;
    extern GameCommonData          *g_ptGCData;
    extern GfxInternalData         *g_ptGfxData;
    extern RWindow                 *g_ptMainRWindow;
    extern VariableResRWindowData  *g_ptRWindowData;
    extern void                    *g_ptLobbyServiceInternal;
    extern GameConnectionID         g_idGameConnection;
    extern bool                     g_requestSeasonsLoad;

    extern uintptr_t &g_varLocalLoggingEnable;
    extern uintptr_t &g_varOnlineServicePTR;
    extern uintptr_t &g_varFreeToPlay;
    extern uintptr_t &g_varSeasonsOverrideEnabled;
    extern uintptr_t &g_varChallengeRiftEnabled;
    extern uintptr_t &g_varSeasonNum;
    extern uintptr_t &g_varSeasonState;
    extern uintptr_t &g_varMaxParagonLevel;
    extern uintptr_t &g_varExperimentalScheduling;
    extern uintptr_t &g_varDoubleRiftKeystones;
    extern uintptr_t &g_varDoubleBloodShards;
    extern uintptr_t &g_varDoubleTreasureGoblins;
    extern uintptr_t &g_varDoubleBountyBags;
    extern uintptr_t &g_varRoyalGrandeur;
    extern uintptr_t &g_varLegacyOfNightmares;
    extern uintptr_t &g_varTriunesWill;
    extern uintptr_t &g_varPandemonium;
    extern uintptr_t &g_varKanaiPowers;
    extern uintptr_t &g_varTrialsOfTempests;
    extern uintptr_t &g_varShadowClones;
    extern uintptr_t &g_varFourthKanaisCubeSlot;
    extern uintptr_t &g_varEtherealItems;
    extern uintptr_t &g_varSoulShards;
    extern uintptr_t &g_varSwarmRifts;
    extern uintptr_t &g_varSanctifiedItems;
    extern uintptr_t &g_varDarkAlchemy;
    extern uintptr_t &g_varNestingPortals;
    extern uintptr_t &g_varParagonCap;

    extern std::unordered_map<ACDID, bool>   g_mapLobby;
    extern std::unordered_map<Attrib, int32> g_mapCheckedAttribs;

}  // namespace d3
