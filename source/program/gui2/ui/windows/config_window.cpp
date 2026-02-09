#include "program/gui2/ui/windows/config_window.hpp"

#include <array>
#include <algorithm>
#include <cstdio>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <string>

#include "program/d3/setting.hpp"
#include "program/d3/resolution_util.hpp"
#include "program/config_schema.hpp"
#include "program/gui2/input_util.hpp"
#include "program/gui2/imgui_overlay.hpp"
#include "program/gui2/ui/overlay.hpp"
#include "program/gui2/ui/virtual_keyboard.hpp"
#include "program/gui2/ui/windows/notifications_window.hpp"
#include "program/runtime_apply.hpp"
#include "types.h"

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

    }  // namespace

    ConfigWindow::ConfigWindow(ui::Overlay &overlay) :
        Window(D3HACK_VER), overlay_(overlay) {
        SetFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
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

        ImGuiIO   &io   = ImGui::GetIO();
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

        const ImVec2 win_pos   = window_pos;
        const ImVec2 win_size  = window_size;
        const float  left_dist = std::abs(win_pos.x - viewport->Pos.x);
        const float  right_dist =
            std::abs((viewport->Pos.x + viewport->Size.x) - (win_pos.x + win_size.x));
        const float top_dist = std::abs(win_pos.y - viewport->Pos.y);
        const float bottom_dist =
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
            const bool   inside =
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
        const float abs_x       = std::abs(delta.x);
        const float abs_y       = std::abs(delta.y);
        const float trigger_x   = win_size.x * 0.20f;
        const float trigger_y   = win_size.y * 0.20f;
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

        if (!global_config.gui.enabled) {
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

        struct FormLayout {
            bool table_open = false;
        };

        static ImGuiTextFilter events_filter;
        static ImGuiTextFilter cheats_filter;
        static ImGuiTextFilter loot_filter;

        auto draw_restart_marker = [&](bool restart_required) -> void {
            if (!restart_required) {
                return;
            }
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextDisabled("%s", overlay_.tr("gui.restart_marker_short", "[R]"));
        };

        auto begin_form_layout = [&](const char *table_id, float max_label_width) -> FormLayout {
            FormLayout  layout {};
            const float avail_w      = ImGui::GetContentRegionAvail().x;
            const bool  long_label   = max_label_width > (avail_w * 0.48f);
            const bool  can_use_wide = avail_w >= 620.0f && !long_label;
            if (can_use_wide) {
                const float label_col_w = std::clamp(max_label_width + 28.0f, 220.0f, avail_w * 0.55f);
                if (ImGui::BeginTable(
                        table_id,
                        2,
                        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_NoSavedSettings
                    )) {
                    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, label_col_w);
                    ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    layout.table_open = true;
                }
            }
            return layout;
        };

        auto end_form_layout = [&](FormLayout &layout) -> void {
            if (layout.table_open) {
                ImGui::EndTable();
            }
        };

        auto begin_form_row = [&](FormLayout &layout, const char *label, bool restart_required) -> void {
            if (layout.table_open) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(label);
                draw_restart_marker(restart_required);
                ImGui::TableSetColumnIndex(1);
            } else {
                float row_start = ImGui::GetCursorPosX();
                float wrap_pos  = row_start + ImGui::GetContentRegionAvail().x;
                if (restart_required) {
                    const float marker_w = ImGui::CalcTextSize(overlay_.tr("gui.restart_marker_short", "[R]")).x;
                    wrap_pos             = std::max(row_start + 120.0f, wrap_pos - marker_w - 12.0f);
                }
                ImGui::AlignTextToFramePadding();
                ImGui::PushTextWrapPos(wrap_pos);
                ImGui::TextUnformatted(label);
                ImGui::PopTextWrapPos();
                draw_restart_marker(restart_required);
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
        };

        auto end_form_row = [&](FormLayout &layout) -> void {
            if (!layout.table_open) {
                ImGui::Spacing();
            }
        };

        auto checkbox_row = [&](FormLayout &layout, const char *label, const char *id, bool *value, bool restart_required) -> bool {
            begin_form_row(layout, label, restart_required);
            const bool changed = ImGui::Checkbox(id, value);
            end_form_row(layout);
            return changed;
        };

        auto slider_int_row = [&](FormLayout &layout, const char *label, const char *id, int *value, int min_value, int max_value, const char *fmt, ImGuiSliderFlags flags, bool restart_required) -> bool {
            begin_form_row(layout, label, restart_required);
            const bool changed = ImGui::SliderInt(id, value, min_value, max_value, fmt, flags);
            end_form_row(layout);
            return changed;
        };

        auto slider_float_row = [&](FormLayout &layout, const char *label, const char *id, float *value, float min_value, float max_value, const char *fmt, ImGuiSliderFlags flags, bool restart_required) -> bool {
            begin_form_row(layout, label, restart_required);
            const bool changed = ImGui::SliderFloat(id, value, min_value, max_value, fmt, flags);
            end_form_row(layout);
            return changed;
        };

        auto input_int_row = [&](FormLayout &layout, const char *label, const char *id, int *value, int step, int step_fast, ImGuiInputTextFlags flags, bool restart_required) -> bool {
            begin_form_row(layout, label, restart_required);
            const bool changed = ImGui::InputInt(id, value, step, step_fast, flags);
            end_form_row(layout);
            return changed;
        };

        auto combo_row = [&](FormLayout &layout, const char *label, const char *id, bool restart_required, auto &&draw_combo_body) -> bool {
            begin_form_row(layout, label, restart_required);
            const bool changed = draw_combo_body(id);
            end_form_row(layout);
            return changed;
        };

        auto readonly_int_row = [&](FormLayout &layout, const char *label, const char *id, int value) -> void {
            begin_form_row(layout, label, false);
            ImGui::BeginDisabled();
            ImGui::InputInt(id, &value, 0, 0, ImGuiInputTextFlags_ReadOnly);
            ImGui::EndDisabled();
            end_form_row(layout);
        };

        auto render_restart_legend = [&]() -> void {
            ImGui::TextDisabled("%s", overlay_.tr("gui.restart_marker_legend", "[R] restart required"));
        };

        auto render_subgroup_header = [&](const char *label_key, const char *fallback, bool top_of_section) -> void {
            if (!top_of_section) {
                ImGui::Spacing();
            }
            ImGui::SeparatorText(overlay_.tr(label_key, fallback));
        };

        auto label_passes_filter = [&](const ImGuiTextFilter &filter, const char *label) -> bool {
            return filter.IsActive() ? filter.PassFilter(label) : true;
        };

        struct BoolRowSpec {
            const char *tr_key;
            const char *fallback;
            const char *id;
            bool       *value;
            const bool *reset_value;
            bool        restart_required;
        };

        auto render_filter_and_bulk = [&](const char *filter_label, const char *filter_id, ImGuiTextFilter &filter, auto &&rows, bool disabled) -> bool {
            bool changed = false;
            ImGui::TextUnformatted(filter_label);
            ImGui::SameLine();
            filter.Draw(filter_id, ImGui::GetContentRegionAvail().x);
            const bool filter_active = filter.IsActive();
            if (filter_active) {
                ImGui::TextDisabled("%s", overlay_.tr("gui.filter_active", "Filter active"));
                ImGui::SameLine();
                ImGui::PushID(filter_id);
                if (ImGui::Button(overlay_.tr("gui.filter_clear", "Clear filter"))) {
                    filter.Clear();
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::BeginDisabled(disabled);
            const char *bulk_enable  = overlay_.tr("gui.bulk_enable_visible", "Enable visible");
            const char *bulk_disable = overlay_.tr("gui.bulk_disable_visible", "Disable visible");
            const char *bulk_reset   = overlay_.tr("gui.bulk_reset_visible", "Reset visible");

            if (ImGui::Button(bulk_enable)) {
                for (const BoolRowSpec &row : rows) {
                    const char *label = overlay_.tr(row.tr_key, row.fallback);
                    if (!label_passes_filter(filter, label)) {
                        continue;
                    }
                    if (!*row.value) {
                        *row.value = true;
                        changed    = true;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(bulk_disable)) {
                for (const BoolRowSpec &row : rows) {
                    const char *label = overlay_.tr(row.tr_key, row.fallback);
                    if (!label_passes_filter(filter, label)) {
                        continue;
                    }
                    if (*row.value) {
                        *row.value = false;
                        changed    = true;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(bulk_reset)) {
                for (const BoolRowSpec &row : rows) {
                    const char *label = overlay_.tr(row.tr_key, row.fallback);
                    if (!label_passes_filter(filter, label)) {
                        continue;
                    }
                    if (*row.value != *row.reset_value) {
                        *row.value = *row.reset_value;
                        changed    = true;
                    }
                }
            }
            ImGui::EndDisabled();
            return changed;
        };

        auto render_overlays = [&]() -> void {
            float max_label = ImGui::CalcTextSize(overlay_.tr("gui.overlays_enabled", "Enabled")).x;
            for (const auto &e : d3::config_schema::EntriesForSection("overlays")) {
                if (e.tr_label != nullptr && e.label_fallback != nullptr) {
                    max_label = std::max(max_label, ImGui::CalcTextSize(overlay_.tr(e.tr_label, e.label_fallback)).x);
                }
            }
            FormLayout layout = begin_form_layout("cfg_overlays_form", max_label);

            const auto *enabled_entry = d3::config_schema::FindEntry("overlays", "SectionEnabled");
            if (enabled_entry == nullptr || enabled_entry->get_bool == nullptr || enabled_entry->set_bool == nullptr) {
                const bool enabled_changed = checkbox_row(layout, overlay_.tr("gui.overlays_enabled", "Enabled"), "##overlays_enabled", &cfg.overlays.active, false);
                mark_dirty(enabled_changed);
                ImGui::BeginDisabled(!cfg.overlays.active);
                mark_dirty(checkbox_row(layout, overlay_.tr("gui.overlays_buildlocker", "Build locker watermark"), "##overlays_buildlocker", &cfg.overlays.buildlocker_watermark, true));
                mark_dirty(checkbox_row(layout, overlay_.tr("gui.overlays_ddm", "DDM labels"), "##overlays_ddm", &cfg.overlays.ddm_labels, true));
                mark_dirty(checkbox_row(layout, overlay_.tr("gui.overlays_fps", "FPS label"), "##overlays_fps", &cfg.overlays.fps_label, false));
                mark_dirty(checkbox_row(layout, overlay_.tr("gui.overlays_var_res", "Variable resolution label"), "##overlays_var_res", &cfg.overlays.var_res_label, false));
                ImGui::EndDisabled();
                end_form_layout(layout);
                render_restart_legend();
                return;
            }

            bool enabled = enabled_entry->get_bool(cfg);
            if (checkbox_row(layout, overlay_.tr(enabled_entry->tr_label, enabled_entry->label_fallback), "##overlays_enabled", &enabled, enabled_entry->restart == d3::config_schema::RestartPolicy::RestartRequired)) {
                enabled_entry->set_bool(cfg, enabled);
                overlay_.set_ui_dirty(true);
            }

            ImGui::BeginDisabled(!enabled);
            for (const auto &e : d3::config_schema::EntriesForSection("overlays")) {
                if (e.key == "SectionEnabled") {
                    continue;
                }
                if (e.kind != d3::config_schema::ValueKind::Bool || e.get_bool == nullptr || e.set_bool == nullptr) {
                    continue;
                }
                bool        v  = e.get_bool(cfg);
                std::string id = "##overlays_";
                id += e.key;
                if (checkbox_row(layout, overlay_.tr(e.tr_label, e.label_fallback), id.c_str(), &v, e.restart == d3::config_schema::RestartPolicy::RestartRequired)) {
                    e.set_bool(cfg, v);
                    overlay_.set_ui_dirty(true);
                }
            }
            ImGui::EndDisabled();
            end_form_layout(layout);
            render_restart_legend();
        };

        auto render_gui = [&]() -> void {
            float max_label = 0.0f;
            for (const auto &e : d3::config_schema::EntriesForSection("gui")) {
                if (e.kind == d3::config_schema::ValueKind::Bool && e.tr_label != nullptr && e.label_fallback != nullptr) {
                    max_label = std::max(max_label, ImGui::CalcTextSize(overlay_.tr(e.tr_label, e.label_fallback)).x);
                }
            }
            max_label         = std::max(max_label, ImGui::CalcTextSize(overlay_.tr("gui.language", "Language")).x);
            FormLayout layout = begin_form_layout("cfg_gui_form", max_label);
            for (const auto &e : d3::config_schema::EntriesForSection("gui")) {
                if (e.kind != d3::config_schema::ValueKind::Bool || e.get_bool == nullptr || e.set_bool == nullptr) {
                    continue;
                }
                bool        v  = e.get_bool(cfg);
                std::string id = "##gui_";
                id += e.key;
                if (checkbox_row(layout, overlay_.tr(e.tr_label, e.label_fallback), id.c_str(), &v, e.restart == d3::config_schema::RestartPolicy::RestartRequired)) {
                    e.set_bool(cfg, v);
                    overlay_.set_ui_dirty(true);
                }
            }

            const auto &langs         = overlay_.translations_metadata();
            const auto *selected_lang = [&]() -> const d3::gui2::ui::TranslationLanguage * {
                for (const auto &lang : langs) {
                    if (lang.code == cfg.gui.language_override) {
                        return &lang;
                    }
                }
                return nullptr;
            }();

            const char *preview = nullptr;
            if (selected_lang != nullptr) {
                preview = overlay_.tr(selected_lang->label_key.c_str(), selected_lang->label_fallback.c_str());
            } else if (cfg.gui.language_override.empty()) {
                preview = overlay_.tr("gui.lang_auto", "Auto (game)");
            } else {
                preview = cfg.gui.language_override.c_str();
            }

            mark_dirty(combo_row(
                layout,
                overlay_.tr("gui.language", "Language"),
                "##gui_language",
                true,
                [&](const char *id) -> bool {
                    bool changed = false;
                    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
                    if (ImGui::BeginCombo(id, preview)) {
                        for (const auto &lang : langs) {
                            const char *label       = overlay_.tr(lang.label_key.c_str(), lang.label_fallback.c_str());
                            const bool  is_selected = (cfg.gui.language_override == lang.code);
                            if (ImGui::Selectable(label, is_selected)) {
                                cfg.gui.language_override = lang.code;
                                changed                   = true;
                                lang_restart_pending_     = cfg.gui.language_override != global_config.gui.language_override;
                                if (auto *notifications = overlay_.notifications_window()) {
                                    notifications->AddNotification(
                                        ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
                                        6.0f,
                                        overlay_.tr("gui.notify_lang_restart", "Language updated. Restart to fully apply new glyphs.")
                                    );
                                }
                            }
                            if (is_selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    return changed;
                }
            ));

            end_form_layout(layout);
            render_restart_legend();

            ImGui::Separator();
            const char *lang_value = cfg.gui.language_override.empty() ? overlay_.tr("gui.lang_auto", "Auto (game)") : cfg.gui.language_override.c_str();
            ImGui::Text(overlay_.tr("gui.language_current", "Language code: %s"), lang_value);

            if (ImGui::Button(overlay_.tr("gui.language_edit", "Edit language code (OSK)"))) {
                d3::gui2::ui::virtual_keyboard::Options opts {};
                opts.max_len       = 16;
                opts.allowed_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
                opts.allow_space   = false;
                d3::gui2::ui::virtual_keyboard::Open(
                    overlay_.tr("gui.language_edit_title", "Enter language code (e.g. en, ja, zh)"),
                    &cfg.gui.language_override,
                    opts
                );
            }
            ImGui::SameLine();
            if (ImGui::Button(overlay_.tr("gui.language_clear", "Clear"))) {
                cfg.gui.language_override.clear();
                overlay_.set_ui_dirty(true);
                lang_restart_pending_ = cfg.gui.language_override != global_config.gui.language_override;
            }
            ImGui::TextUnformatted(overlay_.tr("gui.hotkey_toggle", "Hold + and - (0.5s) to toggle overlay visibility."));
        };

        auto render_resolution = [&]() -> void {
            const float general_max_label = std::max(
                ImGui::CalcTextSize(overlay_.tr("gui.resolution_spoof_docked", "Spoof docked")).x,
                ImGui::CalcTextSize(overlay_.tr("gui.resolution_exp_scheduler", "Experimental scheduler")).x
            );
            const float output_max_label = std::max(
                ImGui::CalcTextSize(overlay_.tr("gui.resolution_output_target_handheld", "Handheld output target (vertical)")).x,
                ImGui::CalcTextSize(overlay_.tr("gui.resolution_output_handheld_auto", "Handheld output scale: auto (stock)")).x
            );
            const float scale_max_label = std::max(
                ImGui::CalcTextSize(overlay_.tr("gui.resolution_min_scale", "Minimum resolution scale")).x,
                ImGui::CalcTextSize(overlay_.tr("gui.resolution_max_scale", "Maximum resolution scale")).x
            );
            const float clamp_max_label = ImGui::CalcTextSize(overlay_.tr("gui.resolution_clamp_texture", "Clamp texture height (0=off, 100-9999)")).x;

            render_subgroup_header("gui.resolution_group_general", "General", true);
            FormLayout general_layout = begin_form_layout("cfg_resolution_general_form", general_max_label);
            mark_dirty(checkbox_row(general_layout, overlay_.tr("gui.resolution_enabled", "Enabled"), "##res_enabled", &cfg.resolution_hack.active, true));
            ImGui::BeginDisabled(!cfg.resolution_hack.active);
            mark_dirty(checkbox_row(general_layout, overlay_.tr("gui.resolution_exp_scheduler", "Experimental scheduler"), "##res_exp_scheduler", &cfg.resolution_hack.exp_scheduler, false));
            ImGui::EndDisabled();
            end_form_layout(general_layout);

            ImGui::BeginDisabled(!cfg.resolution_hack.active);
            render_subgroup_header("gui.resolution_group_output", "Output Targeting", false);
            FormLayout output_layout = begin_form_layout("cfg_resolution_output_form", output_max_label);
            const int  max_target    = static_cast<int>(d3::MaxResolutionHackOutputTarget());
            int        target        = static_cast<int>(cfg.resolution_hack.target_resolution);
            if (target > max_target) {
                target = max_target;
                cfg.resolution_hack.SetTargetRes(static_cast<u32>(target));
                overlay_.set_ui_dirty(true);
            }
            if (slider_int_row(output_layout, overlay_.tr("gui.resolution_output_target", "Output target (vertical)"), "##res_output_target", &target, 720, max_target, "%dp", 0, true)) {
                target = SnapOutputTarget(target);
                cfg.resolution_hack.SetTargetRes(static_cast<u32>(target));
                overlay_.set_ui_dirty(true);
            }
            mark_dirty(checkbox_row(output_layout, overlay_.tr("gui.resolution_spoof_docked", "Spoof docked"), "##res_spoof_docked", &cfg.resolution_hack.spoof_docked, true));
            bool handheld_auto = cfg.resolution_hack.output_handheld_scale <= 0.0f;
            if (checkbox_row(output_layout, overlay_.tr("gui.resolution_output_handheld_auto", "Handheld output scale: auto (stock)"), "##res_handheld_auto", &handheld_auto, true)) {
                if (handheld_auto) {
                    cfg.resolution_hack.output_handheld_scale = 0.0f;
                } else if (cfg.resolution_hack.output_handheld_scale <= 0.0f) {
                    cfg.resolution_hack.output_handheld_scale = 80.0f;
                }
                overlay_.set_ui_dirty(true);
            }
            ImGui::BeginDisabled(handheld_auto);
            float handheld_scale = cfg.resolution_hack.output_handheld_scale;
            if (slider_float_row(output_layout, overlay_.tr("gui.resolution_output_handheld_scale", "Handheld output scale (%)"), "##res_handheld_scale", &handheld_scale, PatchConfig::ResolutionHackConfig::kHandheldScaleMin, PatchConfig::ResolutionHackConfig::kHandheldScaleMax, "%.0f%%", ImGuiSliderFlags_AlwaysClamp, true)) {
                handheld_scale = PatchConfig::ResolutionHackConfig::NormalizeHandheldScale(handheld_scale);
                if (handheld_scale != cfg.resolution_hack.output_handheld_scale) {
                    cfg.resolution_hack.output_handheld_scale = handheld_scale;
                    overlay_.set_ui_dirty(true);
                }
            }
            ImGui::EndDisabled();
            readonly_int_row(output_layout, overlay_.tr("gui.resolution_output_target_handheld", "Handheld output target (vertical)"), "##res_output_target_handheld", static_cast<int>(cfg.resolution_hack.OutputHandheldHeightPx()));
            end_form_layout(output_layout);

            render_subgroup_header("gui.resolution_group_scale", "Dynamic Scale Bounds", false);
            FormLayout scale_layout = begin_form_layout("cfg_resolution_scale_form", scale_max_label);
            float      min_scale    = cfg.resolution_hack.min_res_scale;
            float      max_scale    = cfg.resolution_hack.max_res_scale;
            bool       scale_dirty  = false;
            if (slider_float_row(scale_layout, overlay_.tr("gui.resolution_min_scale", "Minimum resolution scale"), "##res_min_scale", &min_scale, 10.0f, 100.0f, "%.0f%%", 0, true)) {
                if (min_scale > max_scale) {
                    max_scale = min_scale;
                }
                scale_dirty = true;
            }
            if (slider_float_row(scale_layout, overlay_.tr("gui.resolution_max_scale", "Maximum resolution scale"), "##res_max_scale", &max_scale, 10.0f, 100.0f, "%.0f%%", 0, true)) {
                if (max_scale < min_scale) {
                    min_scale = max_scale;
                }
                scale_dirty = true;
            }
            if (scale_dirty) {
                cfg.resolution_hack.min_res_scale = min_scale;
                cfg.resolution_hack.max_res_scale = max_scale;
                overlay_.set_ui_dirty(true);
            }
            end_form_layout(scale_layout);

            render_subgroup_header("gui.resolution_group_texture", "Texture Clamp", false);
            FormLayout clamp_layout = begin_form_layout("cfg_resolution_clamp_form", clamp_max_label);
            int        clamp_height = static_cast<int>(cfg.resolution_hack.clamp_texture_resolution);
            if (input_int_row(clamp_layout, overlay_.tr("gui.resolution_clamp_texture", "Clamp texture height (0=off, 100-9999)"), "##res_clamp_texture", &clamp_height, 1, 100, 0, true)) {
                clamp_height = std::max(clamp_height, 0);
                if (clamp_height != 0 && clamp_height < 100) {
                    clamp_height = 100;
                }
                if (clamp_height > 9999) {
                    clamp_height = 9999;
                }
                cfg.resolution_hack.clamp_texture_resolution = static_cast<u32>(clamp_height);
                overlay_.set_ui_dirty(true);
            }
            end_form_layout(clamp_layout);

            render_restart_legend();

            ImGui::Text(overlay_.tr("gui.resolution_output_size", "Output size: %ux%u"), cfg.resolution_hack.OutputWidthPx(), cfg.resolution_hack.OutputHeightPx());

            render_subgroup_header("gui.resolution_group_display_overrides", "Display Overrides (Advanced)", false);
            if (ImGui::CollapsingHeader(overlay_.tr("gui.resolution_extra_header", "Display mode overrides"), ImGuiTreeNodeFlags_None)) {
                ImGui::TextDisabled("%s", overlay_.tr("gui.resolution_extra_hint", "Set to -1 to keep the game's value."));
                const float extra_max_label = ImGui::CalcTextSize(overlay_.tr("gui.resolution_extra_render_height", "Render height (-1=default)")).x;
                FormLayout  extra_layout    = begin_form_layout("cfg_resolution_extra_form", extra_max_label);
                auto       &extra           = cfg.resolution_hack.extra;
                const int   min_value       = PatchConfig::ResolutionHackConfig::ExtraConfig::kUnset;
                auto        edit_extra      = [&](const char *label, const char *id, s32 *value, int max_value) -> void {
                    int v = *value;
                    if (input_int_row(extra_layout, label, id, &v, 1, 100, 0, true)) {
                        v      = std::clamp(v, min_value, max_value);
                        *value = static_cast<s32>(v);
                        overlay_.set_ui_dirty(true);
                    }
                };

                edit_extra(overlay_.tr("gui.resolution_extra_msaa", "MSAA level (-1=default)"), "##res_extra_msaa", &extra.msaa_level, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxMsaaLevel);
                edit_extra(overlay_.tr("gui.resolution_extra_bit_depth", "Bit depth (-1=default)"), "##res_extra_bit_depth", &extra.bit_depth, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxBitDepth);
                edit_extra(overlay_.tr("gui.resolution_extra_refresh", "Refresh rate (-1=default)"), "##res_extra_refresh", &extra.refresh_rate, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxRefreshRate);
                edit_extra(overlay_.tr("gui.resolution_extra_win_left", "Window left (-1=default)"), "##res_extra_win_left", &extra.window_left, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension);
                edit_extra(overlay_.tr("gui.resolution_extra_win_top", "Window top (-1=default)"), "##res_extra_win_top", &extra.window_top, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension);
                edit_extra(overlay_.tr("gui.resolution_extra_win_width", "Window width (-1=default)"), "##res_extra_win_width", &extra.window_width, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension);
                edit_extra(overlay_.tr("gui.resolution_extra_win_height", "Window height (-1=default)"), "##res_extra_win_height", &extra.window_height, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension);
                edit_extra(overlay_.tr("gui.resolution_extra_ui_width", "UI width (-1=default)"), "##res_extra_ui_width", &extra.ui_opt_width, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension);
                edit_extra(overlay_.tr("gui.resolution_extra_ui_height", "UI height (-1=default)"), "##res_extra_ui_height", &extra.ui_opt_height, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension);
                edit_extra(overlay_.tr("gui.resolution_extra_render_width", "Render width (-1=default)"), "##res_extra_render_width", &extra.render_width, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension);
                edit_extra(overlay_.tr("gui.resolution_extra_render_height", "Render height (-1=default)"), "##res_extra_render_height", &extra.render_height, PatchConfig::ResolutionHackConfig::ExtraConfig::kMaxDimension);
                end_form_layout(extra_layout);
                render_restart_legend();
            }
            ImGui::EndDisabled();
        };

        auto render_seasons = [&]() -> void {
            const float max_label = ImGui::CalcTextSize(overlay_.tr("gui.seasons_spoof_ptr", "Spoof PTR flag")).x;
            FormLayout  layout    = begin_form_layout("cfg_seasons_form", max_label);
            mark_dirty(checkbox_row(layout, overlay_.tr("gui.seasons_enabled", "Enabled"), "##seasons_enabled", &cfg.seasons.active, false));
            ImGui::BeginDisabled(!cfg.seasons.active);
            mark_dirty(checkbox_row(layout, overlay_.tr("gui.seasons_allow_online", "Allow online play"), "##seasons_allow_online", &cfg.seasons.allow_online, false));
            int season = static_cast<int>(cfg.seasons.current_season);
            if (slider_int_row(layout, overlay_.tr("gui.seasons_current", "Current season"), "##seasons_current", &season, 1, 200, "%d", 0, false)) {
                cfg.seasons.current_season = static_cast<u32>(season);
                overlay_.set_ui_dirty(true);
            }
            mark_dirty(checkbox_row(layout, overlay_.tr("gui.seasons_spoof_ptr", "Spoof PTR flag"), "##seasons_spoof_ptr", &cfg.seasons.spoof_ptr, false));
            ImGui::EndDisabled();
            end_form_layout(layout);
        };

        auto render_events = [&]() -> void {
            ImGui::TextUnformatted(overlay_.tr("gui.events_group_core", "Core controls"));
            const float core_max_label = ImGui::CalcTextSize(overlay_.tr("gui.events_season_map_mode", "Season map mode")).x;
            FormLayout  core_layout    = begin_form_layout("cfg_events_core_form", core_max_label);
            mark_dirty(checkbox_row(core_layout, overlay_.tr("gui.events_enabled", "Enabled"), "##events_enabled", &cfg.events.active, false));

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

            mark_dirty(combo_row(
                core_layout,
                overlay_.tr("gui.events_season_map_mode", "Season map mode"),
                "##events_season_map_mode",
                false,
                [&](const char *id) -> bool {
                    bool        changed    = false;
                    const char *mode_label = season_map_mode_label(cfg.events.SeasonMapMode);
                    if (ImGui::BeginCombo(id, mode_label)) {
                        auto selectable_mode = [&](PatchConfig::SeasonEventMapMode mode) -> void {
                            const char *label       = season_map_mode_label(mode);
                            const bool  is_selected = (cfg.events.SeasonMapMode == mode);
                            if (ImGui::Selectable(label, is_selected)) {
                                cfg.events.SeasonMapMode = mode;
                                changed                  = true;
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
                    return changed;
                }
            ));
            end_form_layout(core_layout);

            ImGui::Spacing();
            ImGui::TextUnformatted(overlay_.tr("gui.events_group_toggles", "Event toggles"));
            const std::array<BoolRowSpec, 21> rows = {{
                {"gui.events_igr", "IGR enabled", "##events_igr", &cfg.events.IgrEnabled, &global_config.events.IgrEnabled, false},
                {"gui.events_anniversary", "Anniversary enabled", "##events_anniversary", &cfg.events.AnniversaryEnabled, &global_config.events.AnniversaryEnabled, false},
                {"gui.events_easter_egg_world", "Easter egg world enabled", "##events_easter_egg_world", &cfg.events.EasterEggWorldEnabled, &global_config.events.EasterEggWorldEnabled, false},
                {"gui.events_double_keystones", "Double rift keystones", "##events_double_keystones", &cfg.events.DoubleRiftKeystones, &global_config.events.DoubleRiftKeystones, false},
                {"gui.events_double_blood_shards", "Double blood shards", "##events_double_blood_shards", &cfg.events.DoubleBloodShards, &global_config.events.DoubleBloodShards, false},
                {"gui.events_double_goblins", "Double treasure goblins", "##events_double_goblins", &cfg.events.DoubleTreasureGoblins, &global_config.events.DoubleTreasureGoblins, false},
                {"gui.events_double_bounty_bags", "Double bounty bags", "##events_double_bounty_bags", &cfg.events.DoubleBountyBags, &global_config.events.DoubleBountyBags, false},
                {"gui.events_royal_grandeur", "Royal Grandeur", "##events_royal_grandeur", &cfg.events.RoyalGrandeur, &global_config.events.RoyalGrandeur, false},
                {"gui.events_legacy_of_nightmares", "Legacy of Nightmares", "##events_legacy_of_nightmares", &cfg.events.LegacyOfNightmares, &global_config.events.LegacyOfNightmares, false},
                {"gui.events_triunes_will", "Triune's Will", "##events_triunes_will", &cfg.events.TriunesWill, &global_config.events.TriunesWill, false},
                {"gui.events_pandemonium", "Pandemonium", "##events_pandemonium", &cfg.events.Pandemonium, &global_config.events.Pandemonium, false},
                {"gui.events_kanai_powers", "Kanai powers", "##events_kanai_powers", &cfg.events.KanaiPowers, &global_config.events.KanaiPowers, false},
                {"gui.events_trials_of_tempests", "Trials of Tempests", "##events_trials_of_tempests", &cfg.events.TrialsOfTempests, &global_config.events.TrialsOfTempests, false},
                {"gui.events_shadow_clones", "Shadow clones", "##events_shadow_clones", &cfg.events.ShadowClones, &global_config.events.ShadowClones, false},
                {"gui.events_fourth_kanais", "Fourth Kanai's Cube slot", "##events_fourth_kanais", &cfg.events.FourthKanaisCubeSlot, &global_config.events.FourthKanaisCubeSlot, false},
                {"gui.events_ethereal_items", "Ethereal items", "##events_ethereal_items", &cfg.events.EtherealItems, &global_config.events.EtherealItems, false},
                {"gui.events_soul_shards", "Soul shards", "##events_soul_shards", &cfg.events.SoulShards, &global_config.events.SoulShards, false},
                {"gui.events_swarm_rifts", "Swarm rifts", "##events_swarm_rifts", &cfg.events.SwarmRifts, &global_config.events.SwarmRifts, false},
                {"gui.events_sanctified_items", "Sanctified items", "##events_sanctified_items", &cfg.events.SanctifiedItems, &global_config.events.SanctifiedItems, false},
                {"gui.events_dark_alchemy", "Dark Alchemy", "##events_dark_alchemy", &cfg.events.DarkAlchemy, &global_config.events.DarkAlchemy, false},
                {"gui.events_nesting_portals", "Nesting portals", "##events_nesting_portals", &cfg.events.NestingPortals, &global_config.events.NestingPortals, false},
            }};

            ImGui::BeginDisabled(!cfg.events.active);
            mark_dirty(render_filter_and_bulk(overlay_.tr("gui.filter_events", "Filter"), "##events_filter", events_filter, rows, false));
            const auto render_events_cluster = [&](const char *title_key, const char *title_fallback, const auto &indices, const char *table_id) -> bool {
                float max_label = 0.0f;
                for (int index : indices) {
                    EXL_ASSERT(index >= 0 && index < static_cast<int>(rows.size()), "events cluster index out of range");
                    const BoolRowSpec &row   = rows[static_cast<size_t>(index)];
                    const char        *label = overlay_.tr(row.tr_key, row.fallback);
                    if (!label_passes_filter(events_filter, label)) {
                        continue;
                    }
                    max_label = std::max(max_label, ImGui::CalcTextSize(label).x);
                }
                if (max_label <= 0.0f) {
                    return false;
                }

                render_subgroup_header(title_key, title_fallback, false);
                FormLayout toggles_layout = begin_form_layout(table_id, max_label);
                for (int index : indices) {
                    EXL_ASSERT(index >= 0 && index < static_cast<int>(rows.size()), "events cluster index out of range");
                    const BoolRowSpec &row   = rows[static_cast<size_t>(index)];
                    const char        *label = overlay_.tr(row.tr_key, row.fallback);
                    if (!label_passes_filter(events_filter, label)) {
                        continue;
                    }
                    mark_dirty(checkbox_row(toggles_layout, label, row.id, row.value, row.restart_required));
                }
                end_form_layout(toggles_layout);
                return true;
            };

            int visible_count = 0;
            for (const BoolRowSpec &row : rows) {
                const char *label = overlay_.tr(row.tr_key, row.fallback);
                if (label_passes_filter(events_filter, label)) {
                    ++visible_count;
                }
            }

            if (visible_count == 0) {
                ImGui::TextDisabled("%s", overlay_.tr("gui.filter_empty", "No rows match current filter."));
            } else {
                constexpr std::array<int, 7>  kCoreEventIndices     = {0, 1, 2, 7, 8, 9, 10};
                constexpr std::array<int, 4>  kBonusIndices         = {3, 4, 5, 6};
                constexpr std::array<int, 10> kSeasonalEventIndices = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

                bool rendered_any = false;
                rendered_any |= render_events_cluster("gui.events_cluster_core", "Core Events", kCoreEventIndices, "cfg_events_toggles_core_form");
                rendered_any |= render_events_cluster("gui.events_cluster_bonus", "Bonus Multipliers", kBonusIndices, "cfg_events_toggles_bonus_form");
                rendered_any |= render_events_cluster("gui.events_cluster_seasonal", "Seasonal Mechanics", kSeasonalEventIndices, "cfg_events_toggles_seasonal_form");
                if (!rendered_any) {
                    ImGui::TextDisabled("%s", overlay_.tr("gui.filter_empty", "No rows match current filter."));
                }
            }
            ImGui::EndDisabled();
        };

        auto render_cheats = [&]() -> void {
            ImGui::TextUnformatted(overlay_.tr("gui.cheats_group_core", "Core controls"));
            const float core_max_label = ImGui::CalcTextSize(overlay_.tr("gui.rare_cheats_attack_speed", "Attack speed multiplier")).x;
            FormLayout  core_layout    = begin_form_layout("cfg_cheats_core_form", core_max_label);
            mark_dirty(checkbox_row(core_layout, overlay_.tr("gui.rare_cheats_enabled", "Enabled"), "##rare_cheats_enabled", &cfg.rare_cheats.active, false));
            ImGui::BeginDisabled(!cfg.rare_cheats.active);
            float move_speed = static_cast<float>(cfg.rare_cheats.move_speed);
            if (slider_float_row(core_layout, overlay_.tr("gui.rare_cheats_move_speed", "Move speed multiplier"), "##rare_cheats_move_speed", &move_speed, 0.1f, 10.0f, "%.2fx", 0, false)) {
                cfg.rare_cheats.move_speed = static_cast<double>(move_speed);
                overlay_.set_ui_dirty(true);
            }
            float attack_speed = static_cast<float>(cfg.rare_cheats.attack_speed);
            if (slider_float_row(core_layout, overlay_.tr("gui.rare_cheats_attack_speed", "Attack speed multiplier"), "##rare_cheats_attack_speed", &attack_speed, 0.1f, 10.0f, "%.2fx", 0, false)) {
                cfg.rare_cheats.attack_speed = static_cast<double>(attack_speed);
                overlay_.set_ui_dirty(true);
            }
            end_form_layout(core_layout);

            ImGui::Spacing();
            ImGui::TextUnformatted(overlay_.tr("gui.cheats_group_toggles", "Cheat toggles"));
            const std::array<BoolRowSpec, 18> rows = {{
                {"gui.rare_cheats_super_god_mode", "Super god mode", "##rare_cheats_super_god_mode", &cfg.rare_cheats.super_god_mode, &global_config.rare_cheats.super_god_mode, false},
                {"gui.rare_cheats_extra_gr_orbs_elites", "Extra GR orbs on elite kill", "##rare_cheats_extra_gr_orbs_elites", &cfg.rare_cheats.extra_gr_orbs_elites, &global_config.rare_cheats.extra_gr_orbs_elites, false},
                {"gui.rare_cheats_floating_damage", "Floating damage color", "##rare_cheats_floating_damage", &cfg.rare_cheats.floating_damage_color, &global_config.rare_cheats.floating_damage_color, false},
                {"gui.rare_cheats_guaranteed_legendaries", "Guaranteed legendaries", "##rare_cheats_guaranteed_legendaries", &cfg.rare_cheats.guaranteed_legendaries, &global_config.rare_cheats.guaranteed_legendaries, false},
                {"gui.rare_cheats_drop_anything", "Drop anything", "##rare_cheats_drop_anything", &cfg.rare_cheats.drop_anything, &global_config.rare_cheats.drop_anything, false},
                {"gui.rare_cheats_instant_portal", "Instant portal", "##rare_cheats_instant_portal", &cfg.rare_cheats.instant_portal, &global_config.rare_cheats.instant_portal, false},
                {"gui.rare_cheats_no_cooldowns", "No cooldowns", "##rare_cheats_no_cooldowns", &cfg.rare_cheats.no_cooldowns, &global_config.rare_cheats.no_cooldowns, false},
                {"gui.rare_cheats_instant_craft", "Instant craft actions", "##rare_cheats_instant_craft", &cfg.rare_cheats.instant_craft_actions, &global_config.rare_cheats.instant_craft_actions, false},
                {"gui.rare_cheats_any_gem_any_slot", "Any gem any slot", "##rare_cheats_any_gem_any_slot", &cfg.rare_cheats.any_gem_any_slot, &global_config.rare_cheats.any_gem_any_slot, false},
                {"gui.rare_cheats_auto_pickup", "Auto pickup", "##rare_cheats_auto_pickup", &cfg.rare_cheats.auto_pickup, &global_config.rare_cheats.auto_pickup, false},
                {"gui.rare_cheats_equip_any_slot", "Equip any slot", "##rare_cheats_equip_any_slot", &cfg.rare_cheats.equip_any_slot, &global_config.rare_cheats.equip_any_slot, false},
                {"gui.rare_cheats_unlock_all_difficulties", "Unlock all difficulties", "##rare_cheats_unlock_all_difficulties", &cfg.rare_cheats.unlock_all_difficulties, &global_config.rare_cheats.unlock_all_difficulties, false},
                {"gui.rare_cheats_easy_kill", "Easy kill damage", "##rare_cheats_easy_kill", &cfg.rare_cheats.easy_kill_damage, &global_config.rare_cheats.easy_kill_damage, false},
                {"gui.rare_cheats_cube_no_consume", "Cube no consume", "##rare_cheats_cube_no_consume", &cfg.rare_cheats.cube_no_consume, &global_config.rare_cheats.cube_no_consume, false},
                {"gui.rare_cheats_gem_upgrade_always", "Gem upgrade always", "##rare_cheats_gem_upgrade_always", &cfg.rare_cheats.gem_upgrade_always, &global_config.rare_cheats.gem_upgrade_always, false},
                {"gui.rare_cheats_gem_upgrade_speed", "Gem upgrade speed", "##rare_cheats_gem_upgrade_speed", &cfg.rare_cheats.gem_upgrade_speed, &global_config.rare_cheats.gem_upgrade_speed, false},
                {"gui.rare_cheats_gem_upgrade_lvl150", "Gem upgrade lvl150", "##rare_cheats_gem_upgrade_lvl150", &cfg.rare_cheats.gem_upgrade_lvl150, &global_config.rare_cheats.gem_upgrade_lvl150, false},
                {"gui.rare_cheats_equip_multi_legendary", "Equip multi legendary", "##rare_cheats_equip_multi_legendary", &cfg.rare_cheats.equip_multi_legendary, &global_config.rare_cheats.equip_multi_legendary, false},
            }};

            mark_dirty(render_filter_and_bulk(overlay_.tr("gui.filter_cheats", "Filter"), "##cheats_filter", cheats_filter, rows, false));
            int visible_count = 0;
            for (const BoolRowSpec &row : rows) {
                const char *label = overlay_.tr(row.tr_key, row.fallback);
                if (!label_passes_filter(cheats_filter, label)) {
                    continue;
                }
                ++visible_count;
            }
            if (visible_count == 0) {
                ImGui::TextDisabled("%s", overlay_.tr("gui.filter_empty", "No rows match current filter."));
            } else {
                const float avail_w               = ImGui::GetContentRegionAvail().x;
                const bool  use_checkbox_grid     = (avail_w >= 760.0f);
                const auto  render_cheats_cluster = [&](const char *title_key, const char *title_fallback, const auto &indices, const char *grid_id, const char *table_id) -> bool {
                    std::array<const BoolRowSpec *, rows.size()> visible_rows {};
                    int                                          visible_len = 0;
                    float                                        max_label   = 0.0f;
                    for (int index : indices) {
                        EXL_ASSERT(index >= 0 && index < static_cast<int>(rows.size()), "cheats cluster index out of range");
                        const BoolRowSpec &row   = rows[static_cast<size_t>(index)];
                        const char        *label = overlay_.tr(row.tr_key, row.fallback);
                        if (!label_passes_filter(cheats_filter, label)) {
                            continue;
                        }
                        visible_rows[static_cast<size_t>(visible_len++)] = &row;
                        max_label                                        = std::max(max_label, ImGui::CalcTextSize(label).x);
                    }
                    if (visible_len == 0) {
                        return false;
                    }

                    render_subgroup_header(title_key, title_fallback, false);
                    if (use_checkbox_grid && ImGui::BeginTable(grid_id, 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
                        for (int i = 0; i < visible_len; i += 2) {
                            ImGui::TableNextRow();
                            for (int col = 0; col < 2; ++col) {
                                ImGui::TableSetColumnIndex(col);
                                if (i + col >= visible_len) {
                                    continue;
                                }
                                EXL_ASSERT(i + col < visible_len, "visible index out of range in cheats grid");
                                const BoolRowSpec *row = visible_rows[static_cast<size_t>(i + col)];
                                EXL_ASSERT(row != nullptr, "null visible row in cheats grid");
                                bool row_changed = ImGui::Checkbox(row->id, row->value);
                                ImGui::SameLine(0.0f, 6.0f);
                                ImGui::TextUnformatted(overlay_.tr(row->tr_key, row->fallback));
                                mark_dirty(row_changed);
                            }
                        }
                        ImGui::EndTable();
                    } else {
                        FormLayout toggles_layout = begin_form_layout(table_id, max_label);
                        for (int i = 0; i < visible_len; ++i) {
                            const BoolRowSpec *row = visible_rows[static_cast<size_t>(i)];
                            EXL_ASSERT(row != nullptr, "null visible row in cheats form");
                            const char *label = overlay_.tr(row->tr_key, row->fallback);
                            mark_dirty(checkbox_row(toggles_layout, label, row->id, row->value, row->restart_required));
                        }
                        end_form_layout(toggles_layout);
                    }
                    return true;
                };

                constexpr std::array<int, 5> kCombatIndices       = {0, 1, 2, 6, 12};
                constexpr std::array<int, 8> kLootCraftingIndices = {3, 4, 7, 8, 13, 14, 15, 16};
                constexpr std::array<int, 2> kProgressionIndices  = {11, 17};
                constexpr std::array<int, 3> kMiscIndices         = {5, 9, 10};

                bool rendered_any = false;
                rendered_any |= render_cheats_cluster("gui.cheats_cluster_combat", "Combat", kCombatIndices, "cfg_cheats_grid_combat", "cfg_cheats_toggles_combat_form");
                rendered_any |= render_cheats_cluster("gui.cheats_cluster_loot_crafting", "Loot-Crafting", kLootCraftingIndices, "cfg_cheats_grid_loot_crafting", "cfg_cheats_toggles_loot_crafting_form");
                rendered_any |= render_cheats_cluster("gui.cheats_cluster_progression", "Progression", kProgressionIndices, "cfg_cheats_grid_progression", "cfg_cheats_toggles_progression_form");
                rendered_any |= render_cheats_cluster("gui.cheats_cluster_misc", "Misc", kMiscIndices, "cfg_cheats_grid_misc", "cfg_cheats_toggles_misc_form");
                if (!rendered_any) {
                    ImGui::TextDisabled("%s", overlay_.tr("gui.filter_empty", "No rows match current filter."));
                }
            }
            ImGui::EndDisabled();
        };

        auto render_rifts = [&]() -> void {
            const float max_label = ImGui::CalcTextSize(overlay_.tr("gui.challenge_rifts_range_end", "Range end")).x;
            FormLayout  layout    = begin_form_layout("cfg_rifts_form", max_label);
            mark_dirty(checkbox_row(layout, overlay_.tr("gui.challenge_rifts_enabled", "Enabled"), "##challenge_rifts_enabled", &cfg.challenge_rifts.active, false));
            ImGui::BeginDisabled(!cfg.challenge_rifts.active);
            mark_dirty(checkbox_row(layout, overlay_.tr("gui.challenge_rifts_random", "Randomize within range"), "##challenge_rifts_random", &cfg.challenge_rifts.random, false));
            int start = static_cast<int>(cfg.challenge_rifts.range_start);
            int end   = static_cast<int>(cfg.challenge_rifts.range_end);
            if (input_int_row(layout, overlay_.tr("gui.challenge_rifts_range_start", "Range start"), "##challenge_rifts_range_start", &start, 1, 10, 0, false)) {
                cfg.challenge_rifts.range_start = static_cast<u32>(std::max(0, start));
                overlay_.set_ui_dirty(true);
            }
            if (input_int_row(layout, overlay_.tr("gui.challenge_rifts_range_end", "Range end"), "##challenge_rifts_range_end", &end, 1, 10, 0, false)) {
                cfg.challenge_rifts.range_end = static_cast<u32>(std::max(0, end));
                overlay_.set_ui_dirty(true);
            }
            ImGui::EndDisabled();
            end_form_layout(layout);
        };

        auto render_loot = [&]() -> void {
            ImGui::TextUnformatted(overlay_.tr("gui.loot_group_core", "Core controls"));
            const float core_max_label = ImGui::CalcTextSize(overlay_.tr("gui.loot_ancient_rank", "Ancient rank")).x;
            FormLayout  core_layout    = begin_form_layout("cfg_loot_core_form", core_max_label);
            mark_dirty(checkbox_row(core_layout, overlay_.tr("gui.loot_enabled", "Enabled"), "##loot_enabled", &cfg.loot_modifiers.active, false));
            end_form_layout(core_layout);

            ImGui::BeginDisabled(!cfg.loot_modifiers.active);
            ImGui::Spacing();
            ImGui::TextUnformatted(overlay_.tr("gui.loot_group_toggles", "Drop toggles"));
            const std::array<BoolRowSpec, 5> rows = {{
                {"gui.loot_disable_ancient", "Disable ancient drops", "##loot_disable_ancient", &cfg.loot_modifiers.DisableAncientDrops, &global_config.loot_modifiers.DisableAncientDrops, false},
                {"gui.loot_disable_primal", "Disable primal ancient drops", "##loot_disable_primal", &cfg.loot_modifiers.DisablePrimalAncientDrops, &global_config.loot_modifiers.DisablePrimalAncientDrops, false},
                {"gui.loot_disable_torment", "Disable torment drops", "##loot_disable_torment", &cfg.loot_modifiers.DisableTormentDrops, &global_config.loot_modifiers.DisableTormentDrops, false},
                {"gui.loot_disable_torment_check", "Disable torment check", "##loot_disable_torment_check", &cfg.loot_modifiers.DisableTormentCheck, &global_config.loot_modifiers.DisableTormentCheck, false},
                {"gui.loot_suppress_gift", "Suppress gift generation", "##loot_suppress_gift", &cfg.loot_modifiers.SuppressGiftGeneration, &global_config.loot_modifiers.SuppressGiftGeneration, false},
            }};

            mark_dirty(render_filter_and_bulk(overlay_.tr("gui.filter_loot", "Filter"), "##loot_filter", loot_filter, rows, false));
            float max_label = 0.0f;
            for (const BoolRowSpec &row : rows) {
                const char *label = overlay_.tr(row.tr_key, row.fallback);
                if (!label_passes_filter(loot_filter, label)) {
                    continue;
                }
                max_label = std::max(max_label, ImGui::CalcTextSize(label).x);
            }
            if (max_label <= 0.0f) {
                ImGui::TextDisabled("%s", overlay_.tr("gui.filter_empty", "No rows match current filter."));
            } else {
                FormLayout toggles_layout = begin_form_layout("cfg_loot_toggles_form", max_label);
                for (const BoolRowSpec &row : rows) {
                    const char *label = overlay_.tr(row.tr_key, row.fallback);
                    if (!label_passes_filter(loot_filter, label)) {
                        continue;
                    }
                    mark_dirty(checkbox_row(toggles_layout, label, row.id, row.value, false));
                }
                end_form_layout(toggles_layout);
            }

            ImGui::Spacing();
            ImGui::TextUnformatted(overlay_.tr("gui.loot_group_overrides", "Overrides"));
            const float quality_max_label = std::max(
                ImGui::CalcTextSize(overlay_.tr("gui.loot_forced_ilevel", "Forced iLevel")).x,
                ImGui::CalcTextSize(overlay_.tr("gui.loot_ancient_rank", "Ancient rank")).x
            );
            const float progression_max_label = ImGui::CalcTextSize(overlay_.tr("gui.loot_tiered_run_level", "Tiered loot run level")).x;

            render_subgroup_header("gui.loot_cluster_quality", "Item Quality", true);
            FormLayout quality_layout = begin_form_layout("cfg_loot_overrides_quality_form", quality_max_label);
            int        forced_ilevel  = cfg.loot_modifiers.ForcedILevel;
            if (slider_int_row(quality_layout, overlay_.tr("gui.loot_forced_ilevel", "Forced iLevel"), "##loot_forced_ilevel", &forced_ilevel, 0, 70, "%d", 0, false)) {
                cfg.loot_modifiers.ForcedILevel = forced_ilevel;
                overlay_.set_ui_dirty(true);
            }

            const std::array<const char *, 3> ranks = {
                overlay_.tr("gui.loot_rank_normal", "Normal"),
                overlay_.tr("gui.loot_rank_ancient", "Ancient"),
                overlay_.tr("gui.loot_rank_primal", "Primal"),
            };
            int rank_value = cfg.loot_modifiers.AncientRankValue;
            if (combo_row(
                    quality_layout,
                    overlay_.tr("gui.loot_ancient_rank", "Ancient rank"),
                    "##loot_ancient_rank",
                    false,
                    [&](const char *id) -> bool {
                        return ImGui::Combo(id, &rank_value, ranks.data(), static_cast<int>(ranks.size()));
                    }
                )) {
                cfg.loot_modifiers.AncientRankValue = rank_value;
                overlay_.set_ui_dirty(true);
            }
            end_form_layout(quality_layout);

            render_subgroup_header("gui.loot_cluster_progression", "Progression", false);
            FormLayout progression_layout = begin_form_layout("cfg_loot_overrides_progression_form", progression_max_label);
            int        tiered_level       = cfg.loot_modifiers.TieredLootRunLevel;
            if (slider_int_row(progression_layout, overlay_.tr("gui.loot_tiered_run_level", "Tiered loot run level"), "##loot_tiered_run_level", &tiered_level, 0, 150, "%d", 0, false)) {
                cfg.loot_modifiers.TieredLootRunLevel = tiered_level;
                overlay_.set_ui_dirty(true);
            }
            end_form_layout(progression_layout);
            ImGui::EndDisabled();
        };

        auto render_debug = [&]() -> void {
            float max_label = ImGui::CalcTextSize(overlay_.tr("gui.debug_show_metrics", "Show ImGui metrics")).x;
            for (const auto &e : d3::config_schema::EntriesForSection("debug")) {
                if (e.tr_label != nullptr && e.label_fallback != nullptr) {
                    max_label = std::max(max_label, ImGui::CalcTextSize(overlay_.tr(e.tr_label, e.label_fallback)).x);
                }
            }
            FormLayout  layout        = begin_form_layout("cfg_debug_form", max_label);
            const auto *enabled_entry = d3::config_schema::FindEntry("debug", "SectionEnabled");
            if (enabled_entry == nullptr || enabled_entry->get_bool == nullptr || enabled_entry->set_bool == nullptr) {
                mark_dirty(checkbox_row(layout, overlay_.tr("gui.debug_enabled", "Enabled"), "##debug_enabled", &cfg.debug.active, false));
                ImGui::BeginDisabled(!cfg.debug.active);
                mark_dirty(checkbox_row(layout, overlay_.tr("gui.debug_enable_crashes", "Enable crashes (danger)"), "##debug_enable_crashes", &cfg.debug.enable_crashes, true));
                mark_dirty(checkbox_row(layout, overlay_.tr("gui.debug_pubfile_dump", "Enable pubfile dump"), "##debug_pubfile_dump", &cfg.debug.enable_pubfile_dump, true));
                mark_dirty(checkbox_row(layout, overlay_.tr("gui.debug_error_traces", "Enable error traces"), "##debug_error_traces", &cfg.debug.enable_error_traces, true));
                mark_dirty(checkbox_row(layout, overlay_.tr("gui.debug_debug_flags", "Enable debug flags"), "##debug_debug_flags", &cfg.debug.enable_debug_flags, true));
                mark_dirty(checkbox_row(layout, overlay_.tr("gui.debug_spoof_network", "Spoof Network Functions"), "##debug_spoof_network", &cfg.debug.tagnx, true));
                ImGui::EndDisabled();
            } else {
                bool enabled = enabled_entry->get_bool(cfg);
                if (checkbox_row(layout, overlay_.tr(enabled_entry->tr_label, enabled_entry->label_fallback), "##debug_enabled", &enabled, enabled_entry->restart == d3::config_schema::RestartPolicy::RestartRequired)) {
                    enabled_entry->set_bool(cfg, enabled);
                    overlay_.set_ui_dirty(true);
                }

                ImGui::BeginDisabled(!enabled);
                for (const auto &e : d3::config_schema::EntriesForSection("debug")) {
                    if (e.key == "SectionEnabled") {
                        continue;
                    }
                    if (e.kind != d3::config_schema::ValueKind::Bool || e.get_bool == nullptr || e.set_bool == nullptr) {
                        continue;
                    }
                    bool        v  = e.get_bool(cfg);
                    std::string id = "##debug_";
                    id += e.key;
                    if (checkbox_row(layout, overlay_.tr(e.tr_label, e.label_fallback), id.c_str(), &v, e.restart == d3::config_schema::RestartPolicy::RestartRequired)) {
                        e.set_bool(cfg, v);
                        overlay_.set_ui_dirty(true);
                    }
                }
                ImGui::EndDisabled();
            }
            end_form_layout(layout);
            render_restart_legend();

            ImGui::Separator();
            bool show_demo = false;
            ImGui::BeginDisabled();
            ImGui::Checkbox(overlay_.tr("gui.debug_show_demo", "Show ImGui demo window (not linked)"), &show_demo);
            ImGui::EndDisabled();
            ImGui::Checkbox(overlay_.tr("gui.debug_show_metrics", "Show ImGui metrics"), &show_metrics_);
        };

        const ImGuiStyle &style              = ImGui::GetStyle();
        constexpr float   kNavMinWidth       = 200.0f;
        constexpr float   kNavMaxWidth       = 320.0f;
        constexpr float   kNavFontScale      = 1.12f;
        constexpr float   kNavRowHeightScale = 1.45f;
        constexpr float   kNavExtraSpacingY  = 6.0f;
        constexpr float   kNavExtraPaddingY  = 4.0f;
        const float       nav_font_size      = style.FontSizeBase * kNavFontScale;
        ImFont           *nav_font           = d3::imgui_overlay::GetTitleFont();
        const bool        has_nav_font       = (nav_font != nullptr);
        float             nav_text_w         = 0.0f;
        ImGui::PushFont(has_nav_font ? nav_font : nullptr, nav_font_size);
        for (const auto &section : kSections) {
            const char *label = overlay_.tr(section.key, section.fallback);
            nav_text_w        = std::max(nav_text_w, ImGui::CalcTextSize(label).x);
        }
        ImGui::PopFont();
        float       nav_width = nav_text_w + style.FramePadding.x * 2.0f + style.ItemSpacing.x * 2.0f;
        const float avail_w   = ImGui::GetContentRegionAvail().x;
        const float nav_max   = std::max(kNavMinWidth, std::min(kNavMaxWidth, avail_w * 0.45f));
        nav_width             = std::clamp(nav_width, kNavMinWidth, nav_max);

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
        const bool  action_buttons_wrap = buttons_width > (avail_w * 0.72f);
        const float action_bar_height   = action_buttons_wrap
                                              ? ImGui::GetFrameHeightWithSpacing() * 3.0f + style.WindowPadding.y * 2.0f
                                              : ImGui::GetFrameHeightWithSpacing() + style.WindowPadding.y * 2.0f;

        ImGui::BeginChild("cfg_body", ImVec2(0.0f, -action_bar_height), false);
        ImGui::BeginChild(
            "cfg_nav",
            ImVec2(nav_width, 0.0f),
            ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened
        );
        ImGui::PushFont(has_nav_font ? nav_font : nullptr, nav_font_size);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.ItemSpacing.y + kNavExtraSpacingY));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, style.FramePadding.y + kNavExtraPaddingY));
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
        const float nav_row_height = ImGui::GetTextLineHeightWithSpacing() * kNavRowHeightScale;
        bool        nav_has_focus  = false;
        for (int i = 0; i < static_cast<int>(kSections.size()); ++i) {
            const bool  selected = (section_index_ == i);
            const char *label    = overlay_.tr(kSections[static_cast<size_t>(i)].key, kSections[static_cast<size_t>(i)].fallback);
            if (ImGui::Selectable(label, selected, 0, ImVec2(0.0f, nav_row_height))) {
                section_index_ = i;
            }
            if (ImGui::IsItemFocused()) {
                nav_has_focus = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::PopStyleVar(3);
        ImGui::PopFont();
        ImGui::EndChild();

        const bool nav_right_requested =
            nav_has_focus &&
            (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight));

        ImGui::SameLine();

        auto apply_panel_scroll = [&]() -> void {
            ImGuiIO    &io           = ImGui::GetIO();
            const float line_height  = ImGui::GetTextLineHeightWithSpacing();
            float       scroll_delta = 0.0f;

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                io.MouseWheel != 0.0f) {
                scroll_delta -= io.MouseWheel * line_height * 5.0f;
            }

            const auto     &dbg                  = overlay_.frame_debug();
            const float     ry                   = d3::gui2::NormalizeStickComponentS16(static_cast<s32>(dbg.last_npad_stick_ry));
            constexpr float kStickDeadzone       = 0.25f;
            constexpr float kStickLinesPerSecond = 16.0f;
            const float     abs_ry               = std::abs(ry);
            if (abs_ry > kStickDeadzone) {
                const float t     = (abs_ry - kStickDeadzone) / (1.0f - kStickDeadzone);
                const float speed = t * kStickLinesPerSecond * line_height;
                scroll_delta -= (ry > 0.0f ? 1.0f : -1.0f) * speed * io.DeltaTime;
            }

            if (scroll_delta != 0.0f) {
                const float max_scroll = ImGui::GetScrollMaxY();
                float       next       = std::clamp(ImGui::GetScrollY() + scroll_delta, 0.0f, max_scroll);
                ImGui::SetScrollY(next);
            }
        };

        const ImGuiWindowFlags panel_flags       = ImGuiWindowFlags_NoScrollWithMouse;
        const ImGuiChildFlags  panel_child_flags = ImGuiChildFlags_NavFlattened;
        ImGui::BeginChild("cfg_panel", ImVec2(0.0f, 0.0f), panel_child_flags, panel_flags);
        apply_panel_scroll();
        ImGui::TextUnformatted(overlay_.tr(kSections[static_cast<size_t>(section_index_)].key, kSections[static_cast<size_t>(section_index_)].fallback));
        ImGui::Separator();
        if (nav_right_requested) {
            ImGui::SetKeyboardFocusHere();
        }
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

        ImGui::BeginChild(
            "cfg_action_bar",
            ImVec2(0.0f, action_bar_height),
            ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened,
            ImGuiWindowFlags_NoScrollbar
        );
        ImGui::AlignTextToFramePadding();

        const float start_x     = ImGui::GetCursorPosX();
        const float avail_x     = ImGui::GetContentRegionAvail().x;
        const float right_x     = start_x + avail_x - buttons_width;
        const bool  action_wrap = action_buttons_wrap || avail_x < 560.0f;

        auto render_state_banners = [&](bool inline_mode) -> bool {
            bool has_state = false;
            if (overlay_.ui_dirty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", overlay_.tr("gui.ui_state_dirty", "UI state: UNSAVED changes"));
                has_state = true;
            }
            if (restart_required_) {
                if (inline_mode && has_state) {
                    ImGui::SameLine();
                }
                if (restart_note_[0] != '\0') {
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
                        overlay_.tr("gui.restart_required_banner_note", "Restart required: %s"),
                        restart_note_
                    );
                } else {
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
                        "%s",
                        overlay_.tr("gui.restart_required_banner", "Restart required for some changes.")
                    );
                }
                has_state = true;
            }
            if (lang_restart_pending_) {
                if (inline_mode && has_state) {
                    ImGui::SameLine();
                }
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", overlay_.tr("gui.lang_restart_banner", "Language updated. Restart to fully apply new glyphs."));
                has_state = true;
            }
            return has_state;
        };

        bool clicked_reload = false;
        bool clicked_reset  = false;
        bool clicked_apply  = false;
        bool clicked_save   = false;

        if (!action_wrap) {
            bool has_state = render_state_banners(true);
            if (has_state) {
                ImGui::SameLine();
            }
            if (right_x > ImGui::GetCursorPosX()) {
                ImGui::SetCursorPosX(right_x);
            }
            clicked_reload = ImGui::Button(label_reload);
            ImGui::SameLine();
            clicked_reset = ImGui::Button(label_reset);
            ImGui::SameLine();
            clicked_apply = ImGui::Button(label_apply);
            ImGui::SameLine();
            clicked_save = ImGui::Button(label_save);
        } else {
            const bool has_state = render_state_banners(false);
            if (has_state) {
                ImGui::Separator();
            }
            const float line_start_x         = ImGui::GetCursorPosX();
            const float max_x                = line_start_x + ImGui::GetContentRegionAvail().x;
            auto        place_wrapped_button = [&](const char *label) -> bool {
                const float w      = button_width(label);
                const float cursor = ImGui::GetCursorPosX();
                if (cursor > line_start_x && (cursor + w) > max_x) {
                    ImGui::NewLine();
                } else if (cursor > line_start_x) {
                    ImGui::SameLine();
                }
                return ImGui::Button(label);
            };
            clicked_reload = place_wrapped_button(label_reload);
            clicked_reset  = place_wrapped_button(label_reset);
            clicked_apply  = place_wrapped_button(label_apply);
            clicked_save   = place_wrapped_button(label_save);
        }

        auto build_runtime_apply = [&](const PatchConfig &requested, bool *gui_pending_out) -> PatchConfig {
            PatchConfig runtime                      = requested;
            runtime.gui.enabled                      = global_config.gui.enabled;
            runtime.gui.visible                      = global_config.gui.visible;
            runtime.gui.allow_left_stick_passthrough = global_config.gui.allow_left_stick_passthrough;
            runtime.gui.language_override            = global_config.gui.language_override;
            if (gui_pending_out != nullptr) {
                *gui_pending_out =
                    requested.gui.enabled != global_config.gui.enabled ||
                    requested.gui.visible != global_config.gui.visible ||
                    requested.gui.allow_left_stick_passthrough != global_config.gui.allow_left_stick_passthrough;
            }
            return runtime;
        };

        auto update_restart_note = [&](const d3::RuntimeApplyResult &apply, bool gui_pending) -> void {
            restart_note_[0] = '\0';
            if (apply.note[0] != '\0') {
                snprintf(restart_note_, sizeof(restart_note_), "%s", apply.note);
            }
            if (gui_pending) {
                if (restart_note_[0] == '\0') {
                    snprintf(restart_note_, sizeof(restart_note_), "%s", "GUI");
                } else {
                    const size_t len = std::strlen(restart_note_);
                    if (len + 6 < sizeof(restart_note_)) {
                        std::strncat(restart_note_, ", ", sizeof(restart_note_) - len - 1);
                        std::strncat(restart_note_, "GUI", sizeof(restart_note_) - std::strlen(restart_note_) - 1);
                    }
                }
            }
        };

        if (clicked_reload) {
            PatchConfig loaded {};
            std::string error;
            if (LoadPatchConfigFromPath("sd:/config/d3hack-nx/config.toml", loaded, error)) {
                const PatchConfig      normalized    = NormalizePatchConfig(loaded);
                bool                   gui_pending   = false;
                const PatchConfig      runtime_apply = build_runtime_apply(normalized, &gui_pending);
                d3::RuntimeApplyResult apply {};
                d3::ApplyPatchConfigRuntime(runtime_apply, &apply);
                restart_required_ = apply.restart_required || gui_pending;
                update_restart_note(apply, gui_pending);
                if (!restart_required_) {
                    restart_note_[0] = '\0';
                }
                cfg                   = normalized;
                lang_restart_pending_ = false;
                overlay_.set_ui_dirty(false);
                if (auto *notifications = overlay_.notifications_window())
                    notifications->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, overlay_.tr("gui.notify_reloaded", "Reloaded config.toml"));
            } else {
                if (auto *notifications = overlay_.notifications_window())
                    notifications->AddNotification(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 8.0f, overlay_.tr("gui.notify_reload_failed", "Reload failed: %s"), error.empty() ? overlay_.tr("gui.error_not_found", "not found") : error.c_str());
            }
        }

        if (clicked_reset) {
            cfg = global_config;
            overlay_.set_ui_dirty(false);
            restart_required_     = false;
            restart_note_[0]      = '\0';
            lang_restart_pending_ = false;
        }

        if (clicked_apply) {
            const PatchConfig      normalized    = NormalizePatchConfig(cfg);
            bool                   gui_pending   = false;
            const PatchConfig      runtime_apply = build_runtime_apply(normalized, &gui_pending);
            d3::RuntimeApplyResult apply {};
            d3::ApplyPatchConfigRuntime(runtime_apply, &apply);
            const bool restart_needed = apply.restart_required || gui_pending;
            restart_required_         = restart_needed;
            update_restart_note(apply, gui_pending);
            if (!restart_required_) {
                restart_note_[0] = '\0';
            }

            cfg                   = normalized;
            lang_restart_pending_ = cfg.gui.language_override != global_config.gui.language_override;
            overlay_.set_ui_dirty(false);

            auto *notifications = overlay_.notifications_window();
            if (restart_needed) {
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

        if (clicked_save) {
            std::string error;
            if (SavePatchConfigToPath("sd:/config/d3hack-nx/config.toml", cfg, error)) {
                lang_restart_pending_ = cfg.gui.language_override != global_config.gui.language_override;
                if (auto *notifications = overlay_.notifications_window())
                    notifications->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, overlay_.tr("gui.notify_saved", "Saved config.toml"));
            } else {
                if (auto *notifications = overlay_.notifications_window())
                    notifications->AddNotification(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 8.0f, overlay_.tr("gui.notify_save_failed", "Save failed: %s"), error.empty() ? overlay_.tr("gui.error_unknown", "unknown") : error.c_str());
            }
        }

        ImGui::EndChild();

        if (d3::gui2::ui::virtual_keyboard::Render()) {
            overlay_.set_ui_dirty(true);
            lang_restart_pending_ = cfg.gui.language_override != global_config.gui.language_override;
            if (auto *notifications = overlay_.notifications_window()) {
                notifications->AddNotification(
                    ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
                    6.0f,
                    overlay_.tr("gui.notify_lang_restart", "Language updated. Restart to fully apply new glyphs.")
                );
            }
        }

        UpdateDockSwipe(window_pos, window_size);
    }

}  // namespace d3::gui2::ui::windows
