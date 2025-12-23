#pragma once

#include "d3/_util.hpp"
#include "../config.hpp"

namespace d3 {

    constinit const char c_szHackVerWatermark[] = D3HACK_VER D3HACK_BUILD;
    constinit const char c_szHackVerAutosave[]  = CRLF D3HACK_FULLWWW CRLF CRLF CRLF;
    constinit const char c_szHackVerStart[]     = CRLF D3HACK_FULLVER CRLF CRLF CRLF;

    constinit const char      c_szTraceStat[]         = "scratch:/config/d3hack-nx/debug.txt";
    constinit const char      c_szVariableResString[] = "Resolution Multiplier: %4.2f | %4dp";
    constinit const Signature c_tSignature {
        "   " D3HACK_VER CRLF D3HACK_WEB CRLF " ", "Diablo III v", 2, 7, 6
    };
    const RGBAColor c_rgbaText       = crgbaUIGold;
    const RGBAColor c_rgbaDropShadow = crgbaWatermarkShadow;

    namespace armv8 = exl::armv8;
    namespace patch = exl::patch;
    using namespace exl::armv8::reg;
    using namespace exl::armv8::inst;

    using dword = std::array<std::byte, 0x4>;

    void MakeAdrlPatch(const int32 main_addr, uintptr_t adrp_target, const exl::armv8::reg::Register register_target) {
        auto      jest       = patch::RandomAccessPatcher();
        uintptr_t AdrpOffset = main_addr;
        auto      diff       = Adrp::GetDifference(
            GameOffset(AdrpOffset),
            adrp_target
        );
        jest.Patch<Adrp>(AdrpOffset, register_target, diff.m_Page);
        jest.Patch<AddImmediate>(AdrpOffset + 4, register_target, register_target, diff.m_Offset);
    }

    /* String swap and formatting for BuildLockerDrawWatermark() */
    void PatchBuildlocker() {
        if (!(global_config.overlays.active && global_config.overlays.buildlocker_watermark))
            return;
        auto jest = patch::RandomAccessPatcher();
        /* Spoof the existence of a build signature so we can use the debug watermark (tSignature) */
        jest.Patch<Nop>(0xF6C);
        jest.Patch<Movz>(0xFD4, W20, (8 + GLOBALSNO_FONT_SCRIPT));
        MakeAdrlPatch(0x000FA4, reinterpret_cast<uintptr_t>(&c_tSignature), X2);
        MakeAdrlPatch(0x001040, reinterpret_cast<uintptr_t>(&crgbaTan), X19);
        MakeAdrlPatch(0x001228, reinterpret_cast<uintptr_t>(&c_szHackVerWatermark), X1);
        auto szBuildLockerFormat = reinterpret_cast<std::array<char, 21> *>(jest.RwFromAddr(0xE6D9E4));
        *szBuildLockerFormat     = std::to_array("%s%s%d.%d.%d\0\0\0\0\0\0\0\0");
        jest.Patch<Movz>(0x1088, W21, 0x13);  // RENDERLAYER_UI_OVERLAY

        /* 0x10CC - lower right */

        /* 0x1114 - upper left */
        //      0x106C 09 D0 35 1E                   FMOV            S9, #-15.0
        //      0x10D4 0A D0 25 1E                   FMOV            S10, #15.0
        //      0x10D8 21 28 2A 1E                   FADD            S1, S1, S10 <- Y-axis from top
        //      0x10FC 00 28 29 1E                   FADD            S0, S0, S9  <- X-axis from left
        jest.Patch<dword>(0x10FC, make_bytes(0x00, 0x28, 0x2A, 0x1E));  // S0, +15.0 (S10) instead of -15.0

        /* 0x1160 - Lower left */
        //      0x1120 21 38 22 1E                   FSUB            S1, S1, S2
        //      0x1128 00 28 29 1E                   FADD            S0, S0, S9
        //      0x1150 21 28 29 1E                   FADD            S1, S1, S9
        jest.Patch<dword>(0x1128, make_bytes(0x00, 0x28, 0x2A, 0x1E));  // S0, +15.0 (S10) instead of -15.0

        /* 0x11AC - upper right */

        /* 0x1224 - center screen */
        jest.Patch<Nop>(0x1224);

        /* 0x12F8 - primary, lower middle */
        //      0x11C0 0A 10 2C 1E                   FMOV            S10, #0.5 <- center X axis on screen
        // jest.Patch<dword>(0x11C0, make_bytes(0x0A, 0x10, 0x2E, 0x1E));  // #1.0, aka shift right
    }

    /* String swap and formatting for APP_DRAW_VARIABLE_RES_DEBUG_BIT */
    void PatchVarResLabel() {
        if (!(global_config.overlays.active && global_config.overlays.var_res_label))
            return;
        if (!(global_config.rare_cheats.active && global_config.rare_cheats.draw_var_res))
            return;
        auto jest = patch::RandomAccessPatcher();
        jest.Patch<Movz>(0x03CC20, W0, (8 + GLOBALSNO_FONT_EXOCETLIGHT));
        MakeAdrlPatch(0x03CC68, reinterpret_cast<uintptr_t>(&c_szVariableResString), X1);
        // 3CC78    movz x8, #0x42c8, lsl #16   --> 100.0f
        jest.Patch<Movz>(0x03CC78, X8, 0x4288, ShiftValue_16);  // X-axis: 68.0f
        // 3CC7C    movk x8, #0x4316, lsl #48   --> 150.0f
        jest.Patch<Movk>(0x03CC7C, X8, 0x4080, ShiftValue_48);  // Y-axis: 4.0f
    }

    /* String swap and formatting for APP_DRAW_FPS_BIT */
    void PatchReleaseFPSLabel() {
        if (!(global_config.overlays.active && global_config.overlays.fps_label))
            return;
        if (!(global_config.rare_cheats.active && global_config.rare_cheats.draw_fps))
            return;
        auto jest = patch::RandomAccessPatcher();
        jest.Patch<Movz>(0x3F654, W0, (8 + GLOBALSNO_FONT_EXOCETLIGHT));
        auto szReleaseFPSFormat = reinterpret_cast<std::array<char, 10> *>(jest.RwFromAddr(0xE46144));
        auto nReleaseFPSPosX    = reinterpret_cast<float *>(jest.RoFromAddr(0x1098658));
        auto nReleaseFPSPosY    = reinterpret_cast<float *>(jest.RoFromAddr(0x109865C));
        *szReleaseFPSFormat     = std::to_array("FPS: %.0f");  // :.ascii "%3.1f FPS"
        *nReleaseFPSPosX        = -77.7;                       // nReleaseFPSPosX:.float -180.0
        *nReleaseFPSPosY        = 500.0;                       // nReleaseFPSPosY:.float 290.0

        /* Skip the frame timer check/color setter (avoid flashing text colors) */
        jest.Patch<Nop>(0x03F69C);
        MakeAdrlPatch(0x03F6A0, reinterpret_cast<uintptr_t>(&crgbaWhite), X8);

        /* Skip rendering all extra FPS data labels */
        jest.Patch<Nop>(0x03F840);
        jest.Patch<Nop>(0x03F8E4);
        jest.Patch<Nop>(0x03F9A8);
        jest.Patch<Nop>(0x03FA68);
        jest.Patch<Nop>(0x03FB80);
        jest.Patch<Nop>(0x03FC7C);
        jest.Patch<Nop>(0x03FD78);
    }

    void PatchDDMLabels() {
        if (!(global_config.overlays.active && global_config.overlays.ddm_labels))
            return;
        auto jest = patch::RandomAccessPatcher();
        /* String swap and formatting for DDM_FPS_QA */
        jest.Patch<Movz>(0x03E530, W0, (8 + GLOBALSNO_FONT_SCRIPT));
        MakeAdrlPatch(0x03E568, reinterpret_cast<uintptr_t>(&c_szHackVerWatermark), X1);
        MakeAdrlPatch(0x03E580, reinterpret_cast<uintptr_t>(&c_rgbaText), X3);
        MakeAdrlPatch(0x03E58C, reinterpret_cast<uintptr_t>(&c_rgbaDropShadow), X4);
        jest.Patch<Movz>(0x03E594, W7, 0x13);  // RENDERLAYER_UI_OVERLAY

        /* String swap and formatting for DDM_FPS_SIMPLE */
        jest.Patch<Movz>(0x03FE78, W0, (8 + GLOBALSNO_FONT_SCRIPT));
        MakeAdrlPatch(0x03FE3C, reinterpret_cast<uintptr_t>(&c_szHackVerWatermark), X23);
        MakeAdrlPatch(0x03FF18, reinterpret_cast<uintptr_t>(&c_rgbaText), X3);
        MakeAdrlPatch(0x03FF28, reinterpret_cast<uintptr_t>(&c_rgbaDropShadow), X4);
        jest.Patch<Movz>(0x03FF20, W7, 0x13);  // RENDERLAYER_UI_OVERLAY
    }

    void PatchResolutionTargets() {
        if (!(global_config.rare_cheats.active && global_config.rare_cheats.fhd_mode))
            return;
        auto jest = patch::RandomAccessPatcher();
        /* Patch both operation mode checks to 0 (docked) */
        jest.Patch<Movz>(0x6678B8, W0, 0);
        jest.Patch<Movz>(0x667AE8, W0, 0);

        /* Patch both performance mode checks to 1 (boost) */
        jest.Patch<Movz>(0x6678C8, W0, 1);
        jest.Patch<Movz>(0x667B04, W0, 1);

        /* 1080p (1905*1072 * 1.01) forced */
        jest.Patch<Movz>(0x0E7858, X8, 1905);
        jest.Patch<Movk>(0x0E7860, X8, 1072, ShiftValue_32);

        /* 900p (1600*900) fallback */
        jest.Patch<Movz>(0x0E785C, X9, 1600);
        jest.Patch<Movk>(0x0E7864, X9, 900, ShiftValue_32);

        /* VariableResRWindowData->flMinPercent = 0.70f -  MOVK     X9, #0x3F33,LSL#48 */
        jest.Patch<Movk>(0x03CBBC, X9, 0x3F40, ShiftValue_48);  // 75% (0.75f) minimum resolution scale

        /* VariableResRWindowData->flMaxPercent = 1.00f -  MOVZ     W8, #0x3F80,LSL#16 */
        jest.Patch<Movz>(0x03CBCC, W8, 0x3F81, ShiftValue_16);  // 101% (1.007f) maximum resolution scale

        /* VariableResRWindowData->flPercentIncr = 0.05f - MOVK     X9, #0x3D4C,LSL#48 */
        jest.Patch<Movk>(0x03CBD4, X9, 0x3CF5, ShiftValue_48);  // 3% (0.03f) resolution adjust percentage

        /* UI scaling fixes for 1080p */
        // EB0E68 00 00 34 44                   dword_EB0E68:.long 0x44340000 (720)
        // jest.Patch<uint32_t>(0xEB0E68, 0x443B8000);  // 0x443B8000 = 750.0
        // EB09E0 00 00 70 44                   dword_EB09E0:.long 0x44700000 (960)
        // jest.Patch<uint32_t>(0xEB09E0, 0x447A0000);  // 1000.0
    }

    void PatchForcedSeasonal() {
        if (!global_config.seasons.active)
            return;
        auto jest = patch::RandomAccessPatcher();

        // jest.Patch<Movz>(0x185F9C, W0, 0);  // always STORAGE_SUCCESS

        jest.Patch<Movz>(0x56B10, W0, 1);     // always Console::GamerProfile::IsSignedInOnline
        jest.Patch<Ret>(0x56B14);             // ^ ret

        jest.Patch<Ret>(0x65270);             // stub Console::Online::LobbyServiceInternal::OnSeasonsFileRetrieved() to override server data

        jest.Patch<Movz>(0x5FF0C, W0, 1);     // always true for Console::Online::IsSeasonsInitialized()
        jest.Patch<Movz>(0x72ED98, W3, global_config.seasons.number);   // Num (of Season)
        jest.Patch<Movz>(0x72EDD0, W3, 1);    // State (of Season)
        jest.Patch<Nop>(0x1BD09C);            // if ( OnlineService::GetSeasonNum() != season_created ) in Console::UIOnlineActions::SetGameParamsForHero()
        jest.Patch<Movz>(0x1BD140, W21, global_config.seasons.number);  // season_created = ...
        jest.Patch<Nop>(0x1BD388);            // skip actual SeasonNum check in Console::UIOnlineActions::IsActiveSeason
        jest.Patch<Movz>(0x1BF5F0, W0, 0);    // error_none for Console::UIOnlineActions::ValidateHeroForPartyMember
        jest.Patch<Ret>(0x1BF5F4);            // ^ ret
        jest.Patch<Movz>(0x351CBC, W1, 1);    // always true for UIHeroCreate::Console::bSeasonConfirmedAvailable() (skip B.ne and always confirm)

        // jest.Patch<Ret>(0xBF6D50);           // stub bdEnvironmentString() to force net offline
        // jest.Patch<Movz>(0x1BD360, W0, 1);   // always true for Console::UIOnlineActions::IsActiveSeason
        // jest.Patch<Ret>(0x1BD364);

        // jest.Patch<Movz>(0x57260, W0, 0);  // online_play_allowed for  Console::GamerProfile::GetOnlinePlayPrivilege(
        // jest.Patch<Ret>(0x57264);

        // jest.Patch<Movz>(0x72EEC0, W3, 1);  // OverrideEnabled
    }

    void PatchForcedEvents() {
        if (!global_config.events.active)
            return;
        auto jest = patch::RandomAccessPatcher();
        // jest.Patch<Ret>(0x641F0);  // stub Console::Online::LobbyServiceInternal::OnConfigFileRetrieved() to override server data
        // jest.Patch<Branch>(0x64228, 0x24);  //0x28
        // jest.Patch<Nop>(0x4A6148);
        // jest.Patch<Nop>(0x4A614C);
        // jest.Patch<Branch>(0x4A6158, 0x1C);
        // jest.Patch<Nop>(0x64254);  // stub Console::Online::LobbyServiceInternal::OnConfigFileRetrieved() ?

        // jest.Patch<Ret>(0xB2340);           // hey look im doing bullshit
        // jest.Patch<dword>(0xF07FB8, make_bytes(0x01, 0x02, 0x03, 0x04));

        // try for challenge rift
        // jest.Patch<dword>(0x66B10, make_bytes(0xFE, 0xDE, 0xFF, 0xE7));

        // jest.Patch<Movz>(0x185F9C, W0, 0);  // always STORAGE_SUCCESS

        jest.Patch<Movz>(0x56B10, W0, 1);   // always Console::GamerProfile::IsSignedInOnline
        jest.Patch<Ret>(0x56B14);           // ^ ret

        if (global_config.events.DarkAlchemy)            jest.Patch<Movz>(0x4A7E98, W3, 1);
        if (global_config.events.IgrEnabled)             jest.Patch<Movz>(0x4A7F24, W3, 1);
        if (global_config.events.AnniversaryEnabled)     jest.Patch<Movz>(0x4A7F84, W3, 1);
        if (global_config.events.EasterEggWorldEnabled)  jest.Patch<Movz>(0x4A7FB4, W3, 1);

        // CommunityEventXP / LegendaryFind / GoldFind + IgrXP
        // jest.Patch<dword>(0x4A7B30, make_bytes(0x48, 0x50, 0x00, 0xB0));
        // jest.Patch<dword>(0x4A7B34, make_bytes(0x08, 0x59, 0x4A, 0xBD));  // 1.0e12 @ 0xEB0A58 (100 Trillion)
        // // jest.Patch<dword>(0x4A7B34, make_bytes(0x08, 0xF0, 0x27, 0x1E));
        // // jest.Patch<dword>(0x4A7B34, make_bytes(0x08, 0xE9, 0x4C, 0xBD));  // 1.0 @ 0xEB0CE8
        // // jest.Patch<dword>(0x4A7B34, make_bytes(0x08, 0x79, 0x4B, 0xBD)); // 1.0e10 @ 0xEB0B78
        // // jest.Patch<dword>(0x4A7B34, make_bytes(0x08, 0xE5, 0x49, 0xBD));  // 1.0+14 @ 0xEB09E4

        if (global_config.events.DoubleRiftKeystones)    jest.Patch<Movz>(0x4A7BD8, W3, 1);
        if (global_config.events.DoubleBloodShards)      jest.Patch<Movz>(0x4A7C04, W3, 1);
        if (global_config.events.DoubleTreasureGoblins)  jest.Patch<Movz>(0x4A7C30, W3, 1);
        if (global_config.events.DoubleBountyBags)       jest.Patch<Movz>(0x4A7C5C, W3, 1);
        if (global_config.events.RoyalGrandeur)          jest.Patch<Movz>(0x4A7C88, W3, 1);
        if (global_config.events.LegacyOfNightmares)     jest.Patch<Movz>(0x4A7CB4, W3, 1);
        if (global_config.events.TriunesWill)            jest.Patch<Movz>(0x4A7CE0, W3, 1);
        if (global_config.events.Pandemonium)            jest.Patch<Movz>(0x4A7D0C, W3, 1);
        if (global_config.events.KanaiPowers)            jest.Patch<Movz>(0x4A7D38, W3, 1);
        if (global_config.events.TrialsOfTempests)       jest.Patch<Movz>(0x4A7D64, W3, 1);
        if (global_config.events.ShadowClones)           jest.Patch<Movz>(0x4A7D90, W3, 1);
        if (global_config.events.FourthKanaisCubeSlot)   jest.Patch<Movz>(0x4A7DBC, W3, 1);
        if (global_config.events.EthrealItems)           jest.Patch<Movz>(0x4A7DE8, W3, 1);
        if (global_config.events.SoulShards)             jest.Patch<Movz>(0x4A7E14, W3, 1);
        if (global_config.events.SwarmRifts)             jest.Patch<Movz>(0x4A7E40, W3, 1);
        if (global_config.events.SanctifiedItems)        jest.Patch<Movz>(0x4A7E6C, W3, 1);
        if (global_config.events.DarkAlchemy)            jest.Patch<Movz>(0x4A7E98, W3, 1);
        if (global_config.events.NestingPortals)         jest.Patch<Movz>(0x4A7EF4, W3, 1);

        // jest.Patch<Movz>(0x4A7EC4, W3, 0);  // ParagonCap
    }

    void PortCheatCodes() {
        if (!global_config.rare_cheats.active)
            return;
        auto jest = patch::RandomAccessPatcher();
        /* Restore debug display of allocation errors */
        jest.Patch<Movz>(0x00A2AD04, W8, 0);
        // jest.Patch<MovRegister>(0x00A37C70, X0, SP);
        jest.Patch<dword>(0x00A37C70, make_bytes(0xE0, 0x03, 0x00, 0x91));

        /* Drop any item (Staff of Herding, etc) */
        if (global_config.rare_cheats.active && global_config.rare_cheats.drop_anything)
            jest.Patch<Movn>(0x00504C78, W0, 0);
        // 04000000 00504C78 12800000

        /* 100% Legendary probability (Kadala/Kanai) */
        if (global_config.rare_cheats.active && global_config.rare_cheats.guaranteed_legendaries) {
            jest.Patch<Nop>(0x0088F82C);  // unlikely? shot in the dark
            jest.Patch<Nop>(0x0088F834);
            jest.Patch<Branch>(0x0088F83C, 0x18C);
            jest.Patch<Nop>(0x0088F9C4);
            jest.Patch<Branch>(0x0088F9E0, 0x350);
        }
        // jest.Patch<dword>(0x0088F9C4, make_bytes(0xD4, 0x00, 0x00, 0x14));
        // 04000000 0088F9E0 140000D4

        /* Instant town portal & Book of Cain */
        if (global_config.rare_cheats.active && global_config.rare_cheats.instant_portal)
            jest.Patch<Movz>(0x009E3250, W24, 0);
        // 04000000 009E3250 52800018

        /* Show testing art cosmetics */
        // jest.Patch<CmnImmediate>(0x004FFFE4, W2, 0);
        // 04000000 004FFFE4 3100081F

        /* ►Primal ancient probability 100% */
        // jest.Patch<Branch>(0x088DDFC, 0x360);
        // jest.Patch<Movz>(0x0088DE58, W8, 2);
        // jest.Patch<dword>(0x0088DF10, make_bytes(0x40, 0x21, 0x2A, 0x1E));
        // jest.Patch<dword>(0x0088DFFC, make_bytes(0x00, 0x20, 0x20, 0x1E));  // covered by W8, 2
        // jest.Patch<dword>(0x0088DFE0, make_bytes(0x1F, 0x20, 0x03, 0xD5));
        /*
        04000000 0088DF10 1E2A2140
        04000000 0088DFFC 1E202000
        04000000 0088DFE0 D503201F
        */

        /* ►No cooldown */
        if (global_config.rare_cheats.active && global_config.rare_cheats.no_cooldowns) {
            jest.Patch<dword>(0x007396D0, make_bytes(0xE0, 0x03, 0x27, 0x1E));
            jest.Patch<dword>(0x007F8960, make_bytes(0xE0, 0x03, 0x00, 0x2A));
            jest.Patch<dword>(0x009BC53C, make_bytes(0xE8, 0x03, 0x27, 0x1E));
            jest.Patch<dword>(0x009BC828, make_bytes(0xE8, 0x03, 0x27, 0x1E));
        }
        /*
        04000000 007396D0 1E2703E0
        04000000 007F8960 2A0003E0
        04000000 009BC53C 1E2703E8
        04000000 009BC828 1E2703E8
        */

        /* ►Instantly identify items */
        if (global_config.rare_cheats.active && global_config.rare_cheats.instant_craft_actions)
            jest.Patch<Nop>(0x0020636C);
        // 04000000 0020636C D503201F

        /* ►Instantly craft items */
        if (global_config.rare_cheats.active && global_config.rare_cheats.instant_craft_actions)
            jest.Patch<AddImmediate>(0x0040F9B0, W8, W8, 5);
        // 04000000 0040F9B0 11001508

        /* ►Instantly enchant items */
        if (global_config.rare_cheats.active && global_config.rare_cheats.instant_craft_actions)
            jest.Patch<AddImmediate>(0x001DCFA4, W8, W8, 1);
        // 04000000 001DCFA4 11000508

        /* ►Instantly craft Kanai's Cube items */
        if (global_config.rare_cheats.active && global_config.rare_cheats.instant_craft_actions) {
            jest.Patch<Movz>(0x00236D20, W8, 0);
            jest.Patch<Movz>(0x00236D24, W9, 0);
            jest.Patch<Movz>(0x00236D28, W10, 0);
            jest.Patch<Movz>(0x00236D30, W8, 0);
        }
        /*
        04000000 00236D20 52800008
        04000000 00236D24 52800009
        04000000 00236D28 5280000A
        04000000 00236D30 52800008
        */

        /* ►Kanai's Cube does not consume materials */
        jest.Patch<Ret>(0x008874D0);
        // 04000000 008874D0 D65F03C0

        /* ►Greater Rift Lv. 150 after clearing once */
        // jest.Patch<dword>(0x004873E4, make_bytes(0xF5, 0x03, 0x00, 0x2A));
        // 04000000 004873E4 2A0003F5

        /* ►Legendary Gem Upgrade 100% */
        jest.Patch<dword>(0x006A255C, make_bytes(0x08, 0x10, 0x2E, 0x1E));
        // 04000000 006A255C 1E2E1008*/

        /* ►Legendary Gem Upgrade Speed */
        jest.Patch<dword>(0x00349B98, make_bytes(0x02, 0x10, 0x2E, 0x1E));
        // 04000000 00349B98 1E2E1002

        /* ►Legendary Gem Lv. 150 after upgrading once */
        /*
        mov w22, w0
        movz w0, #0x96
        sub w23, w0, w22
        */
        // jest.Patch<Movk>(0x00C72F7C, W22, W0);  // MOV LIKE THIS?
        jest.Patch<dword>(0x00C72F7C, make_bytes(0xF6, 0x03, 0x00, 0x2A));
        jest.Patch<Movz>(0x00C72F80, W0, 0x96);
        // jest.Patch<Sub>(0x00C72F84, W23, W0, W22);
        jest.Patch<dword>(0x00C72F84, make_bytes(0x17, 0x00, 0x16, 0x4B));
        jest.Patch<Ret>(0x00C72F88);
        jest.Patch<dword>(0x007DF958, make_bytes(0x89, 0x4D, 0x12, 0x94));
        /*
        04000000 00C72F7C 2A0003F6
        04000000 00C72F80 528012C0
        04000000 00C72F84 4B160017
        04000000 00C72F88 D65F03C0
        04000000 007DF958 94124D89
        */

        // [►Equip Multiple Legendary Item]
        // jest.Patch<Branch>(0x004FB9E4, 0x5C);
        // 04000000 004FB9E4 14000017

        // [►Socket Any Gem To Any Slot]
        if (global_config.rare_cheats.active && global_config.rare_cheats.any_gem_any_slot) {
            jest.Patch<Movz>(0x004F9F1C, W25, 5);
            jest.Patch<Branch>(0x004F9F24, 0xA0);
        }

        // [►Auto Pickup]
        if (global_config.rare_cheats.active && global_config.rare_cheats.auto_pickup)
            jest.Patch<Nop>(0x004F98DC);
        // 04000000 004F98DC D503201F
    }

    void PatchBase() {
        auto jest = patch::RandomAccessPatcher();

        PortCheatCodes();

        /* String swap for autosave screen */
        MakeAdrlPatch(0x335B68, reinterpret_cast<uintptr_t>(&c_szHackVerAutosave), X0);

        /* String swap for start screen */
        MakeAdrlPatch(0x3656B8, reinterpret_cast<uintptr_t>(&c_szHackVerStart), X0);

        /* Enable local logging */
        jest.Patch<Movz>(0x5F0, W0, 1);

        // 0x00A2C614 - E5 0A 00 12
        // AND     W5, W23, #7
        // jest.Patch<exl::patch::inst::

        /* Pretend we are NOT in a mounted ROM */
        // jest.Patch<Movz>(0x84C, W1, 0);

        /* No timed wait for autosave warning */
        // jest.Patch<Nop>(0x335C18);

        /* Inside ItemSetIsBeingManipulated(): BeingManipulated Attrib = 0 */
        // jest.Patch<Branch>(0x0BF070, 0x20);  // CBZ W1, loc_BF090

        /* Don't try to find a slot to equip newly duped item */
        jest.Patch<Nop>(0x802944);  // BL ACDInventoryWhereCanThisGo()

        /* Stub functions that interfere with item duping */
        jest.Patch<Ret>(0x03E180); /* void GameSoftPause(BOOL bPause) */
        jest.Patch<Ret>(0x481160); /* void ActorCommonData::SetAssignedHeroID(ActorCommonData *this, const Player *ptPlayer) */
        jest.Patch<Ret>(0x483D00); /* void ActorCommonData::SetAssignedHeroID(ActorCommonData *this, OnlineService::HeroId idHero) */
        jest.Patch<Ret>(0x7B71D0); /* void SGameSoftPause(BOOL bPause) */

        /* Stub logging to RingBufer */
        jest.Patch<Ret>(0xA2C6B0); /* void FileOutputStream::LogToRingBuffer(int, char const*, char const*) */

        /* Stub net message tracing */
        jest.Patch<Ret>(0x962B10); /* void __fastcall sTraceMessage(const void *pMessage) */

        /* Fix path for stat tracing */
        MakeAdrlPatch(0x7B1C50, reinterpret_cast<uintptr_t>(&c_szTraceStat), X0);

        /* ItemCanDrop bypass */
        // jest.Patch<dword>(0x369384, make_bytes(0x10, 0x00, 0x00, 0x14));

        /* Increase the damage for ACTOR_EASYKILL_BIT */
        jest.Patch<Movz>(0x97F55C, W9, 0x7bc7);
        jest.Patch<Movk>(0x97F560, W9, 0x5d94, ShiftValue_16);
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
