#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "program/config.hpp"
#include "program/gui2/ui/window.hpp"

namespace d3::gui2::ui {

    struct GuiFocusState {
        bool visible                      = false;
        bool nav_active                   = false;
        bool want_capture_gamepad         = false;
        bool want_capture_mouse           = false;
        bool want_capture_keyboard        = false;
        bool should_block_game_input      = false;
        bool allow_left_stick_passthrough = false;
    };

    enum class GuiTheme {
        D3Dark  = 0,
        Blueish = 1,
    };

    namespace windows {
        class NotificationsWindow;
    }

    class Overlay final {
       public:
        Overlay();
        ~Overlay();

        void OnImGuiContextCreated();
        void EnsureConfigLoaded();
        void EnsureWindowsCreated();
        void EnsureTranslationsLoaded();

        struct FrameDebugInfo {
            int    crop_w                  = 0;
            int    crop_h                  = 0;
            int    swapchain_texture_count = 0;
            ImVec2 viewport_size           = ImVec2(0.0f, 0.0f);

            bool               last_npad_valid    = false;
            unsigned long long last_npad_buttons  = 0;
            int                last_npad_stick_lx = 0;
            int                last_npad_stick_ly = 0;
            int                last_npad_stick_rx = 0;
            int                last_npad_stick_ry = 0;
        };

        // Must be called after ImGui::NewFrame(). Returns true if request_focus was consumed by a visible Begin().
        bool UpdateFrame(bool font_uploaded, int crop_w, int crop_h, int swapchain_texture_count, const ImVec2 &viewport_size, bool last_npad_valid, unsigned long long last_npad_buttons, int last_npad_stick_lx, int last_npad_stick_ly, int last_npad_stick_rx, int last_npad_stick_ry);

        const GuiFocusState  &focus_state() const { return focus_; }
        const FrameDebugInfo &frame_debug() const { return frame_debug_; }

        // Translation helper: returns a localized string when available, otherwise returns fallback.
        const char        *tr(const char *key, const char *fallback);
        const std::string &translations_lang() const { return translations_lang_; }

        PatchConfig       &ui_config() { return ui_config_; }
        const PatchConfig &ui_config() const { return ui_config_; }
        bool               ui_dirty() const { return ui_dirty_; }
        void               set_ui_dirty(bool v) { ui_dirty_ = v; }

        bool  overlay_visible() const { return overlay_visible_; }
        bool *overlay_visible_ptr() { return &overlay_visible_; }
        void  set_overlay_visible(bool v);
        void  set_overlay_visible_persist(bool v);
        void  ToggleVisibleAndPersist();

        bool allow_left_stick_passthrough() const { return allow_left_stick_passthrough_; }
        void set_allow_left_stick_passthrough(bool allow) { allow_left_stick_passthrough_ = allow; }

        void RequestFocus();
        void RequestFocusWindow(Window *window);

        bool imgui_render_enabled() const {
            return ui_config_initialized_ ? ui_config_.gui.enabled : global_config.gui.enabled;
        }

        // "Main window" should be rendered this frame (config window). Toasts may still render when false.
        bool wants_render() const {
            return ui_config_initialized_ && ui_config_.gui.enabled && overlay_visible_;
        }

        bool is_config_loaded() const { return ui_config_initialized_; }

        bool     layout_loaded() const { return layout_loaded_; }
        bool     should_apply_default_layout() const { return !layout_loaded_ && !layout_default_applied_; }
        ImGuiID  dockspace_id() const { return dockspace_id_; }
        GuiTheme theme() const { return theme_; }
        void     set_theme(GuiTheme theme);
        void     AfterFrame();

        windows::NotificationsWindow       *notifications_window() { return notifications_window_; }
        const windows::NotificationsWindow *notifications_window() const { return notifications_window_; }

       protected:
       private:
        FrameDebugInfo frame_debug_ {};

        GuiFocusState focus_ {};

        bool        ui_config_initialized_ = false;
        PatchConfig ui_config_ {};
        bool        ui_dirty_ = false;

        bool    overlay_visible_              = true;
        bool    request_focus_                = false;
        Window *focus_window_                 = nullptr;
        bool    allow_left_stick_passthrough_ = false;

        std::string                                  translations_lang_ {};
        bool                                         translations_loaded_ = false;
        std::unordered_map<std::string, std::string> translations_ {};

        bool                                 windows_initialized_ = false;
        std::vector<std::unique_ptr<Window>> windows_ {};
        std::vector<Window *>                dock_windows_ {};
        std::vector<Window *>                overlay_windows_ {};

        bool     imgui_context_ready_    = false;
        bool     layout_loaded_          = false;
        bool     layout_dirty_           = false;
        bool     layout_default_applied_ = false;
        bool     layout_reset_pending_   = false;
        ImGuiID  dockspace_id_           = 0;
        GuiTheme theme_                  = GuiTheme::Blueish;

        Window                       *config_window_        = nullptr;
        windows::NotificationsWindow *notifications_window_ = nullptr;
    };

}  // namespace d3::gui2::ui
