#include "program/gui2/ui/window.hpp"

namespace d3::gui2::ui {

    Window::Window(std::string title, bool enabled_default) :
        title_(std::move(title)), enabled_(enabled_default) {
    }

    auto Window::Render() -> bool {
        if (!enabled_) {
            return false;
        }

        bool *open = GetOpenFlag();
        if (open != nullptr && !*open) {
            return false;
        }

        if (has_default_pos_) {
            ImGui::SetNextWindowPos(default_pos_, default_pos_cond_);
        }
        if (has_default_size_) {
            ImGui::SetNextWindowSize(default_size_, default_size_cond_);
        }

        BeforeBegin();

        const bool window_visible = ImGui::Begin(title_.c_str(), open, flags_);
        if (window_visible) {
            RenderContents();
        }

        // Always end the window even if Begin() returned false.
        ImGui::End();

        AfterEnd();

        return window_visible;
    }

    auto Window::IsEnabled() const -> bool {
        return enabled_;
    }

    void Window::SetEnabled(bool enabled) {
        if (enabled_ == enabled) {
            return;
        }

        enabled_ = enabled;

        if (enabled_) {
            OnEnable();
        } else {
            OnDisable();
        }
    }

    auto Window::IsOpen() const -> bool {
        const bool *open = const_cast<Window *>(this)->GetOpenFlag();
        return (open == nullptr) || *open;
    }

    auto Window::GetTitle() const -> const std::string & {
        return title_;
    }

    void Window::SetTitle(std::string title) {
        title_ = std::move(title);
    }

    void Window::SetDefaultPos(ImVec2 pos, ImGuiCond cond) {
        default_pos_      = pos;
        default_pos_cond_ = cond;
        has_default_pos_  = true;
    }

    void Window::ClearDefaultPos() {
        has_default_pos_ = false;
    }

    void Window::SetDefaultSize(ImVec2 size, ImGuiCond cond) {
        default_size_      = size;
        default_size_cond_ = cond;
        has_default_size_  = true;
    }

    void Window::ClearDefaultSize() {
        has_default_size_ = false;
    }

    auto Window::GetFlags() const -> ImGuiWindowFlags {
        return flags_;
    }

    void Window::SetFlags(ImGuiWindowFlags flags) {
        flags_ = flags;
    }

    auto Window::GetOpenFlag() -> bool * {
        return nullptr;
    }

}  // namespace d3::gui2::ui
