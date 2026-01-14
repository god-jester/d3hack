#pragma once

#include <string>
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

        void SetViewportSize(ImVec2 viewport_size);

        void Tick(float dt_s);
        void Clear();

        void AddNotification(const ImVec4 &color, float ttl_s, const char *fmt, ...);

       protected:
        bool *GetOpenFlag() override;
        void  BeforeBegin() override;
        void  RenderContents() override;

       private:
        bool                      open_ = false;
        ImVec2                    viewport_size_ {};
        std::vector<Notification> notifications_ {};
    };

}  // namespace d3::gui2::ui::windows
