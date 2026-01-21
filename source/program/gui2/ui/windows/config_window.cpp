#include "program/gui2/ui/windows/config_window.hpp"

#include <algorithm>
#include <string>

#include "program/gui2/ui/overlay.hpp"
#include "program/gui2/ui/windows/notifications_window.hpp"
#include "program/runtime_apply.hpp"

namespace d3::gui2::ui::windows {
    namespace {

        static const char *SeasonMapModeToString(PatchConfig::SeasonEventMapMode mode) {
            switch (mode) {
            case PatchConfig::SeasonEventMapMode::MapOnly:
                return "MapOnly";
            case PatchConfig::SeasonEventMapMode::OverlayConfig:
                return "OverlayConfig";
            case PatchConfig::SeasonEventMapMode::Disabled:
                return "Disabled";
            default:
                return "Disabled";
            }
        }

    }  // namespace

    ConfigWindow::ConfigWindow(ui::Overlay &overlay) :
        Window(D3HACK_VER), overlay_(overlay) {
        SetFlags(ImGuiWindowFlags_NoSavedSettings);
    }

    bool *ConfigWindow::GetOpenFlag() {
        return overlay_.overlay_visible_ptr();
    }

    void ConfigWindow::BeforeBegin() {
        ImVec2 pos(24.0f, 140.0f);
        ImVec2 size(740.0f, 480.0f);
        if (const ImGuiViewport *viewport = ImGui::GetMainViewport()) {
            pos  = ImVec2(viewport->Pos.x + 24.0f, viewport->Pos.y);
            size = ImVec2(740.0f, viewport->Size.y);
        }
        ImGui::SetNextWindowPos(pos, ImGuiCond_Once);
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
    }

    void ConfigWindow::AfterEnd() {
        if (show_metrics_) {
            ImGui::ShowMetricsWindow(&show_metrics_);
        }
    }

    void ConfigWindow::RenderContents() {
        ImGui::TextUnformatted(overlay_.tr("gui.hotkey_toggle", "Hold + and - (0.5s) to toggle overlay visibility."));

        if (!overlay_.is_config_loaded()) {
            ImGui::TextUnformatted(overlay_.tr("gui.config_not_loaded", "Config not loaded yet."));
            return;
        }

        PatchConfig &cfg = overlay_.ui_config();

        const char *config_source = global_config.defaults_only ? overlay_.tr("gui.config_source_defaults", "built-in defaults") : "sd:/config/d3hack-nx/config.toml";
        ImGui::Text(overlay_.tr("gui.config_source", "Config source: %s"), config_source);
        ImGui::Separator();

        const auto &dbg = overlay_.frame_debug();
        ImGui::Text(overlay_.tr("gui.viewport_info", "Viewport: %.0fx%.0f | Crop: %dx%d | Swapchain: %d"), dbg.viewport_size.x, dbg.viewport_size.y, dbg.crop_w, dbg.crop_h, dbg.swapchain_texture_count);

        if (overlay_.ui_dirty()) {
            ImGui::TextUnformatted(overlay_.tr("gui.ui_state_dirty", "UI state: UNSAVED changes"));
        } else {
            ImGui::TextUnformatted(overlay_.tr("gui.ui_state_clean", "UI state: clean"));
        }

        if (ImGui::Button(overlay_.tr("gui.reset_ui", "Reset UI to current config"))) {
            cfg = global_config;
            overlay_.set_overlay_visible(global_config.gui.visible);
            overlay_.set_ui_dirty(false);
            restart_required_ = false;
        }

        ImGui::SameLine();
        if (ImGui::Checkbox(overlay_.tr("gui.gui_enabled", "GUI enabled"), &cfg.gui.enabled)) {
            overlay_.set_ui_dirty(true);
        }

        ImGui::SameLine();
        if (ImGui::Button(overlay_.tr("gui.apply", "Apply"))) {
            const PatchConfig      normalized = NormalizePatchConfig(cfg);
            d3::RuntimeApplyResult apply {};
            d3::ApplyPatchConfigRuntime(normalized, &apply);
            restart_required_ = apply.restart_required;

            cfg = global_config;
            overlay_.set_overlay_visible(global_config.gui.visible);
            overlay_.set_ui_dirty(false);

            auto *notifications = overlay_.notifications_window();
            if (apply.restart_required) {
                if (notifications != nullptr)
                    notifications->AddNotification(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), 6.0f, overlay_.tr("gui.notify_applied_restart", "Applied. Restart required."));
            } else if (apply.applied_enable_only || apply.note[0] != '\0') {
                if (notifications != nullptr)
                    notifications->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, overlay_.tr("gui.notify_applied_note", "Applied (%s)."), apply.note[0] ? apply.note : overlay_.tr("gui.note_runtime", "runtime"));
            } else {
                if (notifications != nullptr)
                    notifications->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, overlay_.tr("gui.notify_applied", "Applied."));
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(overlay_.tr("gui.save", "Save"))) {
            std::string error;
            cfg.gui.visible = overlay_.overlay_visible();
            if (SavePatchConfigToPath("sd:/config/d3hack-nx/config.toml", cfg, error)) {
                if (auto *notifications = overlay_.notifications_window())
                    notifications->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, overlay_.tr("gui.notify_saved", "Saved config.toml"));
            } else {
                if (auto *notifications = overlay_.notifications_window())
                    notifications->AddNotification(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 8.0f, overlay_.tr("gui.notify_save_failed", "Save failed: %s"), error.empty() ? overlay_.tr("gui.error_unknown", "unknown") : error.c_str());
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(overlay_.tr("gui.reload", "Reload"))) {
            PatchConfig loaded {};
            std::string error;
            if (LoadPatchConfigFromPath("sd:/config/d3hack-nx/config.toml", loaded, error)) {
                d3::RuntimeApplyResult apply {};
                d3::ApplyPatchConfigRuntime(loaded, &apply);
                restart_required_ = apply.restart_required;
                cfg               = global_config;
                overlay_.set_overlay_visible(global_config.gui.visible);
                overlay_.set_ui_dirty(false);
                if (auto *notifications = overlay_.notifications_window())
                    notifications->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, overlay_.tr("gui.notify_reloaded", "Reloaded config.toml"));
            } else {
                if (auto *notifications = overlay_.notifications_window())
                    notifications->AddNotification(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 8.0f, overlay_.tr("gui.notify_reload_failed", "Reload failed: %s"), error.empty() ? overlay_.tr("gui.error_not_found", "not found") : error.c_str());
            }
        }

        if (restart_required_) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", overlay_.tr("gui.restart_required_banner", "Restart required for some changes."));
        }

        ImGui::Separator();

        if (!cfg.gui.enabled) {
            ImGui::TextUnformatted(overlay_.tr("gui.disabled_help", "GUI is disabled (proof-of-life stays on). Enable GUI to edit config."));
            return;
        }

        if (ImGui::BeginTabBar("d3hack_cfg_tabs")) {
            auto mark_dirty = [&](bool changed) {
                if (changed) {
                    overlay_.set_ui_dirty(true);
                }
            };

            if (ImGui::BeginTabItem(overlay_.tr("gui.tab_overlays", "Overlays"))) {
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.overlays_enabled", "Enabled##overlays"), &cfg.overlays.active));
                ImGui::BeginDisabled(!cfg.overlays.active);
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.overlays_buildlocker", "Build locker watermark"), &cfg.overlays.buildlocker_watermark));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.overlays_ddm", "DDM labels"), &cfg.overlays.ddm_labels));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.overlays_fps", "FPS label"), &cfg.overlays.fps_label));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.overlays_var_res", "Variable resolution label"), &cfg.overlays.var_res_label));
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(overlay_.tr("gui.tab_gui", "GUI"))) {
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.enabled_persist", "Enabled (persist)"), &cfg.gui.enabled));
                if (ImGui::Checkbox(overlay_.tr("gui.visible_persist", "Visible (persist)"), &cfg.gui.visible)) {
                    overlay_.set_overlay_visible_persist(cfg.gui.visible);
                }

                const char *preview = cfg.gui.language_override.empty() ? overlay_.tr("gui.lang_auto", "Auto (game)") : cfg.gui.language_override.c_str();
                if (ImGui::BeginCombo(overlay_.tr("gui.language", "Language"), preview)) {
                    struct Lang {
                        const char *label;
                        const char *code;
                    };
                    const Lang kLangs[] = {
                        {overlay_.tr("gui.lang_auto", "Auto (game)"), ""},
                        {overlay_.tr("gui.lang_en", "English"), "en"},
                        {overlay_.tr("gui.lang_de", "Deutsch"), "de"},
                        {overlay_.tr("gui.lang_fr", "Francais"), "fr"},
                        {overlay_.tr("gui.lang_es", "Espanol"), "es"},
                        {overlay_.tr("gui.lang_it", "Italiano"), "it"},
                        {overlay_.tr("gui.lang_ja", "Japanese"), "ja"},
                        {overlay_.tr("gui.lang_ko", "Korean"), "ko"},
                        {overlay_.tr("gui.lang_zh", "Chinese"), "zh"},
                    };

                    for (const auto &lang : kLangs) {
                        const bool is_selected = (cfg.gui.language_override == lang.code);
                        if (ImGui::Selectable(lang.label, is_selected)) {
                            cfg.gui.language_override = lang.code;
                            overlay_.set_ui_dirty(true);
                        }
                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::TextUnformatted(overlay_.tr("gui.hotkey_toggle", "Hold + and - (0.5s) to toggle overlay visibility."));
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(overlay_.tr("gui.tab_resolution", "Resolution"))) {
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.resolution_enabled", "Enabled##res"), &cfg.resolution_hack.active));
                ImGui::BeginDisabled(!cfg.resolution_hack.active);
                int target = static_cast<int>(cfg.resolution_hack.target_resolution);
                if (ImGui::SliderInt(overlay_.tr("gui.resolution_output_target", "Output target (vertical)"), &target, 720, 1440, "%dp")) {
                    cfg.resolution_hack.SetTargetRes(static_cast<u32>(target));
                    overlay_.set_ui_dirty(true);
                }
                float handheld_scale = cfg.resolution_hack.output_target_handheld;
                if (ImGui::InputFloat(overlay_.tr("gui.resolution_output_target_handheld", "Handheld output scale (0=auto)"), &handheld_scale, 0.05f, 0.1f, "%.2f")) {
                    handheld_scale                             = std::clamp(handheld_scale, 0.0f, 2.0f);
                    cfg.resolution_hack.output_target_handheld = handheld_scale;
                    overlay_.set_ui_dirty(true);
                }
                float min_scale = cfg.resolution_hack.min_res_scale;
                if (ImGui::SliderFloat(overlay_.tr("gui.resolution_min_scale", "Minimum resolution scale"), &min_scale, 10.0f, 100.0f, "%.0f%%")) {
                    cfg.resolution_hack.min_res_scale = min_scale;
                    overlay_.set_ui_dirty(true);
                }
                int clamp_height = static_cast<int>(cfg.resolution_hack.clamp_texture_resolution);
                if (ImGui::InputInt(overlay_.tr("gui.resolution_clamp_2048", "Clamp texture height (0=off, 100-9999)"), &clamp_height)) {
                    clamp_height = std::max(clamp_height, 0);
                    if (clamp_height != 0 && clamp_height < 100)
                        clamp_height = 100;
                    if (clamp_height > 9999)
                        clamp_height = 9999;
                    cfg.resolution_hack.clamp_texture_resolution = static_cast<u32>(clamp_height);
                    overlay_.set_ui_dirty(true);
                }
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.resolution_exp_scheduler", "Experimental scheduler"), &cfg.resolution_hack.exp_scheduler));
                ImGui::Text(overlay_.tr("gui.resolution_output_size", "Output size: %ux%u"), cfg.resolution_hack.OutputWidthPx(), cfg.resolution_hack.OutputHeightPx());
                ImGui::Separator();
                ImGui::TextUnformatted(overlay_.tr("gui.resolution_extra_header", "Display mode overrides"));
                ImGui::TextUnformatted(overlay_.tr("gui.resolution_extra_hint", "Set to -1 to keep the game's value."));

                auto &extra      = cfg.resolution_hack.extra;
                auto  edit_extra = [&](const char *label, s32 *value, int min_value, int max_value) {
                    int v = *value;
                    if (ImGui::InputInt(label, &v)) {
                        v      = std::clamp(v, min_value, max_value);
                        *value = static_cast<s32>(v);
                        overlay_.set_ui_dirty(true);
                    }
                };

                const int min_value = PatchConfig::ResolutionHackConfig::ExtraConfig::kUnset;
                edit_extra(
                    overlay_.tr("gui.resolution_extra_msaa", "MSAA level (-1=default)"), &extra.msaa_level,
                    min_value, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxMsaaLevel
                );
                edit_extra(
                    overlay_.tr("gui.resolution_extra_bit_depth", "Bit depth (-1=default)"), &extra.bit_depth,
                    min_value, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxBitDepth
                );
                edit_extra(
                    overlay_.tr("gui.resolution_extra_refresh", "Refresh rate (-1=default)"), &extra.refresh_rate,
                    min_value, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxRefreshRate
                );
                edit_extra(
                    overlay_.tr("gui.resolution_extra_win_left", "Window left (-1=default)"), &extra.window_left,
                    min_value, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension
                );
                edit_extra(
                    overlay_.tr("gui.resolution_extra_win_top", "Window top (-1=default)"), &extra.window_top,
                    min_value, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension
                );
                edit_extra(
                    overlay_.tr("gui.resolution_extra_win_width", "Window width (-1=default)"), &extra.window_width,
                    min_value, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension
                );
                edit_extra(
                    overlay_.tr("gui.resolution_extra_win_height", "Window height (-1=default)"), &extra.window_height,
                    min_value, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension
                );
                edit_extra(
                    overlay_.tr("gui.resolution_extra_ui_width", "UI width (-1=default)"), &extra.ui_opt_width,
                    min_value, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension
                );
                edit_extra(
                    overlay_.tr("gui.resolution_extra_ui_height", "UI height (-1=default)"), &extra.ui_opt_height,
                    min_value, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension
                );
                edit_extra(
                    overlay_.tr("gui.resolution_extra_render_width", "Render width (-1=default)"), &extra.render_width,
                    min_value, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension
                );
                edit_extra(
                    overlay_.tr("gui.resolution_extra_render_height", "Render height (-1=default)"), &extra.render_height,
                    min_value, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension
                );
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(overlay_.tr("gui.tab_seasons", "Seasons"))) {
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.seasons_enabled", "Enabled##seasons"), &cfg.seasons.active));
                ImGui::BeginDisabled(!cfg.seasons.active);
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.seasons_allow_online", "Allow online play"), &cfg.seasons.allow_online));
                int season = static_cast<int>(cfg.seasons.current_season);
                if (ImGui::SliderInt(overlay_.tr("gui.seasons_current", "Current season"), &season, 1, 200)) {
                    cfg.seasons.current_season = static_cast<u32>(season);
                    overlay_.set_ui_dirty(true);
                }
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.seasons_spoof_ptr", "Spoof PTR flag"), &cfg.seasons.spoof_ptr));
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(overlay_.tr("gui.tab_events", "Events"))) {
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_enabled", "Enabled##events"), &cfg.events.active));

                auto season_map_mode_label = [&](PatchConfig::SeasonEventMapMode mode) -> const char * {
                    switch (mode) {
                    case PatchConfig::SeasonEventMapMode::MapOnly:
                        return overlay_.tr("gui.events_mode_map_only", SeasonMapModeToString(mode));
                    case PatchConfig::SeasonEventMapMode::OverlayConfig:
                        return overlay_.tr("gui.events_mode_overlay", SeasonMapModeToString(mode));
                    case PatchConfig::SeasonEventMapMode::Disabled:
                    default:
                        return overlay_.tr("gui.events_mode_disabled", SeasonMapModeToString(mode));
                    }
                };

                const char *mode_label = season_map_mode_label(cfg.events.SeasonMapMode);
                if (ImGui::BeginCombo(overlay_.tr("gui.events_season_map_mode", "Season map mode"), mode_label)) {
                    auto selectable_mode = [&](PatchConfig::SeasonEventMapMode mode) {
                        const char *label       = season_map_mode_label(mode);
                        const bool  is_selected = (cfg.events.SeasonMapMode == mode);
                        if (ImGui::Selectable(label, is_selected)) {
                            cfg.events.SeasonMapMode = mode;
                            overlay_.set_ui_dirty(true);
                        }
                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    };
                    selectable_mode(PatchConfig::SeasonEventMapMode::MapOnly);
                    selectable_mode(PatchConfig::SeasonEventMapMode::OverlayConfig);
                    selectable_mode(PatchConfig::SeasonEventMapMode::Disabled);
                    ImGui::EndCombo();
                }

                ImGui::BeginDisabled(!cfg.events.active);
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_igr", "IGR enabled"), &cfg.events.IgrEnabled));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_anniversary", "Anniversary enabled"), &cfg.events.AnniversaryEnabled));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_easter_egg_world", "Easter egg world enabled"), &cfg.events.EasterEggWorldEnabled));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_double_keystones", "Double rift keystones"), &cfg.events.DoubleRiftKeystones));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_double_blood_shards", "Double blood shards"), &cfg.events.DoubleBloodShards));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_double_goblins", "Double treasure goblins"), &cfg.events.DoubleTreasureGoblins));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_double_bounty_bags", "Double bounty bags"), &cfg.events.DoubleBountyBags));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_royal_grandeur", "Royal Grandeur"), &cfg.events.RoyalGrandeur));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_legacy_of_nightmares", "Legacy of Nightmares"), &cfg.events.LegacyOfNightmares));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_triunes_will", "Triune's Will"), &cfg.events.TriunesWill));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_pandemonium", "Pandemonium"), &cfg.events.Pandemonium));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_kanai_powers", "Kanai powers"), &cfg.events.KanaiPowers));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_trials_of_tempests", "Trials of Tempests"), &cfg.events.TrialsOfTempests));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_shadow_clones", "Shadow clones"), &cfg.events.ShadowClones));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_fourth_kanais", "Fourth Kanai's Cube slot"), &cfg.events.FourthKanaisCubeSlot));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_ethereal_items", "Ethereal items"), &cfg.events.EtherealItems));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_soul_shards", "Soul shards"), &cfg.events.SoulShards));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_swarm_rifts", "Swarm rifts"), &cfg.events.SwarmRifts));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_sanctified_items", "Sanctified items"), &cfg.events.SanctifiedItems));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_dark_alchemy", "Dark Alchemy"), &cfg.events.DarkAlchemy));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.events_nesting_portals", "Nesting portals"), &cfg.events.NestingPortals));
                ImGui::EndDisabled();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(overlay_.tr("gui.tab_rare_cheats", "Rare cheats"))) {
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_enabled", "Enabled##rare_cheats"), &cfg.rare_cheats.active));
                ImGui::BeginDisabled(!cfg.rare_cheats.active);

                float move_speed = static_cast<float>(cfg.rare_cheats.move_speed);
                if (ImGui::SliderFloat(overlay_.tr("gui.rare_cheats_move_speed", "Move speed multiplier"), &move_speed, 0.1f, 10.0f, "%.2fx")) {
                    cfg.rare_cheats.move_speed = static_cast<double>(move_speed);
                    overlay_.set_ui_dirty(true);
                }
                float attack_speed = static_cast<float>(cfg.rare_cheats.attack_speed);
                if (ImGui::SliderFloat(overlay_.tr("gui.rare_cheats_attack_speed", "Attack speed multiplier"), &attack_speed, 0.1f, 10.0f, "%.2fx")) {
                    cfg.rare_cheats.attack_speed = static_cast<double>(attack_speed);
                    overlay_.set_ui_dirty(true);
                }

                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_floating_damage", "Floating damage color"), &cfg.rare_cheats.floating_damage_color));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_guaranteed_legendaries", "Guaranteed legendaries"), &cfg.rare_cheats.guaranteed_legendaries));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_drop_anything", "Drop anything"), &cfg.rare_cheats.drop_anything));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_instant_portal", "Instant portal"), &cfg.rare_cheats.instant_portal));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_no_cooldowns", "No cooldowns"), &cfg.rare_cheats.no_cooldowns));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_instant_craft", "Instant craft actions"), &cfg.rare_cheats.instant_craft_actions));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_any_gem_any_slot", "Any gem any slot"), &cfg.rare_cheats.any_gem_any_slot));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_auto_pickup", "Auto pickup"), &cfg.rare_cheats.auto_pickup));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_equip_any_slot", "Equip any slot"), &cfg.rare_cheats.equip_any_slot));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_unlock_all_difficulties", "Unlock all difficulties"), &cfg.rare_cheats.unlock_all_difficulties));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_easy_kill", "Easy kill damage"), &cfg.rare_cheats.easy_kill_damage));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_cube_no_consume", "Cube no consume"), &cfg.rare_cheats.cube_no_consume));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_gem_upgrade_always", "Gem upgrade always"), &cfg.rare_cheats.gem_upgrade_always));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_gem_upgrade_speed", "Gem upgrade speed"), &cfg.rare_cheats.gem_upgrade_speed));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_gem_upgrade_lvl150", "Gem upgrade lvl150"), &cfg.rare_cheats.gem_upgrade_lvl150));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_equip_multi_legendary", "Equip multi legendary"), &cfg.rare_cheats.equip_multi_legendary));

                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(overlay_.tr("gui.tab_challenge_rifts", "Challenge rifts"))) {
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.challenge_rifts_enabled", "Enabled##cr"), &cfg.challenge_rifts.active));
                ImGui::BeginDisabled(!cfg.challenge_rifts.active);
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.challenge_rifts_random", "Randomize within range"), &cfg.challenge_rifts.random));
                int start = static_cast<int>(cfg.challenge_rifts.range_start);
                int end   = static_cast<int>(cfg.challenge_rifts.range_end);
                if (ImGui::InputInt(overlay_.tr("gui.challenge_rifts_range_start", "Range start"), &start)) {
                    cfg.challenge_rifts.range_start = static_cast<u32>(std::max(0, start));
                    overlay_.set_ui_dirty(true);
                }
                if (ImGui::InputInt(overlay_.tr("gui.challenge_rifts_range_end", "Range end"), &end)) {
                    cfg.challenge_rifts.range_end = static_cast<u32>(std::max(0, end));
                    overlay_.set_ui_dirty(true);
                }
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(overlay_.tr("gui.tab_loot", "Loot"))) {
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_enabled", "Enabled##loot"), &cfg.loot_modifiers.active));
                ImGui::BeginDisabled(!cfg.loot_modifiers.active);
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_disable_ancient", "Disable ancient drops"), &cfg.loot_modifiers.DisableAncientDrops));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_disable_primal", "Disable primal ancient drops"), &cfg.loot_modifiers.DisablePrimalAncientDrops));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_disable_torment", "Disable torment drops"), &cfg.loot_modifiers.DisableTormentDrops));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_disable_torment_check", "Disable torment check"), &cfg.loot_modifiers.DisableTormentCheck));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_suppress_gift", "Suppress gift generation"), &cfg.loot_modifiers.SuppressGiftGeneration));

                int forced_ilevel = static_cast<int>(cfg.loot_modifiers.ForcedILevel);
                if (ImGui::SliderInt(overlay_.tr("gui.loot_forced_ilevel", "Forced iLevel"), &forced_ilevel, 0, 70)) {
                    cfg.loot_modifiers.ForcedILevel = static_cast<u32>(forced_ilevel);
                    overlay_.set_ui_dirty(true);
                }
                int tiered_level = static_cast<int>(cfg.loot_modifiers.TieredLootRunLevel);
                if (ImGui::SliderInt(overlay_.tr("gui.loot_tiered_run_level", "Tiered loot run level"), &tiered_level, 0, 150)) {
                    cfg.loot_modifiers.TieredLootRunLevel = static_cast<u32>(tiered_level);
                    overlay_.set_ui_dirty(true);
                }

                const char *ranks[] = {
                    overlay_.tr("gui.loot_rank_normal", "Normal"),
                    overlay_.tr("gui.loot_rank_ancient", "Ancient"),
                    overlay_.tr("gui.loot_rank_primal", "Primal"),
                };
                int rank_value = cfg.loot_modifiers.AncientRankValue;
                if (ImGui::Combo(overlay_.tr("gui.loot_ancient_rank", "Ancient rank"), &rank_value, ranks, static_cast<int>(IM_ARRAYSIZE(ranks)))) {
                    cfg.loot_modifiers.AncientRankValue = rank_value;
                    overlay_.set_ui_dirty(true);
                }

                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(overlay_.tr("gui.tab_debug", "Debug"))) {
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.debug_enabled", "Enabled##debug"), &cfg.debug.active));
                ImGui::BeginDisabled(!cfg.debug.active);
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.debug_enable_crashes", "Enable crashes (danger)"), &cfg.debug.enable_crashes));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.debug_pubfile_dump", "Enable pubfile dump"), &cfg.debug.enable_pubfile_dump));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.debug_error_traces", "Enable error traces"), &cfg.debug.enable_error_traces));
                mark_dirty(ImGui::Checkbox(overlay_.tr("gui.debug_debug_flags", "Enable debug flags"), &cfg.debug.enable_debug_flags));
                ImGui::EndDisabled();

                ImGui::Separator();

                // Note: we don't link imgui_demo.cpp, so ShowDemoWindow() would be an unresolved symbol at runtime.
                bool show_demo = false;
                ImGui::BeginDisabled();
                ImGui::Checkbox(overlay_.tr("gui.debug_show_demo", "Show ImGui demo window (not linked)"), &show_demo);
                ImGui::EndDisabled();
                ImGui::Checkbox(overlay_.tr("gui.debug_show_metrics", "Show ImGui metrics"), &show_metrics_);

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

}  // namespace d3::gui2::ui::windows
