// Patch-only offset entries included by `source/program/offsets.hpp`.
// This file is intentionally included inside a template parameter list; keep entry lines comma-terminated.

    /* Patch/event flags (main module). */
    {util::ModuleIndex::Main, 0x114ACB8, "event_buff_start"},
    {util::ModuleIndex::Main, 0x114ACC0, "event_buff_end"},
    {util::ModuleIndex::Main, 0x114AE38, "event_egg_flag"},
    {util::ModuleIndex::Main, 0x1154BF0, "event_igr"},
    {util::ModuleIndex::Main, 0x1154C00, "event_ann"},
    {util::ModuleIndex::Main, 0x1152CC8, "event_egg_xvar"},
    {util::ModuleIndex::Main, 0x114ACD8, "event_enabled"},
    {util::ModuleIndex::Main, 0x114ACE8, "event_season_only"},
    {util::ModuleIndex::Main, 0x18FFFC8, "event_double_keystones"},
    {util::ModuleIndex::Main, 0x1900080, "event_double_blood_shards"},
    {util::ModuleIndex::Main, 0x1900138, "event_double_goblins"},
    {util::ModuleIndex::Main, 0x19001F0, "event_double_bounty_bags"},
    {util::ModuleIndex::Main, 0x19002A8, "event_royal_grandeur"},
    {util::ModuleIndex::Main, 0x1900360, "event_legacy_of_nightmares"},
    {util::ModuleIndex::Main, 0x1900418, "event_triunes_will"},
    {util::ModuleIndex::Main, 0x19004D0, "event_pandemonium"},
    {util::ModuleIndex::Main, 0x1900588, "event_kanai_powers"},
    {util::ModuleIndex::Main, 0x1900640, "event_trials_of_tempests"},
    {util::ModuleIndex::Main, 0x19006F8, "event_shadow_clones"},
    {util::ModuleIndex::Main, 0x19007B0, "event_fourth_cube_slot"},
    {util::ModuleIndex::Main, 0x1900868, "event_ethereal_items"},
    {util::ModuleIndex::Main, 0x1900920, "event_soul_shards"},
    {util::ModuleIndex::Main, 0x19009D8, "event_swarm_rifts"},
    {util::ModuleIndex::Main, 0x1900A90, "event_sanctified_items"},
    {util::ModuleIndex::Main, 0x1900B48, "event_dark_alchemy"},
    {util::ModuleIndex::Main, 0x18FF848, "event_nesting_portals"},
    {util::ModuleIndex::Main, 0x1900C00, "event_paragon_cap"},

    /* XVar pointers (main module). */
    {util::ModuleIndex::Main, 0x115A408, "xvar_local_logging_enable_ptr"},
    {util::ModuleIndex::Main, 0x1158FB0, "xvar_online_service_ptr"},
    {util::ModuleIndex::Main, 0x1158FD0, "xvar_free_to_play_ptr"},
    {util::ModuleIndex::Main, 0x11590D8, "xvar_seasons_override_enabled_ptr"},
    {util::ModuleIndex::Main, 0x1158FD8, "xvar_challengerift_enabled_ptr"},
    {util::ModuleIndex::Main, 0x114AC60, "xvar_season_num_ptr"},
    {util::ModuleIndex::Main, 0x114AC68, "xvar_season_state_ptr"},
    {util::ModuleIndex::Main, 0x11551D0, "xvar_max_paragon_level_ptr"},
    {util::ModuleIndex::Main, 0x11597C8, "xvar_experimental_scheduling_ptr"},
    {util::ModuleIndex::Main, 0x1159808, "xvar_broadcast_ptr"},       // unused
    {util::ModuleIndex::Main, 0x1A482C8, "local_logging_state_ptr"},  // unused

    // Static patch offsets (main module).
    // Sourced from `source/program/d3/patches.hpp` (including resolution hack).
    // Naming: `patch_<group>_<NNN>_<kind>` where NNN is order-of-appearance within that patch function.

    /* Patch-only data addresses (ro/rw). */
    {util::ModuleIndex::Main, 0xE6D9E4, "data_build_locker_format_rw"},  // RwFromAddr | szBuildLockerFormat
    {util::ModuleIndex::Main, 0xE46144, "data_release_fps_format_rw"},   // RwFromAddr | szReleaseFPSFormat
    {util::ModuleIndex::Main, 0x1098658, "data_release_fps_pos_x_ro"},   // RoFromAddr | nReleaseFPSPosX
    {util::ModuleIndex::Main, 0x109865C, "data_release_fps_pos_y_ro"},   // RoFromAddr | nReleaseFPSPosY

    /* Patch_buildlocker */
    {util::ModuleIndex::Main, 0xF6C, "patch_buildlocker_01_nop"},      // NOP
    {util::ModuleIndex::Main, 0xFD4, "patch_buildlocker_02_movz"},     // MOVZ | W20
    {util::ModuleIndex::Main, 0x000FA4, "patch_buildlocker_03_adrl"},  // ADRP/ADD | &c_tSignature | X2
    {util::ModuleIndex::Main, 0x001040, "patch_buildlocker_04_adrl"},  // ADRP/ADD | &crgbaTan | X19
    {util::ModuleIndex::Main, 0x001228, "patch_buildlocker_05_adrl"},  // ADRP/ADD | &c_szHackVerWatermark | X1
    {util::ModuleIndex::Main, 0x1088, "patch_buildlocker_06_movz"},    // MOVZ | W21 | RENDERLAYER_UI_OVERLAY
    {util::ModuleIndex::Main, 0x10FC, "patch_buildlocker_07_bytes"},   // 00 28 29 1E | FADD | S0, S0, S9 | X-axis from left | patch: 00 28 2A 1E | S0, +15.0 (S10) instead of -15.0
    {util::ModuleIndex::Main, 0x1128, "patch_buildlocker_08_bytes"},   // 00 28 29 1E | FADD | S0, S0, S9 | patch: 00 28 2A 1E | S0, +15.0 (S10) instead of -15.0
    {util::ModuleIndex::Main, 0x1224, "patch_buildlocker_09_nop"},     // NOP
    {util::ModuleIndex::Main, 0x11C0, "patch_buildlocker_10_bytes"},   // unused | 0A 10 2C 1E | FMOV | S10, #0.5 | center X axis on screen | patch: 0A 10 2E 1E | #1.0, aka shift right
    /* Patch_var_res_label */
    {util::ModuleIndex::Main, 0x03CC20, "patch_var_res_label_01_movz"},  // MOVZ | W0
    {util::ModuleIndex::Main, 0x03CC78, "patch_var_res_label_02_movz"},  // MOVZ | X8 | X-axis: 68.0f
    {util::ModuleIndex::Main, 0x03CC7C, "patch_var_res_label_03_movk"},  // MOVK | X8 | Y-axis: 4.0f
    {util::ModuleIndex::Main, 0x03CC68, "patch_var_res_label_04_adrl"},  // unused | ADRP/ADD | &c_szVariableResString | X1
    /* Patch_release_fps_label */
    {util::ModuleIndex::Main, 0x3F654, "patch_release_fps_label_01_movz"},   // MOVZ | W0
    {util::ModuleIndex::Main, 0x03F69C, "patch_release_fps_label_02_nop"},   // NOP
    {util::ModuleIndex::Main, 0x03F6A0, "patch_release_fps_label_03_adrl"},  // ADRP/ADD | &crgbaWhite | X8
    {util::ModuleIndex::Main, 0x03F840, "patch_release_fps_label_04_nop"},   // NOP
    {util::ModuleIndex::Main, 0x03F8E4, "patch_release_fps_label_05_nop"},   // NOP
    {util::ModuleIndex::Main, 0x03F9A8, "patch_release_fps_label_06_nop"},   // NOP
    {util::ModuleIndex::Main, 0x03FA68, "patch_release_fps_label_07_nop"},   // NOP
    {util::ModuleIndex::Main, 0x03FB80, "patch_release_fps_label_08_nop"},   // NOP
    {util::ModuleIndex::Main, 0x03FC7C, "patch_release_fps_label_09_nop"},   // NOP
    {util::ModuleIndex::Main, 0x03FD78, "patch_release_fps_label_10_nop"},   // NOP
    /* Patch_ddm_labels */
    {util::ModuleIndex::Main, 0x03E530, "patch_ddm_labels_01_movz"},  // MOVZ | W0
    {util::ModuleIndex::Main, 0x03E568, "patch_ddm_labels_02_adrl"},  // ADRP/ADD | &c_szHackVerWatermark | X1
    {util::ModuleIndex::Main, 0x03E580, "patch_ddm_labels_03_adrl"},  // ADRP/ADD | &c_rgbaText | X3
    {util::ModuleIndex::Main, 0x03E58C, "patch_ddm_labels_04_adrl"},  // ADRP/ADD | &c_rgbaDropShadow | X4
    {util::ModuleIndex::Main, 0x03E594, "patch_ddm_labels_05_movz"},  // MOVZ | W7 | RENDERLAYER_UI_OVERLAY
    {util::ModuleIndex::Main, 0x03FE78, "patch_ddm_labels_06_movz"},  // MOVZ | W0
    {util::ModuleIndex::Main, 0x03FE3C, "patch_ddm_labels_07_adrl"},  // ADRP/ADD | &c_szHackVerWatermark | X23
    {util::ModuleIndex::Main, 0x03FF18, "patch_ddm_labels_08_adrl"},  // ADRP/ADD | &c_rgbaText | X3
    {util::ModuleIndex::Main, 0x03FF28, "patch_ddm_labels_09_adrl"},  // ADRP/ADD | &c_rgbaDropShadow | X4
    {util::ModuleIndex::Main, 0x03FF20, "patch_ddm_labels_10_movz"},  // MOVZ | W7 | RENDERLAYER_UI_OVERLAY
    /* Patch_resolution_targets */
    {util::ModuleIndex::Main, 0x0E7858, "patch_resolution_targets_01_movz"},  // MOVZ | X8 | output width
    {util::ModuleIndex::Main, 0x0E7860, "patch_resolution_targets_02_movk"},  // MOVK | X8 | output height
    {util::ModuleIndex::Main, 0x0E785C, "patch_resolution_targets_03_movz"},  // MOVZ | X9 | fallback width
    {util::ModuleIndex::Main, 0x0E7864, "patch_resolution_targets_04_movk"},  // MOVK | X9 | fallback height
    {util::ModuleIndex::Main, 0x03CBBC, "patch_resolution_targets_05_movk"},  // MOVK | X9 | flMinPercent
    {util::ModuleIndex::Main, 0x03CBCC, "patch_resolution_targets_06_movz"},  // unused | MOVZ | W8 | flMaxPercent
    {util::ModuleIndex::Main, 0x03CBD4, "patch_resolution_targets_07_movk"},  // MOVK | X9 | flPercentIncr
    {util::ModuleIndex::Main, 0x276A60, "patch_resolution_targets_08_movz"},  // MOVZ | W9 | font point size
    {util::ModuleIndex::Main, 0x276A6C, "patch_resolution_targets_09_movz"},  // MOVZ | W8 | font point size
    {util::ModuleIndex::Main, 0x6678B8, "patch_resolution_targets_10_movz"},  // MOVZ | W0 | OperationMode (Docked)
    {util::ModuleIndex::Main, 0x667AE8, "patch_resolution_targets_11_movz"},  // MOVZ | W0 | OperationMode (Docked)
    {util::ModuleIndex::Main, 0x6678C8, "patch_resolution_targets_12_movz"},  // MOVZ | W0 | PerformanceMode (Boost)
    {util::ModuleIndex::Main, 0x667B04, "patch_resolution_targets_13_movz"},  // MOVZ | W0 | PerformanceMode (Boost)
    /* Patch_dynamic_seasonal */
    {util::ModuleIndex::Main, 0x72ED98, "patch_dynamic_seasonal_01_movz"},  // MOVZ | W3 | Season.Num
    {util::ModuleIndex::Main, 0x72EDD0, "patch_dynamic_seasonal_02_movz"},  // MOVZ | W3 | Season.State
    {util::ModuleIndex::Main, 0x351CBC, "patch_dynamic_seasonal_03_movz"},  // MOVZ | W1 | always true for UIHeroCreate::Console::bSeasonConfirmedAvailable() (skip B.ne and always confirm)
    {util::ModuleIndex::Main, 0x5FF0C, "patch_dynamic_seasonal_04_movz"},   // MOVZ | W0 | always true for Console::Online::IsSeasonsInitialized()
    {util::ModuleIndex::Main, 0x1BD140, "patch_dynamic_seasonal_05_movz"},  // unused | MOVZ | W21 | season_created = ...
    {util::ModuleIndex::Main, 0x358A7C, "patch_dynamic_seasonal_06_nop"},   // unused | NOP | B.ne loc_358C74
    {util::ModuleIndex::Main, 0x35C0C4, "patch_dynamic_seasonal_07_movz"},  // unused | MOVZ | W0
    /* Patch_dynamic_events */
    {util::ModuleIndex::Main, 0x4A7BD8, "patch_dynamic_events_01_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7C04, "patch_dynamic_events_02_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7C30, "patch_dynamic_events_03_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7C5C, "patch_dynamic_events_04_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7C88, "patch_dynamic_events_05_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7CB4, "patch_dynamic_events_06_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7CE0, "patch_dynamic_events_07_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7D0C, "patch_dynamic_events_08_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7D38, "patch_dynamic_events_09_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7D64, "patch_dynamic_events_10_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7D90, "patch_dynamic_events_11_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7DBC, "patch_dynamic_events_12_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7DE8, "patch_dynamic_events_13_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7E14, "patch_dynamic_events_14_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7E40, "patch_dynamic_events_15_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7E6C, "patch_dynamic_events_16_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7E98, "patch_dynamic_events_17_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7EF4, "patch_dynamic_events_18_movz"},  // unused | MOVZ | W3
    {util::ModuleIndex::Main, 0x4A7EC4, "patch_dynamic_events_19_movz"},  // unused | MOVZ | W3 | ParagonCap
    /* Patch_port_cheat_codes */
    {util::ModuleIndex::Main, 0x00A2AD04, "patch_cheat_alloc_errors_01_movz"},              // MOVZ | W8
    {util::ModuleIndex::Main, 0x00A37C70, "patch_cheat_alloc_errors_02_bytes"},             // patch: E0 03 00 91

    {util::ModuleIndex::Main, 0x00504C78, "patch_cheat_drop_anything_01_movn"},             // MOVN | W0

    {util::ModuleIndex::Main, 0x0088F82C, "patch_cheat_guaranteed_legendaries_01_nop"},     // NOP | unlikely? shot in the dark
    {util::ModuleIndex::Main, 0x0088F834, "patch_cheat_guaranteed_legendaries_02_nop"},     // NOP
    {util::ModuleIndex::Main, 0x0088F83C, "patch_cheat_guaranteed_legendaries_03_branch"},  // BRANCH | 0x18C
    {util::ModuleIndex::Main, 0x0088F9C4, "patch_cheat_guaranteed_legendaries_04_nop"},     // NOP
    {util::ModuleIndex::Main, 0x0088F9E0, "patch_cheat_guaranteed_legendaries_05_branch"},  // BRANCH | 0x350

    {util::ModuleIndex::Main, 0x009E3250, "patch_cheat_instant_portal_01_movz"},            // MOVZ | W24

    {util::ModuleIndex::Main, 0x004FFFE4, "patch_cheat_testing_cosmetics_01_cmn_imm"},      // unused | CMN_IMM | W2

    {util::ModuleIndex::Main, 0x0088DDFC, "patch_cheat_primal_01_branch"},                  // unused | BRANCH | 0x360
    {util::ModuleIndex::Main, 0x0088DE58, "patch_cheat_primal_02_movz"},                    // unused | MOVZ | W8
    {util::ModuleIndex::Main, 0x0088DF10, "patch_cheat_primal_03_bytes"},                   // unused | patch: 40 21 2A 1E
    {util::ModuleIndex::Main, 0x0088DFFC, "patch_cheat_primal_04_bytes"},                   // unused | patch: 00 20 20 1E | covered by W8, 2
    {util::ModuleIndex::Main, 0x0088DFE0, "patch_cheat_primal_05_bytes"},                   // unused | patch: 1F 20 03 D5

    {util::ModuleIndex::Main, 0x007396D0, "patch_cheat_no_cooldowns_01_bytes"},             // patch: E0 03 27 1E
    {util::ModuleIndex::Main, 0x007F8960, "patch_cheat_no_cooldowns_02_bytes"},             // patch: E0 03 00 2A
    {util::ModuleIndex::Main, 0x009BC53C, "patch_cheat_no_cooldowns_03_bytes"},             // patch: E8 03 27 1E
    {util::ModuleIndex::Main, 0x009BC828, "patch_cheat_no_cooldowns_04_bytes"},             // patch: E8 03 27 1E

    {util::ModuleIndex::Main, 0x0020636C, "patch_cheat_instant_identify_01_nop"},           // NOP
    {util::ModuleIndex::Main, 0x0040F9B0, "patch_cheat_instant_craft_01_add_imm"},          // ADD_IMM | W8 | +5
    {util::ModuleIndex::Main, 0x001DCFA4, "patch_cheat_instant_enchant_01_add_imm"},        // ADD_IMM | W8 | +1

    {util::ModuleIndex::Main, 0x00236D20, "patch_cheat_cube_instant_craft_01_movz"},        // MOVZ | W8
    {util::ModuleIndex::Main, 0x00236D24, "patch_cheat_cube_instant_craft_02_movz"},        // MOVZ | W9
    {util::ModuleIndex::Main, 0x00236D28, "patch_cheat_cube_instant_craft_03_movz"},        // MOVZ | W10
    {util::ModuleIndex::Main, 0x00236D30, "patch_cheat_cube_instant_craft_04_movz"},        // MOVZ | W8

    {util::ModuleIndex::Main, 0x008874D0, "patch_cheat_cube_no_consume_01_ret"},            // RET

    {util::ModuleIndex::Main, 0x004873E4, "patch_cheat_gr150_01_bytes"},                    // unused | patch: F5 03 00 2A

    {util::ModuleIndex::Main, 0x006A255C, "patch_cheat_gem_upgrade_01_bytes"},              // patch: 08 10 2E 1E
    {util::ModuleIndex::Main, 0x00349B98, "patch_cheat_gem_speed_01_bytes"},                // patch: 02 10 2E 1E

    {util::ModuleIndex::Main, 0x00C72F7C, "patch_cheat_gem_lvl150_01_bytes"},               // patch: F6 03 00 2A
    {util::ModuleIndex::Main, 0x00C72F80, "patch_cheat_gem_lvl150_02_movz"},                // MOVZ | W0 | 0x96
    {util::ModuleIndex::Main, 0x00C72F84, "patch_cheat_gem_lvl150_03_bytes"},               // patch: 17 00 16 4B
    {util::ModuleIndex::Main, 0x00C72F88, "patch_cheat_gem_lvl150_04_ret"},                 // RET
    {util::ModuleIndex::Main, 0x007DF958, "patch_cheat_gem_lvl150_05_bytes"},               // patch: 89 4D 12 94

    {util::ModuleIndex::Main, 0x004FB9E4, "patch_cheat_multi_legendary_01_branch"},         // unused | BRANCH | 0x5C

    {util::ModuleIndex::Main, 0x004F9F1C, "patch_cheat_any_gem_any_slot_01_movz"},          // MOVZ | W25 | 5
    {util::ModuleIndex::Main, 0x004F9F24, "patch_cheat_any_gem_any_slot_02_branch"},        // BRANCH | 0xA0

    {util::ModuleIndex::Main, 0x004F98DC, "patch_cheat_auto_pickup_01_nop"},                // NOP

    /* Patch_base */
    {util::ModuleIndex::Main, 0x56B10, "patch_signin_01_movz"},               // MOVZ | W0 | always Console::GamerProfile::IsSignedInOnline
    {util::ModuleIndex::Main, 0x56B14, "patch_signin_02_ret"},                // RET | ^ ret

    {util::ModuleIndex::Main, 0x335B68, "patch_autosave_string_01_adrl"},     // ADRP/ADD | &c_szHackVerAutosave | X0
    {util::ModuleIndex::Main, 0x3656B8, "patch_start_string_01_adrl"},        // ADRP/ADD | &c_szHackVerStart | X0

    {util::ModuleIndex::Main, 0x5F0, "patch_local_logging_01_movz"},          // MOVZ | W0

    {util::ModuleIndex::Main, 0x84C, "patch_rom_mount_01_movz"},              // unused | MOVZ | W1
    {util::ModuleIndex::Main, 0x335C18, "patch_autosave_wait_01_nop"},        // unused | NOP
    {util::ModuleIndex::Main, 0x0BF070, "patch_item_manipulated_01_branch"},  // unused | BRANCH | 0x20 | CBZ W1, loc_BF090

    {util::ModuleIndex::Main, 0x802944, "patch_dupe_noequip_01_nop"},         // NOP | BL ACDInventoryWhereCanThisGo()
    {util::ModuleIndex::Main, 0x03E180, "patch_dupe_stub_01_ret"},            // RET | GameSoftPause
    {util::ModuleIndex::Main, 0x481160, "patch_dupe_stub_02_ret"},            // RET | ActorCommonData::SetAssignedHeroID(Player*)
    {util::ModuleIndex::Main, 0x483D00, "patch_dupe_stub_03_ret"},            // RET | ActorCommonData::SetAssignedHeroID(HeroId)
    {util::ModuleIndex::Main, 0x7B71D0, "patch_dupe_stub_04_ret"},            // RET | SGameSoftPause

    {util::ModuleIndex::Main, 0xA2C6B0, "patch_log_ringbuffer_01_ret"},       // RET | FileOutputStream::LogToRingBuffer
    {util::ModuleIndex::Main, 0x962B10, "patch_trace_message_01_ret"},        // RET | sTraceMessage
    {util::ModuleIndex::Main, 0x7B1C50, "patch_trace_stat_path_01_adrl"},     // ADRP/ADD | &c_szTraceStat | X0

    {util::ModuleIndex::Main, 0x35C1A0, "patch_hideonline_01_branch"},        // unused | BRANCH | -0x90 | force false path in ItemShouldBeVisible
    {util::ModuleIndex::Main, 0x061620, "patch_hideonline_02_movz"},          // MOVZ | W0 | Pause + Main menu: hide "Connect to Diablo Servers" list entry | return false
    {util::ModuleIndex::Main, 0x061624, "patch_hideonline_03_ret"},           // RET
    {util::ModuleIndex::Main, 0x3627EC, "patch_hideonline_04_movz"},          // MOVZ | W1 | Main menu status label | connected path
    {util::ModuleIndex::Main, 0x362800, "patch_hideonline_05_movz"},          // MOVZ | W1 | Main menu status label | disconnected path
    {util::ModuleIndex::Main, 0x362840, "patch_hideonline_06_bytes"},         // patch: 08 29 40 F9 | LDR X8, [X8,#0x50]
    {util::ModuleIndex::Main, 0x362844, "patch_hideonline_07_movz"},          // MOVZ | W1
    {util::ModuleIndex::Main, 0x221324, "patch_hideonline_08_movz"},          // MOVZ | W1 | Pause menu status label | connected path
    {util::ModuleIndex::Main, 0x221338, "patch_hideonline_09_movz"},          // MOVZ | W1 | Pause menu status label | disconnected path
    {util::ModuleIndex::Main, 0x221378, "patch_hideonline_10_bytes"},         // patch: 08 29 40 F9 | LDR X8, [X8,#0x50]
    {util::ModuleIndex::Main, 0x22137C, "patch_hideonline_11_movz"},          // MOVZ | W1 | W1 = 0 (invisible)

    {util::ModuleIndex::Main, 0x5281B0, "patch_handicap_unlock_01_movz"},     // MOVZ | W0 | PlayerCanPlayHandicap(): return true (unlock all difficulties)
    {util::ModuleIndex::Main, 0x5281B4, "patch_handicap_unlock_02_ret"},      // RET

    {util::ModuleIndex::Main, 0x369384, "patch_itemcandrop_01_bytes"},        // unused | patch: 10 00 00 14

    {util::ModuleIndex::Main, 0x97F55C, "patch_easykill_01_movz"},            // MOVZ | W9
    {util::ModuleIndex::Main, 0x97F560, "patch_easykill_02_movk"},            // MOVK | W9
