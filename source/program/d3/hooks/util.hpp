#include "../../config.hpp"
#include "../patches.hpp"
#include "d3/types/common.hpp"
#include "lib/hook/inline.hpp"
#include "lib/hook/replace.hpp"
#include "lib/hook/trampoline.hpp"
#include <oe.hpp>

namespace nn::os {
    auto GetHostArgc() -> u64;
    auto GetHostArgv() -> const char **;
};  // namespace nn::os

namespace d3 {

    inline bool g_testRanOnce = false;
    inline int  g_refCount    = 0;
    inline int  g_drawCount   = 0;
    inline bool g_pending_perf_notify = false;
    inline bool g_pending_dock_notify = false;

    inline void RequestPerfNotify() { g_pending_perf_notify = true; }
    inline void RequestDockNotify() { g_pending_dock_notify = true; }

    inline constinit const char *g_cmdArgs[] = {
        "<<main>>",
        "-ignoreversioncheck",
        // "-w",
        "-d3d11",
        "-launch",
        // "-locale",
        // "-battlenetaddress",
        // "-battlenetaccount",
        // "-battlenetpassword",
        "-autologin",
        // "-fakeinput",
        // "-noinput",
        // "-quicktest",
        // "-test",
        // "-perftestminfpsscreenshot",
        // "-perftestfilter",
        // "-perftestlist",
        // "-perftestentrypoint",
        // "-perftestwaittime",
        // "-perftestsampletime",
        // "-perftestdumpfreq",
        // "-perftestplayernum",
        // "-perftestformat",
        "-ngdp",
        "-cdp",
        // "-perftestclass",
        // "-blizzcon",
        // "-blizzconpvp",
        // "-blizzcondrop",
        // "-joingamename",
        // "-jointeam",
        // "-gametimelimit",
        // "-uid",
        // "-agenttype",
        "-3dv",
        "-highdpicursor",
        // "-cursorignorehighdpi",
        "-nofpslimit",
        "-usepageflip",
        // "-debugd3d",
        "-overridelogfolder",
        "sd:/config/d3hack-nx/",
        // "-overridefmodemergencymem",
        // "-nod4license"
    };

    HOOK_DEFINE_TRAMPOLINE(CmdLineParse) {
        static void Callback(const char *szCmdLine) {
            Orig(szCmdLine);
            return;
            auto &array = CmdLineGetParams()->tBitArray;
            array.SetNthBit(38, true);
            array.SetNthBit(40, true);
            for (int i = 0; i < 62; ++i) {
                PRINT_EXPR("%i %i", i, CmdLineGetParams()->tBitArray.GetNthBit(i));
            }
        }
    };

    HOOK_DEFINE_TRAMPOLINE(AttackSpeed) {
        // float PowerGetFormulaValueAtLevel(AttribGroupID idFastAttrib, const ActorCommonData *ptACD, SNO snoPower, uint8 *pbFormula, int32 nFormulaSize, int32 nLevel)
        static auto Callback(AttribGroupID idFastAttrib, ActorCommonData *ptACD, SNO snoPower, uint8 *pbFormula, int32 nFormulaSize, int32 nLevel) -> float {
            auto ret = Orig(idFastAttrib, ptACD, snoPower, pbFormula, nFormulaSize, nLevel);
            if (snoPower != 0x50860)
                return ret;
            FastAttribKey tKey;
            tKey.nValue = ATTACKS_PER_SECOND_TOTAL, ACD_AttributesSetFloat(ptACD, tKey, 3.0f);
            PRINT_EXPR("ORIG ATKSPD: %f", ret);
            return ret * 5.5f;
        }
    };

    HOOK_DEFINE_TRAMPOLINE(MoveSpeed) {
        // @ float CPlayerGetMoveSpeedForStickInput(ActorCommonData *tACDPlayer, Player *tPlayer, SNO snoPower, float flMinClientWalkSpeed)
        static auto Callback(ActorCommonData *tACDPlayer, Player *tPlayer, SNO snoPower, float flMinClientWalkSpeed) -> float {
            auto MoveSpeed = Orig(tACDPlayer, tPlayer, snoPower, flMinClientWalkSpeed);
            if (!global_config.rare_cheats.active)
                return MoveSpeed;
            const auto mult = static_cast<float>(global_config.rare_cheats.move_speed);
            if (mult <= 0.0f)
                return MoveSpeed;
            return MoveSpeed * mult;
        }
    };

    HOOK_DEFINE_TRAMPOLINE(EquipAny) {
        //   @ GameError ACDInventoryItemAllowedInSlot(const ActorCommonData *tACDItem, const InventoryLocation *tInvLoc, const BOOL fSkipRequirements, const BOOL fSwapping)
        static auto Callback(const ActorCommonData *tACDItem, const InventoryLocation *tInvLoc, const BOOL fSkipRequirements, const BOOL fSwapping) -> GameError {
            auto rGameError = Orig(tACDItem, tInvLoc, fSkipRequirements, fSwapping);
            if (!global_config.rare_cheats.active || !global_config.rare_cheats.equip_any_slot)
                return rGameError;
            return GAMEERROR_NONE;
        }
    };

    void AugmentSpecifier(LootSpecifier *tSpecifier) {
        if (global_config.loot_modifiers.ForcedILevel > 0) {
            tSpecifier->tLooteeParams.nForcedILevel = global_config.loot_modifiers.ForcedILevel;
        }
        if (global_config.loot_modifiers.TieredLootRunLevel > 0) {
            tSpecifier->tLooteeParams.nTieredLootRunLevel = global_config.loot_modifiers.TieredLootRunLevel;
        }
        tSpecifier->tLooteeParams.bDisableAncientDrops       = static_cast<BOOL>(global_config.loot_modifiers.DisableAncientDrops);
        tSpecifier->tLooteeParams.bDisablePrimalAncientDrops = static_cast<BOOL>(global_config.loot_modifiers.DisablePrimalAncientDrops);
        tSpecifier->tLooteeParams.bDisableTormentDrops       = static_cast<BOOL>(global_config.loot_modifiers.DisableTormentDrops);
        tSpecifier->tLooteeParams.bDisableTormentCheck       = static_cast<BOOL>(global_config.loot_modifiers.DisableTormentCheck);
        tSpecifier->tLooteeParams.bSuppressGiftGeneration    = static_cast<BOOL>(global_config.loot_modifiers.SuppressGiftGeneration);
        tSpecifier->eAncientRank                             = global_config.loot_modifiers.AncientRankValue;
    }

    HOOK_DEFINE_REPLACE(ForceAncient) {
        static void Callback(LootSpecifier *tSpecifier, const ACDID idACDLooter) {
            AugmentSpecifier(tSpecifier);
            if (ActorCommonData *ptACDPlayer = ACDTryToGet(idACDLooter); ptACDPlayer) {
                FastAttribKey tKey;
                tKey.nValue = RECEIVED_SEASONAL_LEGENDARY;
                ACD_AttributesSetInt(ptACDPlayer, tKey, 0);
                // tKey.nValue = TARGETED_LEGENDARY_CHANCE;
                // ACD_AttributesSetInt(ptACDPlayer, tKey, 100000);
                // FastAttribKey flKey;
                // flKey.nValue = LEGENDARY_FIND_COMMUNITY_BUFF;
                // ACD_AttributesSetFloat(ptACDPlayer, flKey, 100.0f);
            }
        }
    };

    HOOK_DEFINE_REPLACE(Return_True) {
        static auto Callback() -> BOOL { return 1; }
    };

    HOOK_DEFINE_INLINE(FloatingDmgHook) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            // BB5E8
            auto *DmgColor = reinterpret_cast<RGBAColor *>(ctx->X[3]);
            *DmgColor      = crgbaRed;
        }
    };

    HOOK_DEFINE_INLINE(VarResHook) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto *ptVarRWindow = reinterpret_cast<VariableResRWindowData *>(ctx->X[19]);
            ctx->X[1]          = reinterpret_cast<uintptr_t>(&g_szVariableResString);
            ctx->X[2]          = g_ptGfxData->workerData[0].dwRTHeight;
            ctx->X[3]          = static_cast<uint32>(ptVarRWindow->flPercent * g_ptGfxData->tCurrentMode.dwHeight);

            // if (global_config.overlays.active && global_config.overlays.var_res_label)
            //     AppDrawFlagSet(APP_DRAW_VARIABLE_RES_DEBUG_BIT, 1);
            // if (global_config.overlays.active && global_config.overlays.fps_label)
            //     AppDrawFlagSet(APP_DRAW_FPS_BIT, 1);

            // [[maybe_unused]] auto dwHeight     = g_ptGfxData->tCurrentMode.dwHeight;
            // [[maybe_unused]] auto dwUIHeight   = g_ptGfxData->tCurrentMode.dwUIOptHeight;
            // [[maybe_unused]] auto rtHeight     = g_ptGfxData->workerData[0].dwRTHeight;
            // [[maybe_unused]] auto flResScale   = g_ptMainRWindow->m_ptVariableResRWindowData->flPercent;
            // ptVarRWindow->flPercent = (ptVarRWindow->flPercent * rtHeight);
            // ptVarRWindow->dwAvgHistoryUs = dwHeight;
            // PRINT("VarResHook: UIHeight=%u, RTHeight=%u, Height=%u, Scale=%.3f", dwUIHeight, rtHeight, dwHeight, flResScale)
            // PRINT("VarResHook: RTHeight=%u, RTWidth=%u, flRTHeight=%.3f, flRTWidth=%.3f, flRTInvHeight=%.3f, flRTInvWidth=%.3f", g_ptGfxData->workerData[0].dwRTHeight, g_ptGfxData->workerData[0].dwRTWidth, g_ptGfxData->workerData[0].flRTHeight, g_ptGfxData->workerData[0].flRTWidth, g_ptGfxData->workerData[0].flRTInvHeight, g_ptGfxData->workerData[0].flRTInvWidth)
            // ptVarRWindow->flMinPercent   = 0.75f;
            // ptVarRWindow->flMaxPercent   = 1.25f;
            // ptVarRWindow->dwAvgHistoryUs = dwHeight;
            // g_ptGfxData->tCurrentMode.dwHeight     = static_cast<uint32>(1080);
            // g_ptGfxData->tCurrentMode.dwWidth      = static_cast<uint32>(1920);
            // g_ptGfxData->tCurrentMode.dwUIOptWidth = static_cast<uint32>(1080);
            // g_ptGfxData->tCurrentMode.dwUIOptWidth = static_cast<uint32>(1920);
            // PRINT_EXPR("%f", ptVarRWindow->flMinPercent)
            // PRINT_EXPR("%f", ptVarRWindow->flMaxPercent)
            // ptVarRWindow->flMinPercent = 0.15f;
            // 4096 =  3.8209f; // 2160 = 2.015625f; // 1440 = 1.34375f; // 1080 = 1.0075f;
            // ptVarRWindow->flMaxPercent = 1.0f;
            // return;
            // PRINT_EXPR("%i", ptVarRWindow->dwMinUs)
            // PRINT_EXPR("%i", ptVarRWindow->dwMaxUs)
            // PRINT_EXPR("%i", ptVarRWindow->dwAdjustCooldown)
            // PRINT_EXPR("%f", ptVarRWindow->flPercentIncr)
        };
    };

    HOOK_DEFINE_INLINE(FontStringGetRenderedSizeHook) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto *tProps      = *reinterpret_cast<TextRenderProperties **>(&ctx->X[0]);
            tProps->nFontSize = 19;
            tProps->flScale   = 1.5f;
        };
    };

    HOOK_DEFINE_INLINE(FontStringDrawHook) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            return;
            ++g_drawCount;

            auto *tProps = *reinterpret_cast<TextRenderProperties **>(&ctx->X[0]);
            // tProps->nFontSize           = 19;
            // tProps->flScale             = 1.5f;
            // tProps->fSnapToPixelCenterX = 1;
            // tProps->fSnapToPixelCenterY = 1;
            // tProps->flAdvanceScalar = 1.25f;

            // return;
            auto *vecPointUIC = *reinterpret_cast<Vector2D **>(&ctx->X[2]);
            if (g_drawCount < 2) {
                if (vecPointUIC != nullptr) {
                    PRINT_EXPR("%f %f", vecPointUIC->x, vecPointUIC->y);
                    PRINT_EXPR("%f %f %f", tProps->flStereoDepth, tProps->flAdvanceScalar, tProps->flScale);
                    PRINT_EXPR("%i %i", tProps->nFontSize, tProps->snoFont);
                    PRINT_EXPR("%i %i", tProps->fSnapToPixelCenterX, tProps->fSnapToPixelCenterY);
                    // vecPointUIC->x -= 2.0f;
                    // vecPointUIC->y += 2.0f;
                    // PRINT_EXPR("%f %f", vecPointUIC->x, vecPointUIC->y)
                    // PRINT_EXPR("%i %i", tProps->nFontSize, tProps->snoFont)
                }
            }

            // vecPointUIC->x = 0.75 * vecPointUIC->x;  // 300.666f;
            // vecPointUIC->y += 2.0f;
        };
    };

    HOOK_DEFINE_TRAMPOLINE(Enum_GB_Types) {
        static void Callback(GameBalanceType eType, GBHandleList *listResults) {
            Orig(eType, listResults);

            if (global_config.overlays.active && global_config.overlays.var_res_label)
                AppDrawFlagSet(APP_DRAW_VARIABLE_RES_DEBUG_BIT, 1);
            if (global_config.overlays.active && global_config.overlays.fps_label)
                AppDrawFlagSet(APP_DRAW_FPS_BIT, 1);
            if (global_config.overlays.ddm_labels)
                g_tAppGlobals.eDebugDisplayMode = DDM_FPS_SIMPLE;
            if (global_config.defaults_only || !global_config.debug.active || !global_config.debug.enable_debug_flags) {
                return;
            }

            // PRINT_EXPR("uint: %s", XVarUint32_ToString(&g_varSeasonNum).m_elements)
            // PRINT_EXPR("uint: %s", XVarUint32_ToString(&g_varSeasonState).m_elements)

            // auto pszFileData = std::make_shared<std::string>(c_szSeasonSwap);
            // *pszFileData = testingok;
            // OnSeasonsFileRetrieved(nullptr, 0, &pszFileData);
            // XVarUint32_Set(&g_varSeasonNum, 30, 3u);
            // XVarUint32_Set(&g_varSeasonState, 1, 3u);
            // PRINT_EXPR("uint-post: %s", XVarUint32_ToString(&g_varSeasonNum).m_elements)
            // PRINT_EXPR("uint-post: %s", XVarUint32_ToString(&g_varSeasonState).m_elements)
            // XVarBool_Set(&g_varOnlineServicePTR, true, 3u);
            // XVarBool_Set(&g_varLocalLoggingEnable, true, 3u);
            // XVarBool_Set(&g_varChallengeRiftEnabled, true, 3u);
            // XVarBool_Set(&g_varExperimentalScheduling, true, 3u);
            // g_tAppGlobals->eDebugDisplayMode = DDM_CONVERSATIONS // DDM_SOUND_STATUS // DDM_FPS_SIMPLE; // DDM_FPS_QA // DDM_FPS_PROGRAMMER
            // g_tAppGlobals.eDebugDisplayMode = DDM_LOOT_REST_BONUS;
            g_tAppGlobals.eDebugFadeMode  = DFM_FADE_NEVER;
            g_tAppGlobals.eDebugSoundMode = DSM_FULL;
            // g_tAppGlobals.flDebugDrawRadius = 0.4f;
            // g_tAppGlobals.eStringDebugMode  = STRINGDEBUG_LABEL;
            // AppDrawFlagSet(APP_DRAW_VARIABLE_RES_DEBUG_BIT, 1);
            // AppDrawFlagSet(APP_DRAW_FPS_BIT, 1);
            AppDrawFlagSet(APP_DRAW_CONTROLLER_AUTO_TARGETING, 1);
            AppDrawFlagSet(APP_DRAW_LOW_POLY_SHADOWS_BIT, 1);
            AppDrawFlagSet(APP_DRAW_NO_ACTOR_SHADOWS_BIT, 1);
            AppDrawFlagSet(APP_DRAW_SHOW_BUFFS_BIT, 0);
            // AppDrawFlagSet(APP_DRAW_NO_VARIABLE_RES_BIT, 1);
            // AppDrawFlagSet(APP_DRAW_SSAO_DEBUG_BIT, 1);
            AppDrawFlagSet(APP_DRAW_BONES_BIT, 1);
            // AppDrawFlagSet(APP_DRAW_DRLG_INFO_BIT, 1);
            AppDrawFlagSet(APP_DRAW_FULL_BRIGHT_BIT, 1);
            // AppDrawFlagSet(APP_DRAW_DISABLE_LOS_BIT, 1);
            // AppDrawFlagSet(APP_DRAW_NO_CONSOLE_ICONS_BIT, 1);
            AppDrawFlagSet(APP_DRAW_TEXTURE_MEMORY_BIT, 1);
            AppDrawFlagSet(APP_DRAW_HWCLASS_UI_OUTLINES_BIT, 1);
            AppDrawFlagSet(APP_DRAW_HWCLASS_DISTORTIONS_BIT, 1);
            // AppGlobalFlagDisable(-1, 0u);
            g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_ALL_HITS_CRIT_BIT);
            // g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_FLYTHROUGH_BIT);
            g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_INSTANT_RESURRECTION_BIT);
            // g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_FORCE_SPAWN_ALL_GIZMO_LOCATIONS);
            // g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_DISPLAY_ALL_SKILLS);
            // g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_DISPLAY_ITEM_AFFIXES);
            // g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_DISPLAY_ITEM_ATTRIBUTES);
            g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_ALWAYS_PLAY_GETHIT_BIT);
            g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_DONT_IDENTIFY_RARES_BIT);
            g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_ALWAYS_PLAY_KNOCKBACK_BIT);
            g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_ALL_HITS_OVERKILL_BIT);
            g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_ALL_HITS_CRUSHING_BLOW_BIT);
            g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_DONT_THROTTLE_FLOATING_NUMBERS);
            // g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_DISPLAY_ITEM_TARGETED_CLASS);
            // g_tAppGlobals.dwDebugFlags3 |= (1 << APP_GLOBAL3_ANCIENT_ROLLS_WILL_ALWAYS_SUCCEED);
            // g_tAppGlobals.dwDebugFlags3 |= (1 << APP_GLOBAL3_ANCIENTS_ALWAYS_ROLL_PRIMAL);
            // g_tAppGlobals.dwDebugFlags3 |= (1 << APP_GLOBAL3_LOW_HEALTH_SCREEN_GLOW_DISABLED);
            g_tAppGlobals.dwDebugFlags3 |= (1 << APP_GLOBAL3_CONSOLE_COLLECTORS_EDITION);

            // g_tAppGlobals.dwDebugFlags3 = 0xFFFFFFFF;
            // g_tAppGlobals.dwDebugFlags3 ^= ~(1 << APP_GLOBAL3_ANCIENT_ROLLS_WILL_ALWAYS_SUCCEED);
            // AppGlobalFlagDisable(1, APP_GLOBAL_NET_COLLECT_DATA_BIT);
            // AppGlobalFlagDisable(1, APP_GLOBAL_AI_DISABLED_BIT);
            // AppGlobalFlagDisable(1, APP_GLOBAL_AI_BLIND_BIT);
            // AppGlobalFlagDisable(1, APP_GLOBAL_AI_BLIND_TO_PLAYERS_BIT);
            // AppGlobalFlagDisable(1, APP_GLOBAL_FORCE_WALK_BIT);
            // AppGlobalFlagDisable(1, APP_GLOBAL_TRACE_STAT_BIT);

            // AppGlobalFlagDisable(2, APP_GLOBAL2_DONT_IDENTIFY_RARES_BIT);
            // AppGlobalFlagDisable(2, APP_GLOBAL2_USE_CONSOLE_OBSERVER_BIT);

            // AppGlobalFlagDisable(3, APP_GLOBAL3_PLAYER_MUTE_REQUESTED_BIT);

            if (eType == GB_ITEMS) {
                // int counter = 0;
                // PRINT_EXPR("%d", g_tAppGlobals.ptItemAppGlobals->nItems);
                // PRINT_EXPR("%d", g_tAppGlobals.ptItemAppGlobals->tRootItemType.gbid)
                // if (g_tAppGlobals.ptItemAppGlobals->nItems > 0) {

                //     PRINT_EXPR("%i", GetItemPtr(-1164887190)->gbid)
                //     PRINT_EXPR("%i", GetItemPtr(-1164887190)->nSeasonRequiredToDrop)
                //     PRINT_EXPR("%i", GetItemPtr(-1164887190)->ptItemType->nLootLevelRange)
                //     PRINT_EXPR("%i", GetItemPtr(-1164887190)->ptItemType->ptParentType->nLootLevelRange)
                //     PRINT_EXPR("%s", (LPCSTR)GBIDToStringAll(&GetItemPtr(-1164887190)->gbid))
                //     PRINT_EXPR("%s", (LPCSTR)GBIDToStringAll(&GetItemPtr(-1164887190)->ptItemType->gbid))
                //     PRINT_EXPR("%s", (LPCSTR)GBIDToStringAll(&GetItemPtr(-1164887190)->ptItemType->ptParentType->gbid))
                //     PRINT_EXPR("0x%x", *GetItemPtr(-1164887190)->pdwLabelMask)
                //     PRINT_EXPR("0x%x", *GetItemPtr(-1164887190)->ptItemType->pdwLabelMask)
                //     PRINT_EXPR("0x%x", *GetItemPtr(-1164887190)->ptItemType->ptParentType->pdwLabelMask)
                // }
            }
        }
    };

    HOOK_DEFINE_REPLACE(HostArgC) {
        static auto Callback() -> u64 { return (*(&g_cmdArgs + 1) - g_cmdArgs); }
    };

    HOOK_DEFINE_REPLACE(HostArgV) {
        static auto Callback() -> const char ** { return g_cmdArgs; }
    };

    HOOK_DEFINE_TRAMPOLINE(SpoofDocked) {
        static auto Callback() -> nn::oe::OperationMode {
            if (!global_config.resolution_hack.spoof_docked)
                return Orig();
            return nn::oe::OperationMode_Docked;
        }
    };

    HOOK_DEFINE_TRAMPOLINE(SpoofPerfMode) {
        static auto Callback() -> nn::oe::PerformanceMode {
            if (!global_config.resolution_hack.spoof_docked)
                return Orig();
            return nn::oe::PerformanceMode_Boost;
        }
    };

    HOOK_DEFINE_TRAMPOLINE(TryPopNotificationMessageHook) {
        static auto Callback(u32 *pOutMessage) -> bool {
            const bool ret = Orig(pOutMessage);
            if (ret && pOutMessage != nullptr) {
                const auto msg = static_cast<u32>(*pOutMessage);
                PRINT("TryPopNotificationMessage: %u (0x%x)", msg, msg)
                return ret;
            }
            if (g_pending_dock_notify && pOutMessage != nullptr) {
                g_pending_dock_notify = false;
                *pOutMessage          = 0x1Eu;
                PRINT_LINE("TryPopNotificationMessage: forced 0x1E");
                return true;
            }
            if (g_pending_perf_notify && pOutMessage != nullptr) {
                g_pending_perf_notify = false;
                *pOutMessage          = 0x1Fu;
                PRINT_LINE("TryPopNotificationMessage: forced 0x1F");
                return true;
            }
            return ret;
        }
    };

    inline void SetupUtilityHooks() {
        Enum_GB_Types::
            InstallAtFuncPtr(GBEnumerate);  // A relatively consistent call to refresh without spamming every frame

        if (global_config.rare_cheats.active && global_config.rare_cheats.equip_any_slot) {
            EquipAny::
                InstallAtFuncPtr(ACDInventoryItemAllowedInSlot);
        }

        CmdLineParse::
            InstallAtFuncPtr(cmd_line_parse);
        HostArgC::
            InstallAtFuncPtr(nn::os::GetHostArgc);
        HostArgV::
            InstallAtFuncPtr(nn::os::GetHostArgv);

        if (global_config.overlays.active && global_config.overlays.var_res_label) {
            VarResHook::
                InstallAtSymbol("sym_var_res_label");
        }
        if (global_config.loot_modifiers.active) {
            ForceAncient::
                InstallAtFuncPtr(LootRollForAncientLegendary);
        }
        if (global_config.rare_cheats.active && global_config.rare_cheats.move_speed != 1.0) {
            MoveSpeed::
                InstallAtFuncPtr(move_speed);
        }
        if (global_config.rare_cheats.active && global_config.rare_cheats.attack_speed != 1.0) {
            AttackSpeed::
                InstallAtFuncPtr(attack_speed);
        }
        if (global_config.rare_cheats.active && global_config.rare_cheats.floating_damage_color) {
            FloatingDmgHook::
                InstallAtSymbol("sym_floating_dmg");
        }
        FontStringGetRenderedSizeHook::
            InstallAtSymbol("sym_font_string_get_rendered_size");
        if (global_config.debug.active) {
            FontStringDrawHook::
                InstallAtSymbol("sym_font_string_draw_03ff50");
            FontStringDrawHook::
                InstallAtSymbol("sym_font_string_draw_03e5b0");
            FontStringDrawHook::
                InstallAtSymbol("sym_font_string_draw_0010c8");
        }

        SpoofPerfMode::InstallAtFuncPtr(nn::oe::GetPerformanceMode);
        SpoofDocked::InstallAtFuncPtr(nn::oe::GetOperationMode);
        TryPopNotificationMessageHook::InstallAtFuncPtr(nn::oe::TryPopNotificationMessage);
    }

}  // namespace d3
