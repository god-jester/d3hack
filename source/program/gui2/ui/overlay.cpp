#include "program/gui2/ui/overlay.hpp"

#include <cctype>

#include "program/romfs_assets.hpp"
#include "program/d3/setting.hpp"
#include "symbols/common.hpp"
#include "tomlplusplus/toml.hpp"

#include "program/gui2/ui/windows/config_window.hpp"
#include "program/gui2/ui/windows/notifications_window.hpp"

namespace d3::gui2::ui {
    namespace {

        static std::string NormalizeLang(std::string_view in) {
            std::string out;
            out.reserve(2);

            for (char ch : in) {
                if (std::isalpha(static_cast<unsigned char>(ch)) == 0) {
                    continue;
                }
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                if (out.size() >= 2) {
                    break;
                }
            }

            if (out == "jp") {
                return "ja";
            }
            if (out == "zt") {
                return "zh";
            }
            return out;
        }

        static std::string NormalizeLocale(std::string_view locale) {
            if (locale == "Invalid" || locale == "Global") {
                return {};
            }
            if (locale == "jpJP") {
                return "ja";
            }
            if (locale == "ztCN") {
                return "zh";
            }
            return NormalizeLang(locale);
        }

        static std::string ResolveGameLang() {
            const auto *locale_ptr = reinterpret_cast<const unsigned long long *>(GameOffsetFromTable("unicode_text_current_locale"));
            if (locale_ptr == nullptr) {
                return {};
            }

            const auto xlocale = static_cast<uint32>(*locale_ptr);
            if (xlocale == 0) {
                return {};
            }

            const auto *locale_str = XLocaleToString(xlocale);
            if (locale_str == nullptr || locale_str[0] == '\0') {
                return {};
            }

            return NormalizeLocale(locale_str);
        }

        static void FlattenTomlStrings(const toml::node &node, std::string &prefix, std::unordered_map<std::string, std::string> &out) {
            if (node.is_table()) {
                const auto *tbl = node.as_table();
                tbl->for_each([&](const toml::key &k, const toml::node &v) {
                    const auto key_str = std::string(k.str());

                    const auto old_len = prefix.size();
                    if (!prefix.empty()) {
                        prefix.push_back('.');
                    }
                    prefix += key_str;

                    FlattenTomlStrings(v, prefix, out);
                    prefix.resize(old_len);
                });
                return;
            }

            if (auto s = node.value<std::string>()) {
                if (!prefix.empty()) {
                    out[prefix] = *s;
                }
                return;
            }
        }

    }  // namespace

    Overlay::Overlay() = default;

    Overlay::~Overlay() = default;

    const char *Overlay::tr(const char *key, const char *fallback) {
        if (key == nullptr || fallback == nullptr) {
            return fallback != nullptr ? fallback : "";
        }

        EnsureTranslationsLoaded();

        if (!translations_loaded_) {
            return fallback;
        }

        auto it = translations_.find(key);
        if (it == translations_.end()) {
            return fallback;
        }

        return it->second.c_str();
    }

    void Overlay::EnsureWindowsCreated() {
        if (windows_initialized_) {
            return;
        }

        auto notifications    = std::make_unique<windows::NotificationsWindow>();
        notifications_window_ = notifications.get();
        notifications_window_->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 6.0f, D3HACK_VER " initialized!");
        notifications_window_->AddNotification(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), 10.0f, "Hold + and - to toggle GUI");

        auto config    = std::make_unique<windows::ConfigWindow>(*this);
        config_window_ = config.get();

        windows_.push_back(std::move(config));
        windows_.push_back(std::move(notifications));

        windows_initialized_ = true;
    }

    void Overlay::EnsureConfigLoaded() {
        if (ui_config_initialized_) {
            return;
        }
        if (!global_config.initialized) {
            return;
        }

        ui_config_             = global_config;
        overlay_visible_       = global_config.gui.visible;
        ui_config_initialized_ = true;
        ui_dirty_              = false;
    }

    void Overlay::EnsureTranslationsLoaded() {
        if (!ui_config_initialized_) {
            return;
        }

        std::string desired_lang;
        if (!ui_config_.gui.language_override.empty()) {
            desired_lang = NormalizeLang(ui_config_.gui.language_override);
        } else {
            desired_lang = ResolveGameLang();
        }

        if (desired_lang.empty()) {
            desired_lang = "en";
        }

        if (translations_loaded_ && translations_lang_ == desired_lang) {
            return;
        }

        translations_.clear();
        translations_lang_   = desired_lang;
        translations_loaded_ = false;

        if (translations_lang_ == "en") {
            translations_loaded_ = true;
            return;
        }

        std::string path = "romfs:/d3gui/strings_";
        path += translations_lang_;
        path += ".toml";

        std::string text;
        if (!d3::romfs::ReadFileToString(path.c_str(), text, 512 * 1024)) {
            PRINT("[gui] translations missing: %s (lang=%s)", path.c_str(), translations_lang_.c_str());
            translations_loaded_ = true;
            return;
        }

        auto result = toml::parse(text, std::string_view {path});
        if (!result) {
            const auto       &err  = result.error();
            const std::string desc = std::string(err.description());
            PRINT("[gui] translations parse failed: %s (lang=%s)", desc.c_str(), translations_lang_.c_str());
            translations_loaded_ = true;
            return;
        }

        std::string prefix;
        FlattenTomlStrings(result.table(), prefix, translations_);
        translations_loaded_ = true;
        PRINT("[gui] translations loaded: lang=%s keys=%u", translations_lang_.c_str(), static_cast<unsigned>(translations_.size()));
    }

    void Overlay::set_overlay_visible(bool v) {
        overlay_visible_ = v;
    }

    void Overlay::set_overlay_visible_persist(bool v) {
        const bool was_visible = overlay_visible_;
        set_overlay_visible(v);

        if (!was_visible && overlay_visible_) {
            RequestFocus();
        }

        if (ui_config_initialized_) {
            ui_config_.gui.visible = overlay_visible_;
            ui_dirty_              = true;
        }
    }

    void Overlay::ToggleVisibleAndPersist() {
        set_overlay_visible_persist(!overlay_visible_);
    }

    void Overlay::RequestFocus() {
        request_focus_ = true;
    }

    bool Overlay::UpdateFrame(bool font_uploaded, int crop_w, int crop_h, int swapchain_texture_count, const ImVec2 &viewport_size, bool last_npad_valid, unsigned long long last_npad_buttons, int last_npad_stick_lx, int last_npad_stick_ly, int last_npad_stick_rx, int last_npad_stick_ry) {
        EnsureConfigLoaded();
        EnsureWindowsCreated();

        focus_ = {};

        frame_debug_.crop_w                  = crop_w;
        frame_debug_.crop_h                  = crop_h;
        frame_debug_.swapchain_texture_count = swapchain_texture_count;
        frame_debug_.viewport_size           = viewport_size;
        frame_debug_.last_npad_valid         = last_npad_valid;
        frame_debug_.last_npad_buttons       = last_npad_buttons;
        frame_debug_.last_npad_stick_lx      = last_npad_stick_lx;
        frame_debug_.last_npad_stick_ly      = last_npad_stick_ly;
        frame_debug_.last_npad_stick_rx      = last_npad_stick_rx;
        frame_debug_.last_npad_stick_ry      = last_npad_stick_ry;

        const bool gui_enabled = imgui_render_enabled();
        const bool can_draw    = gui_enabled && font_uploaded;

        if (can_draw) {
            EnsureTranslationsLoaded();
        }

        if (notifications_window_ != nullptr) {
            notifications_window_->SetViewportSize(viewport_size);
            notifications_window_->Tick(ImGui::GetIO().DeltaTime);
            notifications_window_->SetEnabled(can_draw);
        }

        if (config_window_ != nullptr) {
            config_window_->SetEnabled(can_draw && overlay_visible_);
        }

        bool consumed_focus = false;

        if (can_draw) {
            for (auto &window : windows_) {
                if (window == nullptr) {
                    continue;
                }

                if (window.get() == config_window_ && request_focus_) {
                    ImGui::SetNextWindowFocus();
                }

                const bool begin_visible = window->Render();
                if (window.get() == config_window_ && request_focus_ && begin_visible) {
                    request_focus_ = false;
                    consumed_focus = true;
                }
            }

            ImGuiIO &io       = ImGui::GetIO();
            focus_.nav_active = io.NavActive;
            // Dear ImGui doesn't expose WantCaptureGamepad; use keyboard capture as the closest proxy when navigation is enabled.
            focus_.want_capture_gamepad = io.WantCaptureKeyboard;
        }

        const bool toasts_visible      = (notifications_window_ != nullptr && notifications_window_->IsOpen());
        focus_.visible                 = can_draw && (overlay_visible_ || toasts_visible);
        focus_.should_block_game_input = can_draw && overlay_visible_;

        return consumed_focus;
    }

}  // namespace d3::gui2::ui
