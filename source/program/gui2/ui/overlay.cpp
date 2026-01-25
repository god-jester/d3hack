#include "program/gui2/ui/overlay.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string_view>

#include "program/romfs_assets.hpp"
#include "program/d3/setting.hpp"
#include "nn/fs.hpp"  // IWYU pragma: keep
#include "symbols/common.hpp"
#include "tomlplusplus/toml.hpp"

#include "program/gui2/ui/windows/config_window.hpp"
#include "program/gui2/ui/windows/notifications_window.hpp"

namespace d3::gui2::ui {
    namespace {

        static auto NormalizeLang(std::string_view in) -> std::string {
            std::string out;
            out.reserve(2);

            for (char const ch : in) {
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

        static auto NormalizeLocale(std::string_view locale) -> std::string {
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

        static auto ResolveGameLang() -> std::string {
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
                tbl->for_each([&](const toml::key &k, const toml::node &v) -> void {
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

        constexpr const char *kGuiLayoutPath        = "sd:/config/d3hack-nx/gui_layout.ini";
        constexpr const char *kGuiLayoutThemePrefix = ";d3hack_theme=";

        static auto ThemeToString(GuiTheme theme) -> const char * {
            switch (theme) {
            case GuiTheme::D3Dark:
                return "d3";
            case GuiTheme::Luna:
                return "luna";
            }
            return "d3";
        }

        static auto ThemeFromString(std::string_view value, GuiTheme fallback) -> GuiTheme {
            if (value.empty()) {
                return fallback;
            }

            std::string lower;
            lower.reserve(value.size());
            for (const char ch : value) {
                const unsigned char uch = static_cast<unsigned char>(ch);
                lower.push_back(static_cast<char>(std::tolower(uch)));
            }

            if (lower == "d3" || lower == "d3dark" || lower == "classic") {
                return GuiTheme::D3Dark;
            }
            if (lower == "luna" || lower == "lunakit") {
                return GuiTheme::Luna;
            }
            return fallback;
        }

        static void ParseLayoutMeta(const std::string &text, GuiTheme &theme) {
            const size_t prefix_len = std::strlen(kGuiLayoutThemePrefix);
            size_t       pos        = text.find(kGuiLayoutThemePrefix);
            if (pos == std::string::npos) {
                return;
            }

            pos += prefix_len;
            size_t end = text.find_first_of("\r\n", pos);
            if (end == std::string::npos) {
                end = text.size();
            }
            if (end <= pos) {
                return;
            }

            const std::string_view value(text.data() + pos, end - pos);
            theme = ThemeFromString(value, theme);
        }

        static auto BuildLayoutText(GuiTheme theme, const char *ini_data, size_t ini_size) -> std::string {
            std::string out;
            out.reserve(ini_size + 64);
            out.append(kGuiLayoutThemePrefix);
            out.append(ThemeToString(theme));
            out.push_back('\n');

            if (ini_data != nullptr && ini_size > 0) {
                out.append(ini_data, ini_size);
            }

            return out;
        }

        static auto DoesFileExist(const char *path) -> bool {
            nn::fs::DirectoryEntryType type {};
            auto                       rc = nn::fs::GetEntryType(&type, path);
            if (R_FAILED(rc)) {
                return false;
            }
            return type == nn::fs::DirectoryEntryType_File;
        }

        static auto EnsureConfigDirectories(std::string &error_out) -> bool {
            const char *root_dir = "sd:/config";
            const char *hack_dir = "sd:/config/d3hack-nx";

            (void)nn::fs::CreateDirectory(root_dir);
            (void)nn::fs::CreateDirectory(hack_dir);

            nn::fs::DirectoryEntryType type {};
            auto                       rc = nn::fs::GetEntryType(&type, hack_dir);
            if (R_FAILED(rc)) {
                error_out = "Failed to ensure config directory exists";
                return false;
            }
            return true;
        }

        static auto WriteAllAtomic(const char *path, const std::string &text, std::string &error_out) -> bool {
            if (!EnsureConfigDirectories(error_out)) {
                return false;
            }

            std::string const tmp_path = std::string(path) + ".tmp";
            std::string const bak_path = std::string(path) + ".bak";

            (void)nn::fs::DeleteFile(tmp_path.c_str());

            auto rc = nn::fs::CreateFile(tmp_path.c_str(), static_cast<s64>(text.size()));
            if (R_FAILED(rc)) {
                error_out = "Failed to create temp layout file";
                return false;
            }

            nn::fs::FileHandle fh {};
            rc = nn::fs::OpenFile(&fh, tmp_path.c_str(), nn::fs::OpenMode_Write);
            if (R_FAILED(rc)) {
                (void)nn::fs::DeleteFile(tmp_path.c_str());
                error_out = "Failed to open temp layout file";
                return false;
            }

            const auto opt = nn::fs::WriteOption::CreateOption(nn::fs::WriteOptionFlag_Flush);
            rc             = nn::fs::WriteFile(fh, 0, text.data(), static_cast<u64>(text.size()), opt);
            if (R_FAILED(rc)) {
                nn::fs::CloseFile(fh);
                (void)nn::fs::DeleteFile(tmp_path.c_str());
                error_out = "Failed to write temp layout file";
                return false;
            }

            (void)nn::fs::FlushFile(fh);
            nn::fs::CloseFile(fh);

            if (DoesFileExist(path)) {
                (void)nn::fs::DeleteFile(bak_path.c_str());
                rc = nn::fs::RenameFile(path, bak_path.c_str());
                if (R_FAILED(rc)) {
                    (void)nn::fs::DeleteFile(tmp_path.c_str());
                    error_out = "Failed to backup existing layout file";
                    return false;
                }
            }

            rc = nn::fs::RenameFile(tmp_path.c_str(), path);
            if (R_FAILED(rc)) {
                if (DoesFileExist(bak_path.c_str())) {
                    (void)nn::fs::RenameFile(bak_path.c_str(), path);
                }
                (void)nn::fs::DeleteFile(tmp_path.c_str());
                error_out = "Failed to move temp layout into place";
                return false;
            }

            if (DoesFileExist(bak_path.c_str())) {
                (void)nn::fs::DeleteFile(bak_path.c_str());
            }

            return true;
        }

        static void DeleteLayoutFile() {
            if (DoesFileExist(kGuiLayoutPath)) {
                (void)nn::fs::DeleteFile(kGuiLayoutPath);
            }
        }

    }  // namespace

    Overlay::Overlay() = default;

    Overlay::~Overlay() = default;

    void Overlay::OnImGuiContextCreated() {
        if (imgui_context_ready_) {
            return;
        }

        imgui_context_ready_    = true;
        layout_loaded_          = false;
        layout_dirty_           = false;
        layout_default_applied_ = false;
        layout_reset_pending_   = false;

        std::string text;
        if (d3::romfs::ReadFileToString(kGuiLayoutPath, text, 512 * 1024)) {
            ParseLayoutMeta(text, theme_);
            ImGui::LoadIniSettingsFromMemory(text.c_str(), text.size());
            layout_loaded_ = true;
            PRINT("[gui] layout loaded: %s (theme=%s)", kGuiLayoutPath, ThemeToString(theme_));
        } else {
            PRINT("[gui] layout missing: %s", kGuiLayoutPath);
        }
    }

    auto Overlay::tr(const char *key, const char *fallback) -> const char * {
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

        dock_windows_.clear();
        overlay_windows_.clear();

        auto notifications    = std::make_unique<windows::NotificationsWindow>();
        notifications_window_ = notifications.get();
        notifications_window_->AddNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 6.0f, D3HACK_VER " initialized!");
        notifications_window_->AddNotification(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), 10.0f, "Hold + and - to toggle GUI");
        overlay_windows_.push_back(notifications_window_);

        auto config    = std::make_unique<windows::ConfigWindow>(*this);
        config_window_ = config.get();
        dock_windows_.push_back(config_window_);

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
            if (config_window_ != nullptr) {
                config_window_->SetOpen(true);
                RequestFocusWindow(config_window_);
            } else {
                RequestFocus();
            }
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
        focus_window_  = nullptr;
    }

    void Overlay::RequestFocusWindow(Window *window) {
        request_focus_ = true;
        focus_window_  = window;
        if (window != nullptr) {
            window->SetOpen(true);
        }
    }

    void Overlay::set_theme(GuiTheme theme) {
        if (theme_ == theme) {
            return;
        }

        theme_        = theme;
        layout_dirty_ = true;
    }

    auto Overlay::UpdateFrame(bool font_uploaded, int crop_w, int crop_h, int swapchain_texture_count, const ImVec2 &viewport_size, bool last_npad_valid, unsigned long long last_npad_buttons, int last_npad_stick_lx, int last_npad_stick_ly, int last_npad_stick_rx, int last_npad_stick_ry) -> bool {
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

        bool consumed_focus = false;

        if (notifications_window_ != nullptr) {
            notifications_window_->SetViewportSize(viewport_size);
        }

        const float dt_s = ImGui::GetIO().DeltaTime;
        for (auto &window : windows_) {
            if (window == nullptr) {
                continue;
            }
            window->SetEnabled(can_draw);
            if (can_draw) {
                window->Update(dt_s);
            }
        }

        const bool apply_default_layout = can_draw && overlay_visible_ && should_apply_default_layout();

        if (can_draw && overlay_visible_) {
            const ImGuiViewport *viewport = ImGui::GetMainViewport();
            const ImVec2         dock_pos = viewport != nullptr ? viewport->Pos : ImVec2(0.0f, 0.0f);
            const ImVec2         dock_size =
                viewport != nullptr ? viewport->Size : viewport_size;

            ImGui::SetNextWindowPos(dock_pos);
            ImGui::SetNextWindowSize(dock_size);
            if (viewport != nullptr) {
                ImGui::SetNextWindowViewport(viewport->ID);
            }
            ImGui::SetNextWindowBgAlpha(0.18f);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

            const ImGuiWindowFlags dock_flags =
                ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus |
                ImGuiWindowFlags_MenuBar;

            if (ImGui::Begin("##d3hack_dockspace", nullptr, dock_flags)) {
                if (ImGui::BeginMenuBar()) {
                    if (ImGui::BeginMenu("D3Hack")) {
                        if (ImGui::MenuItem(tr("gui.menu_save_layout", "Save layout"))) {
                            layout_dirty_                      = true;
                            ImGui::GetIO().WantSaveIniSettings = true;
                        }
                        if (ImGui::MenuItem(tr("gui.menu_reset_layout", "Reset layout"))) {
                            DeleteLayoutFile();
                            layout_loaded_          = false;
                            layout_default_applied_ = false;
                            layout_reset_pending_   = true;
                            layout_dirty_           = true;
                            if (config_window_ != nullptr) {
                                config_window_->SetOpen(true);
                                RequestFocusWindow(config_window_);
                            }
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem(tr("gui.menu_hide_overlay", "Hide overlay"))) {
                            set_overlay_visible_persist(false);
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu(tr("gui.menu_windows", "Windows"))) {
                        if (ImGui::MenuItem(tr("gui.menu_windows_show_all", "Show all"))) {
                            for (auto *window : dock_windows_) {
                                if (window != nullptr) {
                                    window->SetOpen(true);
                                }
                            }
                            if (config_window_ != nullptr) {
                                RequestFocusWindow(config_window_);
                            }
                        }
                        if (ImGui::MenuItem(tr("gui.menu_windows_hide_all", "Hide all"))) {
                            for (auto *window : dock_windows_) {
                                if (window != nullptr) {
                                    window->SetOpen(false);
                                }
                            }
                        }
                        ImGui::Separator();
                        for (auto *window : dock_windows_) {
                            if (window == nullptr) {
                                continue;
                            }
                            const bool open = window->IsOpen();
                            if (ImGui::MenuItem(window->GetTitle().c_str(), nullptr, open)) {
                                window->SetOpen(!open);
                                if (!open) {
                                    RequestFocusWindow(window);
                                }
                            }
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu(tr("gui.menu_tools", "Tools"))) {
                        if (ImGui::MenuItem(tr("gui.tool_open_config", "Open config"))) {
                            if (config_window_ != nullptr) {
                                config_window_->SetOpen(true);
                                RequestFocusWindow(config_window_);
                            }
                        }
                        if (ImGui::MenuItem(tr("gui.tool_clear_toasts", "Clear notifications"))) {
                            if (notifications_window_ != nullptr) {
                                notifications_window_->Clear();
                            }
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu(tr("gui.menu_theme", "Theme"))) {
                        const bool is_d3   = (theme_ == GuiTheme::D3Dark);
                        const bool is_luna = (theme_ == GuiTheme::Luna);
                        if (ImGui::MenuItem(tr("gui.theme_d3", "D3 dark"), nullptr, is_d3)) {
                            set_theme(GuiTheme::D3Dark);
                        }
                        if (ImGui::MenuItem(tr("gui.theme_luna", "Luna"), nullptr, is_luna)) {
                            set_theme(GuiTheme::Luna);
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu(tr("gui.menu_help", "Help"))) {
                        ImGui::TextUnformatted(tr("gui.hotkey_toggle", "Hold + and - (0.5s) to toggle overlay visibility."));
                        ImGui::EndMenu();
                    }

                    ImGui::EndMenuBar();
                }

                dockspace_id_ = ImGui::GetID("d3hack_dockspace");
                ImGui::DockSpace(dockspace_id_, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
            }
            ImGui::End();

            ImGui::PopStyleVar(3);

            for (auto *window : dock_windows_) {
                if (window == nullptr) {
                    continue;
                }

                if (request_focus_ && (focus_window_ == nullptr || focus_window_ == window)) {
                    ImGui::SetNextWindowFocus();
                }

                const bool begin_visible = window->Render();
                if (request_focus_ && (focus_window_ == nullptr || focus_window_ == window) && begin_visible) {
                    request_focus_ = false;
                    focus_window_  = nullptr;
                    consumed_focus = true;
                }
            }
        } else {
            dockspace_id_ = 0;
        }

        if (can_draw) {
            for (auto *window : overlay_windows_) {
                if (window == nullptr) {
                    continue;
                }
                window->Render();
            }

            ImGuiIO const &io = ImGui::GetIO();
            focus_.nav_active            = io.NavActive;
            focus_.want_capture_mouse    = io.WantCaptureMouse;
            focus_.want_capture_keyboard = io.WantCaptureKeyboard;
            focus_.want_capture_gamepad  = io.NavActive || io.WantCaptureKeyboard;

            const bool toasts_visible      = (notifications_window_ != nullptr && notifications_window_->IsOpen());
            focus_.visible                 = can_draw && (overlay_visible_ || toasts_visible);
            focus_.should_block_game_input = can_draw && overlay_visible_;

            if (apply_default_layout) {
                layout_default_applied_ = true;
                layout_dirty_           = true;
            }
        }

        return consumed_focus;
    }

    void Overlay::AfterFrame() {
        if (!imgui_context_ready_) {
            return;
        }

        ImGuiIO &io = ImGui::GetIO();
        if (layout_reset_pending_) {
            ImGui::LoadIniSettingsFromMemory("", 0);
            layout_reset_pending_  = false;
            io.WantSaveIniSettings = true;
        }
        if (!layout_dirty_ && !io.WantSaveIniSettings) {
            return;
        }

        size_t      ini_size = 0;
        const char *ini_data = ImGui::SaveIniSettingsToMemory(&ini_size);
        const auto  text     = BuildLayoutText(theme_, ini_data, ini_size);

        std::string error;
        if (!WriteAllAtomic(kGuiLayoutPath, text, error)) {
            PRINT("[gui] layout save failed: %s", error.c_str());
            return;
        }

        io.WantSaveIniSettings = false;
        layout_dirty_          = false;
        layout_loaded_         = true;
    }

}  // namespace d3::gui2::ui
