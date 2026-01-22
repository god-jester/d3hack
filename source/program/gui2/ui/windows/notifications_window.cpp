#include "program/gui2/ui/windows/notifications_window.hpp"

#include <array>
#include <algorithm>
#include <cstdarg>
#include <cstdio>

namespace d3::gui2::ui::windows {

    NotificationsWindow::NotificationsWindow() :
        Window("Notifications") {
        SetFlags(ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
    }

    void NotificationsWindow::SetViewportSize(ImVec2 viewport_size) {
        viewport_size_ = viewport_size;
    }

    void NotificationsWindow::Tick(float dt_s) {
        if (notifications_.empty()) {
            open_ = false;
            return;
        }

        for (auto &n : notifications_) {
            n.ttl_s -= dt_s;
        }

        auto removed = std::ranges::remove_if(notifications_, [](const Notification &n) -> bool { return n.ttl_s <= 0.0f; });
        notifications_.erase(removed.begin(), removed.end());

        open_ = !notifications_.empty();
    }

    void NotificationsWindow::Clear() {
        notifications_.clear();
        open_ = false;
    }

    void NotificationsWindow::AddNotification(const ImVec4 &color, float ttl_s, const char *fmt, ...) {
        std::array<char, 512> buf {};

        va_list vl;
        va_start(vl, fmt);
        vsnprintf(buf.data(), buf.size(), fmt, vl);
        va_end(vl);

        buf.back() = '\0';

        Notification n {};
        n.text          = buf.data();
        n.color         = color;
        n.ttl_s         = ttl_s;
        n.ttl_initial_s = ttl_s;
        notifications_.push_back(std::move(n));
        open_ = true;
    }

    auto NotificationsWindow::GetOpenFlag() -> bool * {
        return &open_;
    }

    void NotificationsWindow::BeforeBegin() {
        if (viewport_size_.x > 0.0f) {
            ImGui::SetNextWindowPos(ImVec2(viewport_size_.x - 20.0f, 20.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        }
        ImGui::SetNextWindowBgAlpha(0.35f);
    }

    void NotificationsWindow::RenderContents() {
        if (notifications_.empty()) {
            return;
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 340.0f);

        for (const auto &n : notifications_) {
            float fade = 1.0f;
            if (n.ttl_s < 0.5f) {
                fade = std::max(0.0f, n.ttl_s / 0.5f);
            }

            ImVec4 c = n.color;
            c.w *= fade;
            ImGui::TextColored(c, "%s", n.text.c_str());
        }

        ImGui::PopTextWrapPos();
    }

}  // namespace d3::gui2::ui::windows
