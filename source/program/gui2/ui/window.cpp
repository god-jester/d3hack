#include "program/gui2/ui/window.hpp"

#include <algorithm>
#include <utility>

#include <imgui/imgui.h>

#include "program/gui2/imgui_overlay.hpp"

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

        const ImGuiStyle &style = ImGui::GetStyle();

        // NOTE: ImGui's built-in window close button ("X") is sized from g.FontSize (square).
        // FramePadding changes title bar height and the button's position, but not the "X" size.
        //
        // We make the "X" larger by temporarily increasing the base font size for Begin()
        // (title bar only), and we tighten title-bar-only padding so the button sits closer
        // to the top-right corner.
        //
        // If this still isn't quite right, tweak these constants:
        //  - kTitleBarFontScale: increase to make the "X" bigger.
        //  - kTitleBarPadXScale/YScale: decrease to move the button closer to the edge.
        //  - kTitleBarInnerXScale: decrease to reduce gap between the "X" and the title text.
        constexpr float kTitleBarFontScale   = 1.45f;
        constexpr float kTitleBarPadXScale   = 0.30f;
        constexpr float kTitleBarPadYScale   = 0.40f;
        constexpr float kTitleBarInnerXScale = 0.65f;

        const float title_font_size_unscaled =
            std::max(style.FontSizeBase, 1.0f) * kTitleBarFontScale;

        const ImVec2 title_frame_padding(
            std::max(style.FramePadding.x * kTitleBarPadXScale, 1.0f),
            std::max(style.FramePadding.y * kTitleBarPadYScale, 1.0f)
        );
        const ImVec2 title_item_inner_spacing(
            std::max(style.ItemInnerSpacing.x * kTitleBarInnerXScale, 1.0f),
            style.ItemInnerSpacing.y
        );

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, title_frame_padding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, title_item_inner_spacing);

        ImFont *title_font = d3::imgui_overlay::GetTitleFont();
        ImFont *body_font  = d3::imgui_overlay::GetBodyFont();
        ImGui::PushFont(title_font, title_font_size_unscaled);
        const bool window_visible = ImGui::Begin(title_.c_str(), open, flags_);
        ImGui::PopFont();
        ImGui::PopStyleVar(2);

        if (window_visible) {
            if (body_font != nullptr) {
                ImGui::PushFont(body_font, 0.0f);
            }
            RenderContents();
            if (body_font != nullptr) {
                ImGui::PopFont();
            }
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
