#pragma once

#include "d3/_util.hpp"
#include "../config.hpp"

namespace d3 {

    constinit const char c_szHackVerWatermark[] = D3HACK_VER D3HACK_BUILD;
    constinit const char c_szHackVerAutosave[]  = CRLF D3HACK_FULLWWW CRLF CRLF CRLF;
    constinit const char c_szHackVerStart[]     = CRLF D3HACK_FULLVER CRLF CRLF CRLF;

    constinit const char      c_szTraceStat[]         = "sd:/config/d3hack-nx/debug.txt";
    constinit const char      c_szVariableResString[] = "Output %4dp (Scaled %4dp)";
    constinit const Signature c_tSignature {"   " D3HACK_VER CRLF D3HACK_WEB CRLF " ", "Diablo III v", 2, 7, 6};
    const RGBAColor           c_rgbaText       = crgbaUIGold;
    const RGBAColor           c_rgbaDropShadow = crgbaWatermarkShadow;

    namespace armv8 = exl::armv8;
    namespace patch = exl::patch;
    using namespace exl::armv8::reg;
    using namespace exl::armv8::inst;

    using dword = std::array<std::byte, 0x4>;

    inline uintptr_t PatchTable(const char *name) { return GameOffsetFromTable(name) - exl::util::modules::GetTargetStart(); }

    void MakeAdrlPatch(const int32 main_addr, uintptr_t adrp_target, const exl::armv8::reg::Register register_target) {
        auto      jest       = patch::RandomAccessPatcher();
        uintptr_t AdrpOffset = main_addr;
        auto      diff       = Adrp::GetDifference(GameOffset(AdrpOffset), adrp_target);
        jest.Patch<Adrp>(AdrpOffset, register_target, diff.m_Page);
        jest.Patch<AddImmediate>(AdrpOffset + 4, register_target, register_target, diff.m_Offset);
    }

    /* String swap and formatting for BuildLockerDrawWatermark() */
    void PatchBuildlocker() {
        if (!(global_config.overlays.active && global_config.overlays.buildlocker_watermark))
            return;
        auto jest = patch::RandomAccessPatcher();
        /* Spoof the existence of a build signature so we can use the debug watermark (tSignature) */
        jest.Patch<Nop>(PatchTable("patch_buildlocker_01_nop"));
        jest.Patch<Movz>(PatchTable("patch_buildlocker_02_movz"), W20, (8 + GLOBALSNO_FONT_SCRIPT));
        MakeAdrlPatch(PatchTable("patch_buildlocker_03_adrl"), reinterpret_cast<uintptr_t>(&c_tSignature), X2);
        MakeAdrlPatch(PatchTable("patch_buildlocker_04_adrl"), reinterpret_cast<uintptr_t>(&crgbaTan), X19);
        MakeAdrlPatch(PatchTable("patch_buildlocker_05_adrl"), reinterpret_cast<uintptr_t>(&c_szHackVerWatermark), X1);
        auto szBuildLockerFormat = reinterpret_cast<std::array<char, 21> *>(jest.RwFromAddr(PatchTable("data_build_locker_format_rw")));
        *szBuildLockerFormat     = std::to_array("%s%s%d.%d.%d\0\0\0\0\0\0\0\0");
        jest.Patch<Movz>(PatchTable("patch_buildlocker_06_movz"), W21, 0x13);  // RENDERLAYER_UI_OVERLAY

        /* 0x10CC - lower right */

        /* 0x1114 - upper left */
        //      0x106C 09 D0 35 1E                   FMOV            S9, #-15.0
        //      0x10D4 0A D0 25 1E                   FMOV            S10, #15.0
        //      0x10D8 21 28 2A 1E                   FADD            S1, S1, S10 <- Y-axis from top
        //      0x10FC 00 28 29 1E                   FADD            S0, S0, S9  <- X-axis from left
        jest.Patch<dword>(PatchTable("patch_buildlocker_07_bytes"), make_bytes(0x00, 0x28, 0x2A, 0x1E));  // S0, +15.0 (S10) instead of -15.0

        /* 0x1160 - Lower left */
        //      0x1120 21 38 22 1E                   FSUB            S1, S1, S2
        //      0x1128 00 28 29 1E                   FADD            S0, S0, S9
        //      0x1150 21 28 29 1E                   FADD            S1, S1, S9
        jest.Patch<dword>(PatchTable("patch_buildlocker_08_bytes"), make_bytes(0x00, 0x28, 0x2A, 0x1E));  // S0, +15.0 (S10) instead of -15.0

        /* 0x11AC - upper right */

        /* 0x1224 - center screen */
        jest.Patch<Nop>(PatchTable("patch_buildlocker_09_nop"));

        /* 0x12F8 - primary, lower middle */
        //      0x11C0 0A 10 2C 1E                   FMOV            S10, #0.5 <- center X axis on screen
        // jest.Patch<dword>(PatchTable("patch_buildlocker_10_bytes"), make_bytes(0x0A, 0x10, 0x2E, 0x1E));  // #1.0, aka shift right
    }

    /* String swap and formatting for APP_DRAW_VARIABLE_RES_DEBUG_BIT */
    void PatchVarResLabel() {
        auto jest = patch::RandomAccessPatcher();
        jest.Patch<Movz>(PatchTable("patch_var_res_label_01_movz"), W0, (8 + GLOBALSNO_FONT_EXOCETLIGHT));
        // 3CC78    movz x8, #0x42c8, lsl #16   --> 100.0f
        jest.Patch<Movz>(PatchTable("patch_var_res_label_02_movz"), X8, 0x4288, ShiftValue_16);  // X-axis: 68.0f
        // 3CC7C    movk x8, #0x4316, lsl #48   --> 150.0f
        jest.Patch<Movk>(PatchTable("patch_var_res_label_03_movk"), X8, 0x4080, ShiftValue_48);  // Y-axis: 4.0f

        // MakeAdrlPatch(PatchTable("patch_var_res_label_04_adrl"), reinterpret_cast<uintptr_t>(&c_szVariableResString), X1);
        // AppDrawFlagSet(APP_DRAW_VARIABLE_RES_DEBUG_BIT, 1);     // Must be run each frame in another hook
    }

    /* String swap and formatting for APP_DRAW_FPS_BIT */
    void PatchReleaseFPSLabel() {
        auto jest = patch::RandomAccessPatcher();
        jest.Patch<Movz>(PatchTable("patch_release_fps_label_01_movz"), W0, (8 + GLOBALSNO_FONT_EXOCETLIGHT));
        auto szReleaseFPSFormat = reinterpret_cast<std::array<char, 10> *>(jest.RwFromAddr(PatchTable("data_release_fps_format_rw")));
        auto nReleaseFPSPosX    = reinterpret_cast<float *>(jest.RoFromAddr(PatchTable("data_release_fps_pos_x_ro")));
        auto nReleaseFPSPosY    = reinterpret_cast<float *>(jest.RoFromAddr(PatchTable("data_release_fps_pos_y_ro")));
        *szReleaseFPSFormat     = std::to_array("FPS: %.0f");  // :.ascii "%3.1f FPS"
        *nReleaseFPSPosX        = -77.7;                       // nReleaseFPSPosX:.float -180.0
        *nReleaseFPSPosY        = 500.0;                       // nReleaseFPSPosY:.float 290.0

        /* Skip the frame timer check/color setter (avoid flashing text colors) */
        jest.Patch<Nop>(PatchTable("patch_release_fps_label_02_nop"));
        MakeAdrlPatch(PatchTable("patch_release_fps_label_03_adrl"), reinterpret_cast<uintptr_t>(&crgbaWhite), X8);

        /* Skip rendering all extra FPS data labels */
        jest.Patch<Nop>(PatchTable("patch_release_fps_label_04_nop"));
        jest.Patch<Nop>(PatchTable("patch_release_fps_label_05_nop"));
        jest.Patch<Nop>(PatchTable("patch_release_fps_label_06_nop"));
        jest.Patch<Nop>(PatchTable("patch_release_fps_label_07_nop"));
        jest.Patch<Nop>(PatchTable("patch_release_fps_label_08_nop"));
        jest.Patch<Nop>(PatchTable("patch_release_fps_label_09_nop"));
        jest.Patch<Nop>(PatchTable("patch_release_fps_label_10_nop"));

        // AppDrawFlagSet(APP_DRAW_FPS_BIT, 1);  // Must be run each frame in another hook
    }

    void PatchDDMLabels() {
        auto jest = patch::RandomAccessPatcher();
        /* String swap and formatting for DDM_FPS_QA */
        jest.Patch<Movz>(PatchTable("patch_ddm_labels_01_movz"), W0, (8 + GLOBALSNO_FONT_SCRIPT));
        MakeAdrlPatch(PatchTable("patch_ddm_labels_02_adrl"), reinterpret_cast<uintptr_t>(&c_szHackVerWatermark), X1);
        MakeAdrlPatch(PatchTable("patch_ddm_labels_03_adrl"), reinterpret_cast<uintptr_t>(&c_rgbaText), X3);
        MakeAdrlPatch(PatchTable("patch_ddm_labels_04_adrl"), reinterpret_cast<uintptr_t>(&c_rgbaDropShadow), X4);
        jest.Patch<Movz>(PatchTable("patch_ddm_labels_05_movz"), W7, 0x13);  // RENDERLAYER_UI_OVERLAY

        /* String swap and formatting for DDM_FPS_SIMPLE */
        jest.Patch<Movz>(PatchTable("patch_ddm_labels_06_movz"), W0, (8 + GLOBALSNO_FONT_SCRIPT));
        MakeAdrlPatch(PatchTable("patch_ddm_labels_07_adrl"), reinterpret_cast<uintptr_t>(&c_szHackVerWatermark), X23);
        MakeAdrlPatch(PatchTable("patch_ddm_labels_08_adrl"), reinterpret_cast<uintptr_t>(&c_rgbaText), X3);
        MakeAdrlPatch(PatchTable("patch_ddm_labels_09_adrl"), reinterpret_cast<uintptr_t>(&c_rgbaDropShadow), X4);
        jest.Patch<Movz>(PatchTable("patch_ddm_labels_10_movz"), W7, 0x13);  // RENDERLAYER_UI_OVERLAY
    }

    void PatchResolutionTargets() {
        if (!(global_config.resolution_hack.active))
            return;

        auto jest = patch::RandomAccessPatcher();

        const u32 out_w = global_config.resolution_hack.OutputWidthPx();
        const u32 out_h = global_config.resolution_hack.OutputHeightPx();

        // Display mode pair (full). GFXNX64NVN::Init (0x0E7850).
        // 0x0E7858: MOV X8, #1600
        // 0x0E7860: MOVK X8, #900, LSL#32
        jest.Patch<Movz>(PatchTable("patch_resolution_targets_01_movz"), X8, out_w);
        jest.Patch<Movk>(PatchTable("patch_resolution_targets_02_movk"), X8, out_h, ShiftValue_32);

        // Fallback/base scale mode (same block).
        // 0x0E785C: MOV X9, #1280
        // 0x0E7864: MOVK X9, #720, LSL#32
        jest.Patch<Movz>(PatchTable("patch_resolution_targets_03_movz"), X9, out_w);
        jest.Patch<Movk>(PatchTable("patch_resolution_targets_04_movk"), X9, out_h, ShiftValue_32);

        if (global_config.resolution_hack.min_res_scale >= 100.0f) {
            /* VariableResRWindowData->flMinPercent = 0.70f - 0x03CBBC: MOVK X9, #0x3F33, LSL#48 (CGameVariableResInitializeForRWindow) */
            jest.Patch<Movk>(PatchTable("patch_resolution_targets_05_movk"), X9, 0x3F80, ShiftValue_48);  // 100% (1.0f) minimum resolution scale
        }

        /* VariableResRWindowData->flMaxPercent = 1.00f - 0x03CBCC: MOV W8, #0x3F800000 */
        // jest.Patch<Movz>(PatchTable("patch_resolution_targets_06_movz"), W8, 0x3FA0, ShiftValue_16);  // 125% (1.25f) maximum resolution scale (>1.0f breaks rendering)

        /* VariableResRWindowData->flPercentIncr = 0.05f - 0x03CBD4: MOVK X9, #0x3D4C, LSL#48 */
        jest.Patch<Movk>(PatchTable("patch_resolution_targets_07_movk"), X9, 0x3CF5, ShiftValue_48);  // 3% (0.03f) resolution adjust percentage

        /* FontDefinition::GetPointSizeData (0x276A60/0x276A6C). */
        /* 0x276A60: MOV W9, #8   | 0x276A6C: MOV W8, #0x10 */
        jest.Patch<Movz>(PatchTable("patch_resolution_targets_08_movz"), W9, 32);
        jest.Patch<Movz>(PatchTable("patch_resolution_targets_09_movz"), W8, 32);

        // ShellInitialize (0x6678B8): BL nn::oe::GetOperationMode
        // ShellEventLoop (0x667AE8): BL nn::oe::GetOperationMode
        jest.Patch<Movz>(PatchTable("patch_resolution_targets_10_movz"), W0, 0);  // 0 = "Docked"
        jest.Patch<Movz>(PatchTable("patch_resolution_targets_11_movz"), W0, 0);  // 0 = "Docked"
        // ShellInitialize (0x6678C8): BL nn::oe::GetPerformanceMode
        // ShellEventLoop (0x667B04): BL nn::oe::GetPerformanceMode
        jest.Patch<Movz>(PatchTable("patch_resolution_targets_12_movz"), W0, 1);  // 1 = "Boost"
        jest.Patch<Movz>(PatchTable("patch_resolution_targets_13_movz"), W0, 1);  // 1 = "Boost"
    }

    void PatchDynamicSeasonal() {
        if (global_config.seasons.active) {
            PRINT("Setting active Season to %d", global_config.seasons.current_season)
            XVarUint32_Set(&s_varSeasonNum, global_config.seasons.current_season, 3u);
            XVarUint32_Set(&s_varSeasonState, 1, 3u);

            auto jest = patch::RandomAccessPatcher();
            jest.Patch<Movz>(PatchTable("patch_dynamic_seasonal_03_movz"), W1, 1);  // always true for UIHeroCreate::Console::bSeasonConfirmedAvailable() (skip B.ne and always confirm)
            jest.Patch<Movz>(PatchTable("patch_dynamic_seasonal_04_movz"), W0, 1);  // always true for Console::Online::IsSeasonsInitialized()

            // Update season_created uses at runtime (UIOnlineActions::SetGameParamsForHero).
            // jest.Patch<Movz>(PatchTable("patch_dynamic_seasonal_05_movz"), W21, global_config.seasons.current_season);  // season_created = ...
            // Ignore mismatched hero season in UIHeroSelect (skip Rebirth/season prompt for old seasons).
            // jest.Patch<Nop>(PatchTable("patch_dynamic_seasonal_06_nop"));  // B.ne loc_358C74
            // Hide "Create Seasonal Hero" main menu option (item 9) when forcing season current_season.
            // ItemShouldBeVisible(case 9) calls IsSeasonsInitialized; forcing W0=0 makes it return false.
            // jest.Patch<Movz>(PatchTable("patch_dynamic_seasonal_07_movz"), W0, 0);
        }
    }

    void PatchDynamicEvents() {
        if (!global_config.events.active)
            return;

        constexpr s64 kBuffStartFallback = 1;
        constexpr s64 kBuffEndFallback   = 0x7FFFFFFFFFFFFFFFLL;

        auto *buff_start = *reinterpret_cast<s64 **>(GameOffsetFromTable("event_buff_start"));
        auto *buff_end   = *reinterpret_cast<s64 **>(GameOffsetFromTable("event_buff_end"));
        if (buff_start && buff_end && (!*buff_start || !*buff_end || *buff_end <= *buff_start)) {
            *buff_start = kBuffStartFallback;
            *buff_end   = kBuffEndFallback;
        }

        if (auto *egg_flag = *reinterpret_cast<u32 **>(GameOffsetFromTable("event_egg_flag")))
            *egg_flag = global_config.events.EasterEggWorldEnabled ? 1u : 0u;

        if (auto *igr = *reinterpret_cast<uintptr_t **>(GameOffsetFromTable("event_igr")))
            XVarBool_Set(igr, global_config.events.IgrEnabled, 3u);
        if (auto *ann = *reinterpret_cast<uintptr_t **>(GameOffsetFromTable("event_ann")))
            XVarBool_Set(ann, global_config.events.AnniversaryEnabled, 3u);
        if (auto *egg_xvar = *reinterpret_cast<uintptr_t **>(GameOffsetFromTable("event_egg_xvar")))
            XVarBool_Set(egg_xvar, global_config.events.EasterEggWorldEnabled, 3u);
        if (auto *event_enabled = *reinterpret_cast<uintptr_t **>(GameOffsetFromTable("event_enabled")))
            XVarBool_Set(event_enabled, true, 3u);
        if (auto *event_season_only = *reinterpret_cast<uintptr_t **>(GameOffsetFromTable("event_season_only")))
            XVarBool_Set(event_season_only, false, 3u);

        auto set_bool = [](uintptr_t *var, bool value) {
            if (var)
                XVarBool_Set(var, value, 3u);
        };

        set_bool(&s_varDoubleRiftKeystones, global_config.events.DoubleRiftKeystones);
        set_bool(&s_varDoubleBloodShards, global_config.events.DoubleBloodShards);
        set_bool(&s_varDoubleTreasureGoblins, global_config.events.DoubleTreasureGoblins);
        set_bool(&s_varDoubleBountyBags, global_config.events.DoubleBountyBags);
        set_bool(&s_varRoyalGrandeur, global_config.events.RoyalGrandeur);
        set_bool(&s_varLegacyOfNightmares, global_config.events.LegacyOfNightmares);
        set_bool(&s_varTriunesWill, global_config.events.TriunesWill);
        set_bool(&s_varPandemonium, global_config.events.Pandemonium);
        set_bool(&s_varKanaiPowers, global_config.events.KanaiPowers);
        set_bool(&s_varTrialsOfTempests, global_config.events.TrialsOfTempests);
        set_bool(&s_varShadowClones, global_config.events.ShadowClones);
        set_bool(&s_varFourthKanaisCubeSlot, global_config.events.FourthKanaisCubeSlot);
        set_bool(&s_varEtherealItems, global_config.events.EtherealItems);
        set_bool(&s_varSoulShards, global_config.events.SoulShards);
        set_bool(&s_varSwarmRifts, global_config.events.SwarmRifts);
        set_bool(&s_varSanctifiedItems, global_config.events.SanctifiedItems);
        set_bool(&s_varDarkAlchemy, global_config.events.DarkAlchemy);
        set_bool(&s_varNestingPortals, global_config.events.NestingPortals);
        set_bool(&s_varParagonCap, false);  // ParagonCap off by default
    }

    void PortCheatCodes() {
        if (!global_config.rare_cheats.active)
            return;
        auto jest = patch::RandomAccessPatcher();

        const auto &cheats = global_config.rare_cheats;
        /* Restore debug display of allocation errors */
        jest.Patch<Movz>(PatchTable("patch_cheat_alloc_errors_01_movz"), W8, 0);
        // jest.Patch<MovRegister>(PatchTable("patch_cheat_alloc_errors_02_bytes"), X0, SP);
        jest.Patch<dword>(PatchTable("patch_cheat_alloc_errors_02_bytes"), make_bytes(0xE0, 0x03, 0x00, 0x91));

        /* Drop any item (Staff of Herding, etc) */
        if (cheats.drop_anything)
            jest.Patch<Movn>(PatchTable("patch_cheat_drop_anything_01_movn"), W0, 0);
        // 04000000 00504C78 12800000

        /* 100% Legendary probability (Kadala/Kanai) */
        if (cheats.guaranteed_legendaries) {
            jest.Patch<Nop>(PatchTable("patch_cheat_guaranteed_legendaries_01_nop"));  // unlikely? shot in the dark
            jest.Patch<Nop>(PatchTable("patch_cheat_guaranteed_legendaries_02_nop"));
            jest.Patch<Branch>(PatchTable("patch_cheat_guaranteed_legendaries_03_branch"), 0x18C);
            jest.Patch<Nop>(PatchTable("patch_cheat_guaranteed_legendaries_04_nop"));
            jest.Patch<Branch>(PatchTable("patch_cheat_guaranteed_legendaries_05_branch"), 0x350);
        }
        // jest.Patch<dword>(PatchTable("patch_cheat_guaranteed_legendaries_04_nop"), make_bytes(0xD4, 0x00, 0x00, 0x14));
        // 04000000 0088F9E0 140000D4

        /* Instant town portal & Book of Cain */
        if (cheats.instant_portal)
            jest.Patch<Movz>(PatchTable("patch_cheat_instant_portal_01_movz"), W24, 0);
        // 04000000 009E3250 52800018

        /* Show testing art cosmetics */
        // jest.Patch<CmnImmediate>(PatchTable("patch_cheat_testing_cosmetics_01_cmn_imm"), W2, 0);
        // 04000000 004FFFE4 3100081F

        /* ►Primal ancient probability 100% */
        // jest.Patch<Branch>(PatchTable("patch_cheat_primal_01_branch"), 0x360);
        // jest.Patch<Movz>(PatchTable("patch_cheat_primal_02_movz"), W8, 2);
        // jest.Patch<dword>(PatchTable("patch_cheat_primal_03_bytes"), make_bytes(0x40, 0x21, 0x2A, 0x1E));
        // jest.Patch<dword>(PatchTable("patch_cheat_primal_04_bytes"), make_bytes(0x00, 0x20, 0x20, 0x1E));  // covered by W8, 2
        // jest.Patch<dword>(PatchTable("patch_cheat_primal_05_bytes"), make_bytes(0x1F, 0x20, 0x03, 0xD5));
        /*
        04000000 0088DF10 1E2A2140
        04000000 0088DFFC 1E202000
        04000000 0088DFE0 D503201F
        */

        /* ►No cooldown */
        if (cheats.no_cooldowns) {
            jest.Patch<dword>(PatchTable("patch_cheat_no_cooldowns_01_bytes"), make_bytes(0xE0, 0x03, 0x27, 0x1E));
            jest.Patch<dword>(PatchTable("patch_cheat_no_cooldowns_02_bytes"), make_bytes(0xE0, 0x03, 0x00, 0x2A));
            jest.Patch<dword>(PatchTable("patch_cheat_no_cooldowns_03_bytes"), make_bytes(0xE8, 0x03, 0x27, 0x1E));
            jest.Patch<dword>(PatchTable("patch_cheat_no_cooldowns_04_bytes"), make_bytes(0xE8, 0x03, 0x27, 0x1E));
        }
        /*
        04000000 007396D0 1E2703E0
        04000000 007F8960 2A0003E0
        04000000 009BC53C 1E2703E8
        04000000 009BC828 1E2703E8
        */

        /* ►Instantly identify items */
        if (cheats.instant_craft_actions)
            jest.Patch<Nop>(PatchTable("patch_cheat_instant_identify_01_nop"));
        // 04000000 0020636C D503201F

        /* ►Instantly craft items */
        if (cheats.instant_craft_actions)
            jest.Patch<AddImmediate>(PatchTable("patch_cheat_instant_craft_01_add_imm"), W8, W8, 5);
        // 04000000 0040F9B0 11001508

        /* ►Instantly enchant items */
        if (cheats.instant_craft_actions)
            jest.Patch<AddImmediate>(PatchTable("patch_cheat_instant_enchant_01_add_imm"), W8, W8, 1);
        // 04000000 001DCFA4 11000508

        /* ►Instantly craft Kanai's Cube items */
        if (cheats.instant_craft_actions) {
            jest.Patch<Movz>(PatchTable("patch_cheat_cube_instant_craft_01_movz"), W8, 0);
            jest.Patch<Movz>(PatchTable("patch_cheat_cube_instant_craft_02_movz"), W9, 0);
            jest.Patch<Movz>(PatchTable("patch_cheat_cube_instant_craft_03_movz"), W10, 0);
            jest.Patch<Movz>(PatchTable("patch_cheat_cube_instant_craft_04_movz"), W8, 0);
        }
        /*
        04000000 00236D20 52800008
        04000000 00236D24 52800009
        04000000 00236D28 5280000A
        04000000 00236D30 52800008
        */

        /* ►Kanai's Cube does not consume materials */
        if (cheats.cube_no_consume)
            jest.Patch<Ret>(PatchTable("patch_cheat_cube_no_consume_01_ret"));
        // 04000000 008874D0 D65F03C0

        /* ►Greater Rift Lv. 150 after clearing once */
        // jest.Patch<dword>(PatchTable("patch_cheat_gr150_01_bytes"), make_bytes(0xF5, 0x03, 0x00, 0x2A));
        // 04000000 004873E4 2A0003F5

        /* ►Legendary Gem Upgrade 100% */
        if (cheats.gem_upgrade_always)
            jest.Patch<dword>(PatchTable("patch_cheat_gem_upgrade_01_bytes"), make_bytes(0x08, 0x10, 0x2E, 0x1E));
        // 04000000 006A255C 1E2E1008*/

        /* ►Legendary Gem Upgrade Speed */
        if (cheats.gem_upgrade_speed)
            jest.Patch<dword>(PatchTable("patch_cheat_gem_speed_01_bytes"), make_bytes(0x02, 0x10, 0x2E, 0x1E));
        // 04000000 00349B98 1E2E1002

        /* ►Legendary Gem Lv. 150 after upgrading once */
        /*
        mov w22, w0
        movz w0, #0x96
        sub w23, w0, w22
        */
        // jest.Patch<Movk>(PatchTable("patch_cheat_gem_lvl150_01_bytes"), W22, W0);  // MOV LIKE THIS?
        if (cheats.gem_upgrade_lvl150) {
            jest.Patch<dword>(PatchTable("patch_cheat_gem_lvl150_01_bytes"), make_bytes(0xF6, 0x03, 0x00, 0x2A));
            jest.Patch<Movz>(PatchTable("patch_cheat_gem_lvl150_02_movz"), W0, 0x96);
            // jest.Patch<Sub>(PatchTable("patch_cheat_gem_lvl150_03_bytes"), W23, W0, W22);
            jest.Patch<dword>(PatchTable("patch_cheat_gem_lvl150_03_bytes"), make_bytes(0x17, 0x00, 0x16, 0x4B));
            jest.Patch<Ret>(PatchTable("patch_cheat_gem_lvl150_04_ret"));
            jest.Patch<dword>(PatchTable("patch_cheat_gem_lvl150_05_bytes"), make_bytes(0x89, 0x4D, 0x12, 0x94));
        }
        /*
        04000000 00C72F7C 2A0003F6
        04000000 00C72F80 528012C0
        04000000 00C72F84 4B160017
        04000000 00C72F88 D65F03C0
        04000000 007DF958 94124D89
        */

        // [►Equip Multiple Legendary Item]
        if (cheats.equip_multi_legendary)
            jest.Patch<Branch>(PatchTable("patch_cheat_multi_legendary_01_branch"), 0x5C);
        // 04000000 004FB9E4 14000017

        // [►Socket Any Gem To Any Slot]
        if (cheats.any_gem_any_slot) {
            jest.Patch<Movz>(PatchTable("patch_cheat_any_gem_any_slot_01_movz"), W25, 5);
            jest.Patch<Branch>(PatchTable("patch_cheat_any_gem_any_slot_02_branch"), 0xA0);
        }

        // [►Auto Pickup]
        if (cheats.auto_pickup)
            jest.Patch<Nop>(PatchTable("patch_cheat_auto_pickup_01_nop"));
        // 04000000 004F98DC D503201F
    }

    void PatchBase() {
        auto jest = patch::RandomAccessPatcher();

        PortCheatCodes();

        XVarBool_Set(&s_varLocalLoggingEnable, true, 3u);
        // XVarBool_Set(&s_varChallengeRiftEnabled, global_config.challenge_rifts.active, 3u);
        XVarBool_Set(&s_varOnlineServicePTR, global_config.seasons.spoof_ptr, 3u);
        XVarBool_Set(&s_varExperimentalScheduling, global_config.resolution_hack.exp_scheduler, 3u);
        jest.Patch<Movz>(PatchTable("patch_signin_01_movz"), W0, 1);  // always Console::GamerProfile::IsSignedInOnline
        jest.Patch<Ret>(PatchTable("patch_signin_02_ret"));           // ^ ret

        /* String swap for autosave screen */
        MakeAdrlPatch(PatchTable("patch_autosave_string_01_adrl"), reinterpret_cast<uintptr_t>(&c_szHackVerAutosave), X0);

        /* String swap for start screen */
        MakeAdrlPatch(PatchTable("patch_start_string_01_adrl"), reinterpret_cast<uintptr_t>(&c_szHackVerStart), X0);

        /* Enable local logging */
        jest.Patch<Movz>(PatchTable("patch_local_logging_01_movz"), W0, 1);

        // 0x00A2C614 - E5 0A 00 12
        // AND     W5, W23, #7
        // jest.Patch<exl::patch::inst::

        /* Pretend we are NOT in a mounted ROM */
        // jest.Patch<Movz>(PatchTable("patch_rom_mount_01_movz"), W1, 0);

        /* No timed wait for autosave warning */
        // jest.Patch<Nop>(PatchTable("patch_autosave_wait_01_nop"));

        /* Inside ItemSetIsBeingManipulated(): BeingManipulated Attrib = 0 */
        // jest.Patch<Branch>(PatchTable("patch_item_manipulated_01_branch"), 0x20);  // CBZ W1, loc_BF090

        if (global_config.debug.enable_crashes) {
            /* Don't try to find a slot to equip newly duped item */
            jest.Patch<Nop>(PatchTable("patch_dupe_noequip_01_nop"));  // BL ACDInventoryWhereCanThisGo()
            /* Stub functions that interfere with item duping */
            jest.Patch<Ret>(PatchTable("patch_dupe_stub_01_ret")); /* void GameSoftPause(BOOL bPause) */
            jest.Patch<Ret>(PatchTable("patch_dupe_stub_02_ret")); /* void ActorCommonData::SetAssignedHeroID(ActorCommonData *this, const Player *ptPlayer) */
            jest.Patch<Ret>(PatchTable("patch_dupe_stub_03_ret")); /* void ActorCommonData::SetAssignedHeroID(ActorCommonData *this, OnlineService::HeroId idHero) */
            jest.Patch<Ret>(PatchTable("patch_dupe_stub_04_ret")); /* void SGameSoftPause(BOOL bPause) */
        }

        /* Stub logging to RingBuffer */
        jest.Patch<Ret>(PatchTable("patch_log_ringbuffer_01_ret")); /* void FileOutputStream::LogToRingBuffer(int, char const*, char const*) */

        /* Stub net message tracing */
        jest.Patch<Ret>(PatchTable("patch_trace_message_01_ret")); /* void __fastcall sTraceMessage(const void *pMessage) */

        /* Fix path for stat tracing */
        MakeAdrlPatch(PatchTable("patch_trace_stat_path_01_adrl"), reinterpret_cast<uintptr_t>(&c_szTraceStat), X0);

        if (!global_config.seasons.allow_online) {
            // Hide "Connect to Diablo Servers" menu entry (main menu item 12).
            // jest.Patch<Branch>(PatchTable("patch_hideonline_01_branch"), -0x90);  // force false path in ItemShouldBeVisible

            /* Pause + Main menu: hide "Connect to Diablo Servers" list entry. */
            // 0x061620: STR X19, [SP,#-0x10+var_10]!
            // 0x061624: STP X29, X30, [SP,#0x10+var_s0]
            jest.Patch<Movz>(PatchTable("patch_hideonline_02_movz"), W0, 0);  // return false
            jest.Patch<Ret>(PatchTable("patch_hideonline_03_ret"));

            /* Main menu: hide Diablo Network status label in UIMainMenu::Console::OnUpdate. */
            jest.Patch<Movz>(PatchTable("patch_hideonline_04_movz"), W1, 0);  // off_1151298 visibility = 0 (connected path)
            jest.Patch<Movz>(PatchTable("patch_hideonline_05_movz"), W1, 0);  // off_1151290 visibility = 0 (disconnected path)
            // Hide the status text itself by calling SetVisible(0) on the text control instead of SetText.
            jest.Patch<dword>(PatchTable("patch_hideonline_06_bytes"), make_bytes(0x08, 0x29, 0x40, 0xF9));  // LDR X8, [X8,#0x50]
            jest.Patch<Movz>(PatchTable("patch_hideonline_07_movz"), W1, 0);

            /* Pause menu: hide Diablo Network status label in UIPause::Console::sOnWarningAccepted. */
            // 0x221324: MOV W1, #1
            // 0x221338: MOV W1, #1
            jest.Patch<Movz>(PatchTable("patch_hideonline_08_movz"), W1, 0);  // force SetVisible(0) for connected path
            jest.Patch<Movz>(PatchTable("patch_hideonline_09_movz"), W1, 0);  // force SetVisible(0) for disconnected path
            // Hide the status text itself by calling SetVisible(0) on the text control instead of SetText.
            jest.Patch<dword>(PatchTable("patch_hideonline_10_bytes"), make_bytes(0x08, 0x29, 0x40, 0xF9));  // LDR X8, [X8,#0x50]
            jest.Patch<Movz>(PatchTable("patch_hideonline_11_movz"), W1, 0);                                 // W1 = 0 (invisible)
        }

        /* Unlock all difficulties */
        // 0x5281B0: SXTW X8, W2
        // 0x5281B4: TBZ W3, #0, loc_5281D8
        if (global_config.rare_cheats.active && global_config.rare_cheats.unlock_all_difficulties) {
            jest.Patch<Movz>(PatchTable("patch_handicap_unlock_01_movz"), W0, 1);  // return true
            jest.Patch<Ret>(PatchTable("patch_handicap_unlock_02_ret"));
        }

        /* ItemCanDrop bypass */
        // jest.Patch<dword>(PatchTable("patch_itemcandrop_01_bytes"), make_bytes(0x10, 0x00, 0x00, 0x14));

        /* Increase the damage for ACTOR_EASYKILL_BIT */
        if (global_config.rare_cheats.active && global_config.rare_cheats.easy_kill_damage) {
            jest.Patch<Movz>(PatchTable("patch_easykill_01_movz"), W9, 0x7bc7);
            jest.Patch<Movk>(PatchTable("patch_easykill_02_movk"), W9, 0x5d94, ShiftValue_16);
        }
        // 5863 5FA9 = 1,000 T
        // 5d94 7bc7 = 1,337,420T
        // 60e8 0dae = 133,769,696T
        // 6070 D958 = 69,420,000 T <--- lol
        // 6291 07B5 = 1,337,666,688 T
        // 61B6 53FF = 420,420,000 T
        // 61B6 6F61 = 420,666,656 T <--
        // 6210 8F6F = 666,666,688 T <--
    }

}  // namespace d3
