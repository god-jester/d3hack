#include "program/gui2/ui/windows/config_window.hpp"

#include <algorithm>
#include <string>

#include "program/gui2/ui/overlay.hpp"
#include "program/gui2/ui/windows/notifications_window.hpp"
#include "program/runtime_apply.hpp"

namespace d3::gui2::ui::windows {
namespace {

static const char* SeasonMapModeToString(PatchConfig::SeasonEventMapMode mode) {
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

ConfigWindow::ConfigWindow(ui::Overlay& overlay)
    : Window("d3hack config"), overlay_(overlay) {
    SetDefaultPos(ImVec2(24.0f, 140.0f), ImGuiCond_Once);
    SetDefaultSize(ImVec2(740.0f, 480.0f), ImGuiCond_Once);
    SetFlags(ImGuiWindowFlags_NoSavedSettings);
}

bool* ConfigWindow::GetOpenFlag() {
    return overlay_.overlay_visible_ptr();
}

void ConfigWindow::AfterEnd() {
    if (show_metrics_) {
        ImGui::ShowMetricsWindow(&show_metrics_);
    }
}

void ConfigWindow::RenderContents() {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::TextUnformatted(overlay_.tr("gui.hotkey_toggle", "Hold + and - (0.5s) to toggle overlay visibility."));

    const auto& dbg = overlay_.frame_debug();
    ImGui::Text("Viewport: %.0fx%.0f | Crop: %dx%d | Swapchain: %d",
                dbg.viewport_size.x,
                dbg.viewport_size.y,
                dbg.crop_w,
                dbg.crop_h,
                dbg.swapchain_texture_count);
    ImGui::Text("ImGui: NavActive=%d NavVisible=%d",
                io.NavActive ? 1 : 0,
                io.NavVisible ? 1 : 0);
    ImGui::Text("NPAD: ok=%d buttons=0x%llx stickL=(%d,%d) stickR=(%d,%d)",
                dbg.last_npad_valid ? 1 : 0,
                dbg.last_npad_buttons,
                dbg.last_npad_stick_lx,
                dbg.last_npad_stick_ly,
                dbg.last_npad_stick_rx,
                dbg.last_npad_stick_ry);

    ImGui::Separator();

    if (!overlay_.is_config_loaded()) {
        ImGui::TextUnformatted(overlay_.tr("gui.config_not_loaded", "Config not loaded yet."));
        return;
    }

    PatchConfig& cfg = overlay_.ui_config();

    ImGui::Text("Config source: %s", global_config.defaults_only ? "built-in defaults" : "sd:/config/d3hack-nx/config.toml");

    if (overlay_.ui_dirty()) {
        ImGui::TextUnformatted(overlay_.tr("gui.ui_state_dirty", "UI state: UNSAVED changes"));
    } else {
        ImGui::TextUnformatted(overlay_.tr("gui.ui_state_clean", "UI state: clean"));
    }

    if (ImGui::Button(overlay_.tr("gui.reset_ui", "Reset UI to current config"))) {
        cfg = global_config;
        overlay_.set_overlay_visible(global_config.gui.visible);
        overlay_.set_ui_dirty(false);
    }

    ImGui::SameLine();
    if (ImGui::Checkbox(overlay_.tr("gui.gui_enabled", "GUI enabled"), &cfg.gui.enabled)) {
        overlay_.set_ui_dirty(true);
    }

    ImGui::SameLine();
    if (ImGui::Button(overlay_.tr("gui.apply", "Apply"))) {
        const PatchConfig normalized = NormalizePatchConfig(cfg);
        d3::RuntimeApplyResult apply{};
        d3::ApplyPatchConfigRuntime(normalized, &apply);

        cfg = global_config;
        overlay_.set_overlay_visible(global_config.gui.visible);
        overlay_.set_ui_dirty(false);

        auto* notifications = overlay_.notifications_window();
        if (apply.restart_required) {
            if (notifications != nullptr)
                notifications->AddNotification(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), 6.0f, "Applied. Restart required.");
        } else if (apply.applied_enable_only || apply.note[0] != '\0') {
            if (notifications != nullptr)
                notifications->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, "Applied (%s).", apply.note[0] ? apply.note : "runtime");
        } else {
            if (notifications != nullptr)
                notifications->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, "Applied.");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(overlay_.tr("gui.save", "Save"))) {
        std::string error;
        cfg.gui.visible = overlay_.overlay_visible();
        if (SavePatchConfigToPath("sd:/config/d3hack-nx/config.toml", cfg, error)) {
            if (auto* notifications = overlay_.notifications_window())
                notifications->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, "Saved config.toml");
        } else {
            if (auto* notifications = overlay_.notifications_window())
                notifications->AddNotification(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 8.0f, "Save failed: %s", error.empty() ? "unknown" : error.c_str());
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(overlay_.tr("gui.reload", "Reload"))) {
        PatchConfig loaded{};
        std::string error;
        if (LoadPatchConfigFromPath("sd:/config/d3hack-nx/config.toml", loaded, error)) {
            d3::RuntimeApplyResult apply{};
            d3::ApplyPatchConfigRuntime(loaded, &apply);
            cfg = global_config;
            overlay_.set_overlay_visible(global_config.gui.visible);
            overlay_.set_ui_dirty(false);
            if (auto* notifications = overlay_.notifications_window())
                notifications->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, "Reloaded config.toml");
        } else {
            if (auto* notifications = overlay_.notifications_window())
                notifications->AddNotification(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 8.0f, "Reload failed: %s", error.empty() ? "not found" : error.c_str());
        }
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

        if (ImGui::BeginTabItem("Overlays")) {
            mark_dirty(ImGui::Checkbox("Enabled##overlays", &cfg.overlays.active));
            ImGui::BeginDisabled(!cfg.overlays.active);
            mark_dirty(ImGui::Checkbox("Build locker watermark", &cfg.overlays.buildlocker_watermark));
            mark_dirty(ImGui::Checkbox("DDM labels", &cfg.overlays.ddm_labels));
            mark_dirty(ImGui::Checkbox("FPS label", &cfg.overlays.fps_label));
            mark_dirty(ImGui::Checkbox("Variable resolution label", &cfg.overlays.var_res_label));
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("GUI")) {
            mark_dirty(ImGui::Checkbox("Enabled (persist)", &cfg.gui.enabled));
            if (ImGui::Checkbox("Visible (persist)", &cfg.gui.visible)) {
                overlay_.set_overlay_visible_persist(cfg.gui.visible);
            }
            ImGui::TextUnformatted("Hotkey: hold + and - (0.5s) to toggle visibility.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Resolution")) {
            mark_dirty(ImGui::Checkbox("Enabled##res", &cfg.resolution_hack.active));
            ImGui::BeginDisabled(!cfg.resolution_hack.active);
            int target = static_cast<int>(cfg.resolution_hack.target_resolution);
            if (ImGui::SliderInt("Output target (vertical)", &target, 720, 1440, "%dp")) {
                cfg.resolution_hack.SetTargetRes(static_cast<u32>(target));
                overlay_.set_ui_dirty(true);
            }
            float min_scale = cfg.resolution_hack.min_res_scale;
            if (ImGui::SliderFloat("Minimum resolution scale", &min_scale, 10.0f, 100.0f, "%.0f%%")) {
                cfg.resolution_hack.min_res_scale = min_scale;
                overlay_.set_ui_dirty(true);
            }
            mark_dirty(ImGui::Checkbox("Clamp textures to 2048", &cfg.resolution_hack.clamp_textures_2048));
            mark_dirty(ImGui::Checkbox("Experimental scheduler", &cfg.resolution_hack.exp_scheduler));
            ImGui::Text("Output size: %ux%u", cfg.resolution_hack.OutputWidthPx(), cfg.resolution_hack.OutputHeightPx());
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Seasons")) {
            mark_dirty(ImGui::Checkbox("Enabled##seasons", &cfg.seasons.active));
            ImGui::BeginDisabled(!cfg.seasons.active);
            mark_dirty(ImGui::Checkbox("Allow online play", &cfg.seasons.allow_online));
            int season = static_cast<int>(cfg.seasons.current_season);
            if (ImGui::SliderInt("Current season", &season, 1, 200)) {
                cfg.seasons.current_season = static_cast<u32>(season);
                overlay_.set_ui_dirty(true);
            }
            mark_dirty(ImGui::Checkbox("Spoof PTR flag", &cfg.seasons.spoof_ptr));
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Events")) {
            mark_dirty(ImGui::Checkbox("Enabled##events", &cfg.events.active));

            const char* mode_label = SeasonMapModeToString(cfg.events.SeasonMapMode);
            if (ImGui::BeginCombo("Season map mode", mode_label)) {
                auto selectable_mode = [&](PatchConfig::SeasonEventMapMode mode) {
                    const bool is_selected = (cfg.events.SeasonMapMode == mode);
                    if (ImGui::Selectable(SeasonMapModeToString(mode), is_selected)) {
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
            mark_dirty(ImGui::Checkbox("IGR enabled", &cfg.events.IgrEnabled));
            mark_dirty(ImGui::Checkbox("Anniversary enabled", &cfg.events.AnniversaryEnabled));
            mark_dirty(ImGui::Checkbox("Easter egg world enabled", &cfg.events.EasterEggWorldEnabled));
            mark_dirty(ImGui::Checkbox("Double rift keystones", &cfg.events.DoubleRiftKeystones));
            mark_dirty(ImGui::Checkbox("Double blood shards", &cfg.events.DoubleBloodShards));
            mark_dirty(ImGui::Checkbox("Double treasure goblins", &cfg.events.DoubleTreasureGoblins));
            mark_dirty(ImGui::Checkbox("Double bounty bags", &cfg.events.DoubleBountyBags));
            mark_dirty(ImGui::Checkbox("Royal Grandeur", &cfg.events.RoyalGrandeur));
            mark_dirty(ImGui::Checkbox("Legacy of Nightmares", &cfg.events.LegacyOfNightmares));
            mark_dirty(ImGui::Checkbox("Triune's Will", &cfg.events.TriunesWill));
            mark_dirty(ImGui::Checkbox("Pandemonium", &cfg.events.Pandemonium));
            mark_dirty(ImGui::Checkbox("Kanai powers", &cfg.events.KanaiPowers));
            mark_dirty(ImGui::Checkbox("Trials of Tempests", &cfg.events.TrialsOfTempests));
            mark_dirty(ImGui::Checkbox("Shadow clones", &cfg.events.ShadowClones));
            mark_dirty(ImGui::Checkbox("Fourth Kanai's Cube slot", &cfg.events.FourthKanaisCubeSlot));
            mark_dirty(ImGui::Checkbox("Ethereal items", &cfg.events.EtherealItems));
            mark_dirty(ImGui::Checkbox("Soul shards", &cfg.events.SoulShards));
            mark_dirty(ImGui::Checkbox("Swarm rifts", &cfg.events.SwarmRifts));
            mark_dirty(ImGui::Checkbox("Sanctified items", &cfg.events.SanctifiedItems));
            mark_dirty(ImGui::Checkbox("Dark Alchemy", &cfg.events.DarkAlchemy));
            mark_dirty(ImGui::Checkbox("Nesting portals", &cfg.events.NestingPortals));
            ImGui::EndDisabled();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Rare cheats")) {
            mark_dirty(ImGui::Checkbox("Enabled##rare_cheats", &cfg.rare_cheats.active));
            ImGui::BeginDisabled(!cfg.rare_cheats.active);

            float move_speed = static_cast<float>(cfg.rare_cheats.move_speed);
            if (ImGui::SliderFloat("Move speed multiplier", &move_speed, 0.1f, 10.0f, "%.2fx")) {
                cfg.rare_cheats.move_speed = static_cast<double>(move_speed);
                overlay_.set_ui_dirty(true);
            }
            float attack_speed = static_cast<float>(cfg.rare_cheats.attack_speed);
            if (ImGui::SliderFloat("Attack speed multiplier", &attack_speed, 0.1f, 10.0f, "%.2fx")) {
                cfg.rare_cheats.attack_speed = static_cast<double>(attack_speed);
                overlay_.set_ui_dirty(true);
            }

            mark_dirty(ImGui::Checkbox("Floating damage color", &cfg.rare_cheats.floating_damage_color));
            mark_dirty(ImGui::Checkbox("Guaranteed legendaries", &cfg.rare_cheats.guaranteed_legendaries));
            mark_dirty(ImGui::Checkbox("Drop anything", &cfg.rare_cheats.drop_anything));
            mark_dirty(ImGui::Checkbox("Instant portal", &cfg.rare_cheats.instant_portal));
            mark_dirty(ImGui::Checkbox("No cooldowns", &cfg.rare_cheats.no_cooldowns));
            mark_dirty(ImGui::Checkbox("Instant craft actions", &cfg.rare_cheats.instant_craft_actions));
            mark_dirty(ImGui::Checkbox("Any gem any slot", &cfg.rare_cheats.any_gem_any_slot));
            mark_dirty(ImGui::Checkbox("Auto pickup", &cfg.rare_cheats.auto_pickup));
            mark_dirty(ImGui::Checkbox("Equip any slot", &cfg.rare_cheats.equip_any_slot));
            mark_dirty(ImGui::Checkbox("Unlock all difficulties", &cfg.rare_cheats.unlock_all_difficulties));
            mark_dirty(ImGui::Checkbox("Easy kill damage", &cfg.rare_cheats.easy_kill_damage));
            mark_dirty(ImGui::Checkbox("Cube no consume", &cfg.rare_cheats.cube_no_consume));
            mark_dirty(ImGui::Checkbox("Gem upgrade always", &cfg.rare_cheats.gem_upgrade_always));
            mark_dirty(ImGui::Checkbox("Gem upgrade speed", &cfg.rare_cheats.gem_upgrade_speed));
            mark_dirty(ImGui::Checkbox("Gem upgrade lvl150", &cfg.rare_cheats.gem_upgrade_lvl150));
            mark_dirty(ImGui::Checkbox("Equip multi legendary", &cfg.rare_cheats.equip_multi_legendary));

            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Challenge rifts")) {
            mark_dirty(ImGui::Checkbox("Enabled##cr", &cfg.challenge_rifts.active));
            ImGui::BeginDisabled(!cfg.challenge_rifts.active);
            mark_dirty(ImGui::Checkbox("Randomize within range", &cfg.challenge_rifts.random));
            int start = static_cast<int>(cfg.challenge_rifts.range_start);
            int end = static_cast<int>(cfg.challenge_rifts.range_end);
            if (ImGui::InputInt("Range start", &start)) {
                cfg.challenge_rifts.range_start = static_cast<u32>(std::max(0, start));
                overlay_.set_ui_dirty(true);
            }
            if (ImGui::InputInt("Range end", &end)) {
                cfg.challenge_rifts.range_end = static_cast<u32>(std::max(0, end));
                overlay_.set_ui_dirty(true);
            }
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Loot")) {
            mark_dirty(ImGui::Checkbox("Enabled##loot", &cfg.loot_modifiers.active));
            ImGui::BeginDisabled(!cfg.loot_modifiers.active);
            mark_dirty(ImGui::Checkbox("Disable ancient drops", &cfg.loot_modifiers.DisableAncientDrops));
            mark_dirty(ImGui::Checkbox("Disable primal ancient drops", &cfg.loot_modifiers.DisablePrimalAncientDrops));
            mark_dirty(ImGui::Checkbox("Disable torment drops", &cfg.loot_modifiers.DisableTormentDrops));
            mark_dirty(ImGui::Checkbox("Disable torment check", &cfg.loot_modifiers.DisableTormentCheck));
            mark_dirty(ImGui::Checkbox("Suppress gift generation", &cfg.loot_modifiers.SuppressGiftGeneration));

            int forced_ilevel = static_cast<int>(cfg.loot_modifiers.ForcedILevel);
            if (ImGui::SliderInt("Forced iLevel", &forced_ilevel, 0, 70)) {
                cfg.loot_modifiers.ForcedILevel = static_cast<u32>(forced_ilevel);
                overlay_.set_ui_dirty(true);
            }
            int tiered_level = static_cast<int>(cfg.loot_modifiers.TieredLootRunLevel);
            if (ImGui::SliderInt("Tiered loot run level", &tiered_level, 0, 150)) {
                cfg.loot_modifiers.TieredLootRunLevel = static_cast<u32>(tiered_level);
                overlay_.set_ui_dirty(true);
            }

            static constexpr const char* ranks[] = {"Normal", "Ancient", "Primal"};
            int rank_value = cfg.loot_modifiers.AncientRankValue;
            if (ImGui::Combo("Ancient rank", &rank_value, ranks, static_cast<int>(IM_ARRAYSIZE(ranks)))) {
                cfg.loot_modifiers.AncientRankValue = rank_value;
                overlay_.set_ui_dirty(true);
            }

            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Debug")) {
            mark_dirty(ImGui::Checkbox("Enabled##debug", &cfg.debug.active));
            ImGui::BeginDisabled(!cfg.debug.active);
            mark_dirty(ImGui::Checkbox("Enable crashes (danger)", &cfg.debug.enable_crashes));
            mark_dirty(ImGui::Checkbox("Enable pubfile dump", &cfg.debug.enable_pubfile_dump));
            mark_dirty(ImGui::Checkbox("Enable error traces", &cfg.debug.enable_error_traces));
            mark_dirty(ImGui::Checkbox("Enable debug flags", &cfg.debug.enable_debug_flags));
            ImGui::EndDisabled();

            ImGui::Separator();

            // Note: we don't link imgui_demo.cpp, so ShowDemoWindow() would be an unresolved symbol at runtime.
            bool show_demo = false;
            ImGui::BeginDisabled();
            ImGui::Checkbox("Show ImGui demo window (not linked)", &show_demo);
            ImGui::EndDisabled();
            ImGui::Checkbox("Show ImGui metrics", &show_metrics_);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

}  // namespace d3::gui2::ui::windows
