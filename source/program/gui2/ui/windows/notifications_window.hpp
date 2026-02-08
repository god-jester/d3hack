#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "program/gui2/ui/window.hpp"

namespace d3::gui2::ui::windows {

    struct Notification {
        std::string text;
        ImVec4      color         = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        float       ttl_s         = 0.0f;
        float       ttl_initial_s = 0.0f;
    };

    class NotificationsWindow : public ui::Window {
       public:
        NotificationsWindow();
        ~NotificationsWindow() override;

        void SetViewportSize(ImVec2 viewport_size);

        void Clear();

        void AddNotification(const ImVec4 &color, float ttl_s, const char *fmt, ...);
        void AddNotificationText(const ImVec4 &color, float ttl_s, std::string_view text);
        void SetPinnedOpen(bool pinned);
        bool IsPinnedOpen() const { return pinned_open_; }

       protected:
        void Update(float dt_s) override;
        void BeforeBegin() override;
        void RenderContents() override;

       private:
        ImVec2                    viewport_size_ {};
        std::vector<Notification> notifications_ {};
        bool                      pinned_open_ = false;
    };

}  // namespace d3::gui2::ui::windows
