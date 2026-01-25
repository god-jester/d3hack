#include "program/gui2/ui/windows/config_window.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "program/d3/setting.hpp"
#include "program/gui2/imgui_overlay.hpp"
#include "program/gui2/ui/overlay.hpp"
#include "program/gui2/ui/windows/notifications_window.hpp"
#include "program/runtime_apply.hpp"

namespace d3::gui2::ui::windows {
    namespace {

        static auto SnapOutputTarget(int value) -> int {
            constexpr std::array<int, 6> kTargets       = {720, 900, 1080, 1440, 1800, 2160};
            constexpr int                kSnapThreshold = 20;
            int                          best           = kTargets.front();
            int                          best_dist      = std::abs(value - best);
            for (int target : kTargets) {
                int dist = std::abs(value - target);
                if (dist < best_dist) {
                    best      = target;
                    best_dist = dist;
                }
            }
            if (best_dist <= kSnapThreshold) {
                return best;
            }
            return value;
        }

        static auto SeasonMapModeToString(PatchConfig::SeasonEventMapMode mode) -> const char * {
            switch (mode) {
            case PatchConfig::SeasonEventMapMode::MapOnly:
                return "MapOnly";
            case PatchConfig::SeasonEventMapMode::OverlayConfig:
                return "OverlayConfig";
            case PatchConfig::SeasonEventMapMode::Disabled:
                return "Disabled";
            }
            return "Disabled";
        }

        static auto NormalizeStickComponent(int v) -> float {
            constexpr float kMax = 32767.0f;
            float           out  = static_cast<float>(v) / kMax;
            if (out < -1.0f)
                out = -1.0f;
            if (out > 1.0f)
                out = 1.0f;
            return out;
        }

    }  // namespace

    ConfigWindow::ConfigWindow(ui::Overlay &overlay) :
        Window(D3HACK_VER), overlay_(overlay) {
    }

    void ConfigWindow::BeforeBegin() {
        if (!overlay_.should_apply_default_layout()) {
            return;
        }

        ImVec2 pos(24.0f, 80.0f);
        ImVec2 size(760.0f, 520.0f);
        if (const ImGuiViewport *viewport = ImGui::GetMainViewport()) {
            pos  = ImVec2(viewport->Pos.x + 24.0f, viewport->Pos.y + 16.0f);
            size = ImVec2(760.0f, viewport->Size.y - 32.0f);
        }
        ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);

        const ImGuiID dockspace_id = overlay_.dockspace_id();
        if (dockspace_id != 0) {
            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
        }
    }

    void ConfigWindow::AfterEnd() {
        if (show_metrics_) {
            ImGui::ShowMetricsWindow(&show_metrics_);
        }
    }

    void ConfigWindow::UpdateDockSwipe(ImVec2 window_pos, ImVec2 window_size) {
        if (!overlay_.overlay_visible()) {
            dock_swipe_active_    = false;
            dock_swipe_triggered_ = false;
            return;
        }

        ImGuiIO &io = ImGui::GetIO();
        const bool down = io.MouseDown[ImGuiMouseButton_Left];
        if (!down) {
            dock_swipe_active_    = false;
            dock_swipe_triggered_ = false;
            return;
        }

        const ImGuiViewport *viewport = ImGui::GetWindowViewport();
        if (viewport == nullptr) {
            viewport = ImGui::GetMainViewport();
        }
        if (viewport == nullptr) {
            return;
        }

        const ImVec2 win_pos  = window_pos;
        const ImVec2 win_size = window_size;
        const float  left_dist = std::abs(win_pos.x - viewport->Pos.x);
        const float  right_dist =
            std::abs((viewport->Pos.x + viewport->Size.x) - (win_pos.x + win_size.x));
        const float  top_dist = std::abs(win_pos.y - viewport->Pos.y);
        const float  bottom_dist =
            std::abs((viewport->Pos.y + viewport->Size.y) - (win_pos.y + win_size.y));

        DockEdge nearest_edge = DockEdge::Left;
        float    nearest_dist = left_dist;
        if (right_dist < nearest_dist) {
            nearest_dist = right_dist;
            nearest_edge = DockEdge::Right;
        }
        if (top_dist < nearest_dist) {
            nearest_dist = top_dist;
            nearest_edge = DockEdge::Top;
        }
        if (bottom_dist < nearest_dist) {
            nearest_dist = bottom_dist;
            nearest_edge = DockEdge::Bottom;
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const ImVec2 pos = io.MousePos;
            const bool inside =
                pos.x >= win_pos.x && pos.x <= (win_pos.x + win_size.x) &&
                pos.y >= win_pos.y && pos.y <= (win_pos.y + win_size.y);
            bool in_edge = false;
            if (inside) {
                const float mid_x = win_pos.x + win_size.x * 0.5f;
                const float mid_y = win_pos.y + win_size.y * 0.5f;
                switch (nearest_edge) {
                case DockEdge::Left:
                    in_edge = (pos.x <= mid_x);
                    break;
                case DockEdge::Right:
                    in_edge = (pos.x >= mid_x);
                    break;
                case DockEdge::Top:
                    in_edge = (pos.y <= mid_y);
                    break;
                case DockEdge::Bottom:
                    in_edge = (pos.y >= mid_y);
                    break;
                }
            }
            dock_swipe_active_    = in_edge;
            dock_swipe_triggered_ = false;
            dock_swipe_edge_      = nearest_edge;
            dock_swipe_start_     = pos;
        }

        if (!dock_swipe_active_ || dock_swipe_triggered_) {
            return;
        }

        const ImVec2 delta(
            io.MousePos.x - dock_swipe_start_.x,
            io.MousePos.y - dock_swipe_start_.y
        );
        const float abs_x = std::abs(delta.x);
        const float abs_y = std::abs(delta.y);
        const float trigger_x = win_size.x * 0.20f;
        const float trigger_y = win_size.y * 0.20f;
        const float max_ortho_x = win_size.x * 0.25f;
        const float max_ortho_y = win_size.y * 0.25f;

        switch (dock_swipe_edge_) {
        case DockEdge::Left:
            if (abs_y > max_ortho_y) {
                return;
            }
            if (delta.x <= -trigger_x) {
                overlay_.set_overlay_visible_persist(false);
                dock_swipe_triggered_ = true;
            }
            break;
        case DockEdge::Right:
            if (abs_y > max_ortho_y) {
                return;
            }
            if (delta.x >= trigger_x) {
                overlay_.set_overlay_visible_persist(false);
                dock_swipe_triggered_ = true;
            }
            break;
        case DockEdge::Top:
            if (abs_x > max_ortho_x) {
                return;
            }
            if (delta.y <= -trigger_y) {
                overlay_.set_overlay_visible_persist(false);
                dock_swipe_triggered_ = true;
            }
            break;
        case DockEdge::Bottom:
            if (abs_x > max_ortho_x) {
                return;
            }
            if (delta.y >= trigger_y) {
                overlay_.set_overlay_visible_persist(false);
                dock_swipe_triggered_ = true;
            }
            break;
        }
    }

    void ConfigWindow::RenderContents() {
        const ImVec2 window_pos  = ImGui::GetWindowPos();
        const ImVec2 window_size = ImGui::GetWindowSize();
        ImGui::TextUnformatted(overlay_.tr("gui.hotkey_toggle", "Hold + and - (0.5s) to toggle overlay visibility."));

        if (!overlay_.is_config_loaded()) {
            ImGui::TextUnformatted(overlay_.tr("gui.config_not_loaded", "Config not loaded yet."));
            UpdateDockSwipe(window_pos, window_size);
            return;
        }

        PatchConfig &cfg = overlay_.ui_config();

        const char *config_source = global_config.defaults_only ? overlay_.tr("gui.config_source_defaults", "built-in defaults") : "sd:/config/d3hack-nx/config.toml";
        ImGui::Text(overlay_.tr("gui.config_source", "Config source: %s"), config_source);
        ImGui::Separator();

        if (!cfg.gui.enabled) {
            ImGui::TextUnformatted(overlay_.tr("gui.disabled_help", "GUI is disabled (proof-of-life stays on). Enable GUI to edit config."));
            UpdateDockSwipe(window_pos, window_size);
            return;
        }

        struct SectionInfo {
            const char *key;
            const char *fallback;
        };
        const std::array<SectionInfo, 9> kSections = {{
            {"gui.tab_overlays", "Overlays"},
            {"gui.tab_gui", "GUI"},
            {"gui.tab_resolution", "ResHack"},
            {"gui.tab_seasons", "Seasons"},
            {"gui.tab_events", "Events"},
            {"gui.tab_rare_cheats", "Cheats"},
            {"gui.tab_challenge_rifts", "Rifts"},
            {"gui.tab_loot", "Loot"},
            {"gui.tab_debug", "Debug"},
        }};

        section_index_ = std::clamp(section_index_, 0, static_cast<int>(kSections.size()) - 1);

        auto mark_dirty = [&](bool changed) -> void {
            if (changed) {
                overlay_.set_ui_dirty(true);
            }
        };

        auto render_overlays = [&]() -> void {
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.overlays_enabled", "Enabled##overlays"), &cfg.overlays.active));
            ImGui::BeginDisabled(!cfg.overlays.active);
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.overlays_buildlocker", "Build locker watermark"), &cfg.overlays.buildlocker_watermark));
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.overlays_ddm", "DDM labels"), &cfg.overlays.ddm_labels));
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.overlays_fps", "FPS label"), &cfg.overlays.fps_label));
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.overlays_var_res", "Variable resolution label"), &cfg.overlays.var_res_label));
            ImGui::EndDisabled();
        };

        auto render_gui = [&]() -> void {
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.enabled_persist", "Enabled (persist)"), &cfg.gui.enabled));
            if (ImGui::Checkbox(overlay_.tr("gui.visible_persist", "Visible (persist)"), &cfg.gui.visible)) {
                overlay_.set_overlay_visible_persist(cfg.gui.visible);
            }
            bool allow_left_stick_passthrough = cfg.gui.allow_left_stick_passthrough;
            if (ImGui::Checkbox(
                    overlay_.tr("gui.left_stick_passthrough", "Allow left stick passthrough"),
                    &allow_left_stick_passthrough
                )) {
                cfg.gui.allow_left_stick_passthrough = allow_left_stick_passthrough;
                overlay_.set_allow_left_stick_passthrough(allow_left_stick_passthrough);
                overlay_.set_ui_dirty(true);
            }

            const char *preview = cfg.gui.language_override.empty() ? overlay_.tr("gui.lang_auto", "Auto (game)") : cfg.gui.language_override.c_str();
            if (ImGui::BeginCombo(overlay_.tr("gui.language", "Language"), preview)) {
                struct Lang {
                    const char *label;
                    const char *code;
                };
                const std::array<Lang, 9> kLangs = {{
                    {.label = overlay_.tr("gui.lang_auto", "Auto (game)"),
                     .code  = ""},
                    {.label = overlay_.tr("gui.lang_en", "English"),
                     .code  = "en"},
                    {.label = overlay_.tr("gui.lang_de", "Deutsch"),
                     .code  = "de"},
                    {.label = overlay_.tr("gui.lang_fr", "Francais"),
                     .code  = "fr"},
                    {.label = overlay_.tr("gui.lang_es", "Espanol"),
                     .code  = "es"},
                    {.label = overlay_.tr("gui.lang_it", "Italiano"),
                     .code  = "it"},
                    {.label = overlay_.tr("gui.lang_ja", "Japanese"),
                     .code  = "ja"},
                    {.label = overlay_.tr("gui.lang_ko", "Korean"),
                     .code  = "ko"},
                    {.label = overlay_.tr("gui.lang_zh", "Chinese"),
                     .code  = "zh"},
                }};

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
        };

        auto render_resolution = [&]() -> void {
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.resolution_enabled", "Enabled##res"), &cfg.resolution_hack.active));
            ImGui::BeginDisabled(!cfg.resolution_hack.active);
            int target = static_cast<int>(cfg.resolution_hack.target_resolution);
            if (ImGui::SliderInt(overlay_.tr("gui.resolution_output_target", "Output target (vertical)"), &target, 720, 2160, "%dp")) {
                target = SnapOutputTarget(target);
                cfg.resolution_hack.SetTargetRes(static_cast<u32>(target));
                overlay_.set_ui_dirty(true);
            }
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.resolution_spoof_docked", "Spoof docked"), &cfg.resolution_hack.spoof_docked));
            bool handheld_auto = cfg.resolution_hack.output_handheld_scale <= 0.0f;
            if (ImGui::Checkbox(
                    overlay_.tr("gui.resolution_output_handheld_auto", "Handheld output scale: auto (stock)"),
                    &handheld_auto
                )) {
                if (handheld_auto) {
                    cfg.resolution_hack.output_handheld_scale = 0.0f;
                } else if (cfg.resolution_hack.output_handheld_scale <= 0.0f) {
                    cfg.resolution_hack.output_handheld_scale = 80.0f;
                }
                overlay_.set_ui_dirty(true);
            }
            ImGui::BeginDisabled(handheld_auto);
            float handheld_scale = cfg.resolution_hack.output_handheld_scale;
            if (ImGui::SliderFloat(
                    overlay_.tr("gui.resolution_output_handheld_scale", "Handheld output scale (%)"),
                    &handheld_scale,
                    PatchConfig::ResolutionHackConfig::kHandheldScaleMin,
                    PatchConfig::ResolutionHackConfig::kHandheldScaleMax,
                    "%.0f%%",
                    ImGuiSliderFlags_AlwaysClamp
                )) {
                handheld_scale = PatchConfig::ResolutionHackConfig::NormalizeHandheldScale(handheld_scale);
                if (handheld_scale != cfg.resolution_hack.output_handheld_scale) {
                    cfg.resolution_hack.output_handheld_scale = handheld_scale;
                    overlay_.set_ui_dirty(true);
                }
            }
            ImGui::EndDisabled();
            int handheld_target = static_cast<int>(cfg.resolution_hack.OutputHandheldHeightPx());
            ImGui::BeginDisabled();
            ImGui::InputInt(
                overlay_.tr("gui.resolution_output_target_handheld", "Handheld output target (vertical)"),
                &handheld_target,
                0,
                0,
                ImGuiInputTextFlags_ReadOnly
            );
            ImGui::EndDisabled();
            float min_scale   = cfg.resolution_hack.min_res_scale;
            float max_scale   = cfg.resolution_hack.max_res_scale;
            bool  scale_dirty = false;
            if (ImGui::SliderFloat(
                    overlay_.tr("gui.resolution_min_scale", "Minimum resolution scale"),
                    &min_scale,
                    10.0f,
                    100.0f,
                    "%.0f%%"
                )) {
                if (min_scale > max_scale)
                    max_scale = min_scale;
                scale_dirty = true;
            }
            if (ImGui::SliderFloat(
                    overlay_.tr("gui.resolution_max_scale", "Maximum resolution scale"),
                    &max_scale,
                    10.0f,
                    100.0f,
                    "%.0f%%"
                )) {
                if (max_scale < min_scale)
                    min_scale = max_scale;
                scale_dirty = true;
            }
            if (scale_dirty) {
                cfg.resolution_hack.min_res_scale = min_scale;
                cfg.resolution_hack.max_res_scale = max_scale;
                overlay_.set_ui_dirty(true);
            }
            int clamp_height = static_cast<int>(cfg.resolution_hack.clamp_texture_resolution);
            if (ImGui::InputInt(overlay_.tr("gui.resolution_clamp_texture", "Clamp texture height (0=off, 100-9999)"), &clamp_height)) {
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
            auto  edit_extra = [&](const char *label, s32 *value, int min_value, int max_value) -> void {
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
        };

        auto render_seasons = [&]() -> void {
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
        };

        auto render_events = [&]() -> void {
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
                auto selectable_mode = [&](PatchConfig::SeasonEventMapMode mode) -> void {
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
        };

        auto render_cheats = [&]() -> void {
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.rare_cheats_enabled", "Enabled##rare_cheats"), &cfg.rare_cheats.active));
            ImGui::BeginDisabled(!cfg.rare_cheats.active);

            auto move_speed =
                static_cast<float>(cfg.rare_cheats.move_speed);
            if (ImGui::SliderFloat(overlay_.tr("gui.rare_cheats_move_speed", "Move speed multiplier"), &move_speed, 0.1f, 10.0f, "%.2fx")) {
                cfg.rare_cheats.move_speed = static_cast<double>(move_speed);
                overlay_.set_ui_dirty(true);
            }
            auto attack_speed =
                static_cast<float>(cfg.rare_cheats.attack_speed);
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
        };

        auto render_rifts = [&]() -> void {
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
        };

        auto render_loot = [&]() -> void {
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_enabled", "Enabled##loot"), &cfg.loot_modifiers.active));
            ImGui::BeginDisabled(!cfg.loot_modifiers.active);
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_disable_ancient", "Disable ancient drops"), &cfg.loot_modifiers.DisableAncientDrops));
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_disable_primal", "Disable primal ancient drops"), &cfg.loot_modifiers.DisablePrimalAncientDrops));
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_disable_torment", "Disable torment drops"), &cfg.loot_modifiers.DisableTormentDrops));
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_disable_torment_check", "Disable torment check"), &cfg.loot_modifiers.DisableTormentCheck));
            mark_dirty(ImGui::Checkbox(overlay_.tr("gui.loot_suppress_gift", "Suppress gift generation"), &cfg.loot_modifiers.SuppressGiftGeneration));

            int forced_ilevel = cfg.loot_modifiers.ForcedILevel;
            if (ImGui::SliderInt(overlay_.tr("gui.loot_forced_ilevel", "Forced iLevel"), &forced_ilevel, 0, 70)) {
                cfg.loot_modifiers.ForcedILevel = forced_ilevel;
                overlay_.set_ui_dirty(true);
            }
            int tiered_level = cfg.loot_modifiers.TieredLootRunLevel;
            if (ImGui::SliderInt(overlay_.tr("gui.loot_tiered_run_level", "Tiered loot run level"), &tiered_level, 0, 150)) {
                cfg.loot_modifiers.TieredLootRunLevel = tiered_level;
                overlay_.set_ui_dirty(true);
            }

            const std::array<const char *, 3> ranks = {
                overlay_.tr("gui.loot_rank_normal", "Normal"),
                overlay_.tr("gui.loot_rank_ancient", "Ancient"),
                overlay_.tr("gui.loot_rank_primal", "Primal"),
            };
            int rank_value = cfg.loot_modifiers.AncientRankValue;
            if (ImGui::Combo(overlay_.tr("gui.loot_ancient_rank", "Ancient rank"), &rank_value, ranks.data(), static_cast<int>(ranks.size()))) {
                cfg.loot_modifiers.AncientRankValue = rank_value;
                overlay_.set_ui_dirty(true);
            }

            ImGui::EndDisabled();
        };

        auto render_debug = [&]() -> void {
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
        };

        const ImGuiStyle &style = ImGui::GetStyle();
        constexpr float   kNavMinWidth = 200.0f;
        constexpr float   kNavMaxWidth = 320.0f;
        constexpr float   kNavFontScale = 1.12f;
        constexpr float   kNavRowHeightScale = 1.45f;
        constexpr float   kNavExtraSpacingY = 6.0f;
        constexpr float   kNavExtraPaddingY = 4.0f;
        const float       nav_font_size = style.FontSizeBase * kNavFontScale;
        ImFont           *nav_font      = d3::imgui_overlay::GetTitleFont();
        const bool        has_nav_font  = (nav_font != nullptr);
        float             nav_text_w    = 0.0f;
        ImGui::PushFont(has_nav_font ? nav_font : nullptr, nav_font_size);
        for (const auto &section : kSections) {
            const char *label = overlay_.tr(section.key, section.fallback);
            nav_text_w         = std::max(nav_text_w, ImGui::CalcTextSize(label).x);
        }
        ImGui::PopFont();
        float nav_width = nav_text_w + style.FramePadding.x * 2.0f + style.ItemSpacing.x * 2.0f;
        const float avail_w = ImGui::GetContentRegionAvail().x;
        const float nav_max = std::max(kNavMinWidth, std::min(kNavMaxWidth, avail_w * 0.45f));
        nav_width           = std::clamp(nav_width, kNavMinWidth, nav_max);

        const float action_bar_height = ImGui::GetFrameHeightWithSpacing() + style.WindowPadding.y * 2.0f;

        ImGui::BeginChild("cfg_body", ImVec2(0.0f, -action_bar_height), false);
        ImGui::BeginChild("cfg_nav", ImVec2(nav_width, 0.0f), true);
        ImGui::PushFont(has_nav_font ? nav_font : nullptr, nav_font_size);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.ItemSpacing.y + kNavExtraSpacingY));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, style.FramePadding.y + kNavExtraPaddingY));
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
        const float nav_row_height = ImGui::GetTextLineHeightWithSpacing() * kNavRowHeightScale;
        for (int i = 0; i < static_cast<int>(kSections.size()); ++i) {
            const bool selected = (section_index_ == i);
            const char *label   = overlay_.tr(kSections[static_cast<size_t>(i)].key, kSections[static_cast<size_t>(i)].fallback);
            if (ImGui::Selectable(label, selected, 0, ImVec2(0.0f, nav_row_height))) {
                section_index_ = i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::PopStyleVar(3);
        ImGui::PopFont();
        ImGui::EndChild();

        ImGui::SameLine();

        auto apply_panel_scroll = [&]() -> void {
            ImGuiIO &io = ImGui::GetIO();
            const float line_height = ImGui::GetTextLineHeightWithSpacing();
            float       scroll_delta = 0.0f;

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                io.MouseWheel != 0.0f) {
                scroll_delta -= io.MouseWheel * line_height * 5.0f;
            }

            const auto &dbg = overlay_.frame_debug();
            const float ry = NormalizeStickComponent(dbg.last_npad_stick_ry);
            constexpr float kStickDeadzone = 0.25f;
            constexpr float kStickLinesPerSecond = 16.0f;
            const float abs_ry = std::abs(ry);
            if (abs_ry > kStickDeadzone) {
                const float t = (abs_ry - kStickDeadzone) / (1.0f - kStickDeadzone);
                const float speed = t * kStickLinesPerSecond * line_height;
                scroll_delta -= (ry > 0.0f ? 1.0f : -1.0f) * speed * io.DeltaTime;
            }

            if (scroll_delta != 0.0f) {
                const float max_scroll = ImGui::GetScrollMaxY();
                float       next = std::clamp(ImGui::GetScrollY() + scroll_delta, 0.0f, max_scroll);
                ImGui::SetScrollY(next);
            }
        };

        const ImGuiWindowFlags panel_flags =
            ImGuiWindowFlags_AlwaysVerticalScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::BeginChild("cfg_panel", ImVec2(0.0f, 0.0f), false, panel_flags);
        apply_panel_scroll();
        ImGui::TextUnformatted(overlay_.tr(kSections[static_cast<size_t>(section_index_)].key, kSections[static_cast<size_t>(section_index_)].fallback));
        ImGui::Separator();
        switch (section_index_) {
        case 0:
            render_overlays();
            break;
        case 1:
            render_gui();
            break;
        case 2:
            render_resolution();
            break;
        case 3:
            render_seasons();
            break;
        case 4:
            render_events();
            break;
        case 5:
            render_cheats();
            break;
        case 6:
            render_rifts();
            break;
        case 7:
            render_loot();
            break;
        case 8:
        default:
            render_debug();
            break;
        }
        ImGui::EndChild();
        ImGui::EndChild();

        ImGui::BeginChild("cfg_action_bar", ImVec2(0.0f, action_bar_height), true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::AlignTextToFramePadding();

        const char *label_reload = overlay_.tr("gui.reload", "Reload");
        const char *label_reset  = overlay_.tr("gui.reset_ui", "Revert Changes");
        const char *label_apply  = overlay_.tr("gui.apply", "Apply");
        const char *label_save   = overlay_.tr("gui.save", "Save");

        auto button_width = [&](const char *label) -> float {
            return ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
        };

        const float buttons_width =
            button_width(label_reset) + button_width(label_reload) + button_width(label_save) +
            button_width(label_apply) + style.ItemSpacing.x * 3.0f;

        const float start_x = ImGui::GetCursorPosX();
        const float avail_x = ImGui::GetContentRegionAvail().x;
        const float right_x = start_x + avail_x - buttons_width;
        bool        has_state = false;
        if (overlay_.ui_dirty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s",
                               overlay_.tr("gui.ui_state_dirty", "UI state: UNSAVED changes"));
            has_state = true;
        }
        if (restart_required_) {
            if (has_state) {
                ImGui::SameLine();
            }
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s",
                               overlay_.tr("gui.restart_required_banner", "Restart required for some changes."));
            has_state = true;
        }
        if (has_state) {
            ImGui::SameLine();
        }
        if (right_x > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(right_x);
        }

        if (ImGui::Button(label_reload)) {
            PatchConfig loaded {};
            std::string error;
            if (LoadPatchConfigFromPath("sd:/config/d3hack-nx/config.toml", loaded, error)) {
                d3::RuntimeApplyResult apply {};
                d3::ApplyPatchConfigRuntime(loaded, &apply);
                restart_required_ = apply.restart_required;
                cfg               = global_config;
                overlay_.set_overlay_visible(global_config.gui.visible);
                overlay_.set_allow_left_stick_passthrough(cfg.gui.allow_left_stick_passthrough);
                overlay_.set_ui_dirty(false);
                if (auto *notifications = overlay_.notifications_window())
                    notifications->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, overlay_.tr("gui.notify_reloaded", "Reloaded config.toml"));
            } else {
                if (auto *notifications = overlay_.notifications_window())
                    notifications->AddNotification(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 8.0f, overlay_.tr("gui.notify_reload_failed", "Reload failed: %s"), error.empty() ? overlay_.tr("gui.error_not_found", "not found") : error.c_str());
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(label_reset)) {
            cfg = global_config;
            overlay_.set_overlay_visible(global_config.gui.visible);
            overlay_.set_allow_left_stick_passthrough(cfg.gui.allow_left_stick_passthrough);
            overlay_.set_ui_dirty(false);
            restart_required_ = false;
        }

        ImGui::SameLine();
        if (ImGui::Button(label_apply)) {
            const PatchConfig      normalized = NormalizePatchConfig(cfg);
            d3::RuntimeApplyResult apply {};
            d3::ApplyPatchConfigRuntime(normalized, &apply);
            restart_required_ = apply.restart_required;

            cfg = global_config;
            overlay_.set_overlay_visible(global_config.gui.visible);
            overlay_.set_allow_left_stick_passthrough(cfg.gui.allow_left_stick_passthrough);
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
        if (ImGui::Button(label_save)) {
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

        ImGui::EndChild();

        UpdateDockSwipe(window_pos, window_size);
    }

}  // namespace d3::gui2::ui::windows
