#pragma once

#include <common.hpp>
#include <tuple>

#include "version.hpp"

namespace exl::reloc {
    using VersionType = util::UserVersion;

    template<VersionType Version, impl::LookupEntry... Entries>
    using UserTableType = VersionedTable<Version, Entries...>;

    using UserTableSet = TableSet<VersionType,
        /* DEFAULT is bound to D3CLIENT_VER via DetermineUserVersion(). */
        UserTableType<VersionType::DEFAULT,
            /* Early hooks (main module). */
            { util::ModuleIndex::Main, 0x000480, "hook_main_init" },
            { util::ModuleIndex::Main, 0x298BD0, "hook_gfx_init" },
            { util::ModuleIndex::Main, 0x4CABA0, "hook_game_common_data_init" },
            { util::ModuleIndex::Main, 0x667830, "hook_shell_initialize" },
            { util::ModuleIndex::Main, 0x7B08A0, "hook_sgame_initialize" },
            { util::ModuleIndex::Main, 0x8127A0, "hook_sinitialize_world" },

            /* Utility hooks (main module). */
            { util::ModuleIndex::Main, 0x758000, "hook_cmd_line_parse" },
            { util::ModuleIndex::Main, 0x0C8000, "hook_move_speed" },
            { util::ModuleIndex::Main, 0x6CC600, "hook_attack_speed" },
            { util::ModuleIndex::Main, 0x0BB5E8, "hook_floating_dmg" },
            { util::ModuleIndex::Main, 0x03FEE8, "hook_font_string_get_rendered_size" },
            { util::ModuleIndex::Main, 0x03FF50, "hook_font_string_draw_03ff50" },
            { util::ModuleIndex::Main, 0x03E5B0, "hook_font_string_draw_03e5b0" },
            { util::ModuleIndex::Main, 0x0010C8, "hook_font_string_draw_0010c8" },

            /* Season/event hooks (main module). */
            { util::ModuleIndex::Main, 0x0641F0, "hook_config_file_retrieved" },
            { util::ModuleIndex::Main, 0x065270, "hook_seasons_file_retrieved" },
            { util::ModuleIndex::Main, 0x0657F0, "hook_blacklist_file_retrieved" },

            /* Lobby hooks (main module). */
            { util::ModuleIndex::Main, 0x8950B0, "hook_specifiers_from_modifier" },
            { util::ModuleIndex::Main, 0x896808, "hook_eval_mod" },
            { util::ModuleIndex::Main, 0x495BC0, "hook_dupe_dropped_item" },
            { util::ModuleIndex::Main, 0x0BF630, "hook_request_drop_item" },

            /* Debugging hooks (main module). */
            { util::ModuleIndex::Main, 0x185AA0, "hook_print_challenge_rift_failed" },
            { util::ModuleIndex::Main, 0x185F70, "hook_challenge_rift_callback" },
            { util::ModuleIndex::Main, 0x06A2A8, "hook_pubfile_data_hex" },
            { util::ModuleIndex::Main, 0xA2AC60, "hook_print_error_display" },
            { util::ModuleIndex::Main, 0xA2AE74, "hook_print_error_string_final" },

            /* Globals (main module). */
            { util::ModuleIndex::Main, 0x17DE610, "main_rwindow" },
            { util::ModuleIndex::Main, 0x1830F68, "gfx_internal_data_ptr" },
            { util::ModuleIndex::Main, 0x1905610, "app_globals" },
            { util::ModuleIndex::Main, 0x191CA70, "sigma_thread_data" },
            { util::ModuleIndex::Main, 0x191EAD8, "attrib_defs" },
            { util::ModuleIndex::Main, 0x1154B68, "item_invalid" },
            { util::ModuleIndex::Main, 0x1A39CF0, "world_place_null" },
            { util::ModuleIndex::Main, 0x1A5F108, "refstring_data_buffer_nil" },

            /* Season/event request flags (main module). */
            { util::ModuleIndex::Main, 0x114AD48, "season_config_request_flag_ptr" },
            { util::ModuleIndex::Main, 0x114AD50, "season_seasons_request_flag_ptr" },
            { util::ModuleIndex::Main, 0x114AD68, "season_blacklist_request_flag_ptr" },

            /* Patch/event flags (main module). */
            { util::ModuleIndex::Main, 0x114ACB8, "event_buff_start_ptr" },
            { util::ModuleIndex::Main, 0x114ACC0, "event_buff_end_ptr" },
            { util::ModuleIndex::Main, 0x114AE38, "event_egg_flag_ptr" },
            { util::ModuleIndex::Main, 0x1154BF0, "event_igr_ptr" },
            { util::ModuleIndex::Main, 0x1154C00, "event_ann_ptr" },
            { util::ModuleIndex::Main, 0x1152CC8, "event_egg_xvar_ptr" },
            { util::ModuleIndex::Main, 0x114ACD8, "event_enabled_ptr" },
            { util::ModuleIndex::Main, 0x114ACE8, "event_season_only_ptr" },
            { util::ModuleIndex::Main, 0x18FFFC8, "event_double_keystones" },
            { util::ModuleIndex::Main, 0x1900080, "event_double_blood_shards" },
            { util::ModuleIndex::Main, 0x1900138, "event_double_goblins" },
            { util::ModuleIndex::Main, 0x19001F0, "event_double_bounty_bags" },
            { util::ModuleIndex::Main, 0x19002A8, "event_royal_grandeur" },
            { util::ModuleIndex::Main, 0x1900360, "event_legacy_of_nightmares" },
            { util::ModuleIndex::Main, 0x1900418, "event_triunes_will" },
            { util::ModuleIndex::Main, 0x19004D0, "event_pandemonium" },
            { util::ModuleIndex::Main, 0x1900588, "event_kanai_powers" },
            { util::ModuleIndex::Main, 0x1900640, "event_trials_of_tempests" },
            { util::ModuleIndex::Main, 0x19006F8, "event_shadow_clones" },
            { util::ModuleIndex::Main, 0x19007B0, "event_fourth_cube_slot" },
            { util::ModuleIndex::Main, 0x1900868, "event_ethereal_items" },
            { util::ModuleIndex::Main, 0x1900920, "event_soul_shards" },
            { util::ModuleIndex::Main, 0x19009D8, "event_swarm_rifts" },
            { util::ModuleIndex::Main, 0x1900A90, "event_sanctified_items" },
            { util::ModuleIndex::Main, 0x1900B48, "event_dark_alchemy" },
            { util::ModuleIndex::Main, 0x18FF848, "event_nesting_portals" },
            { util::ModuleIndex::Main, 0x1900C00, "event_paragon_cap" },

            /* XVar pointers (main module). */
            { util::ModuleIndex::Main, 0x115A408, "xvar_local_logging_enable_ptr" },
            { util::ModuleIndex::Main, 0x1158FB0, "xvar_online_service_ptr" },
            { util::ModuleIndex::Main, 0x1158FD0, "xvar_free_to_play_ptr" },
            { util::ModuleIndex::Main, 0x11590D8, "xvar_seasons_override_enabled_ptr" },
            { util::ModuleIndex::Main, 0x11590E0, "xvar_challenge_enabled_ptr" },
            { util::ModuleIndex::Main, 0x114AC60, "xvar_season_num_ptr" },
            { util::ModuleIndex::Main, 0x114AC68, "xvar_season_state_ptr" },
            { util::ModuleIndex::Main, 0x11551D0, "xvar_max_paragon_level_ptr" },
            { util::ModuleIndex::Main, 0x11597C8, "xvar_experimental_scheduling_ptr" }
        >
    >;
}
