#pragma once

#include "program/gui2/ui/window.hpp"

namespace d3::gui2::ui {
    class Overlay;
}

namespace d3::gui2::ui::windows {

    class ConfigWindow : public ui::Window {
       public:
        explicit ConfigWindow(ui::Overlay &overlay);

       protected:
        void BeforeBegin() override;
        void AfterEnd() override;
        void RenderContents() override;

       private:
        enum class DockEdge {
            Left,
            Right,
            Top,
            Bottom,
        };

        void UpdateDockSwipe(ImVec2 window_pos, ImVec2 window_size);

        bool         show_metrics_         = false;
        bool         restart_required_     = false;
        char         restart_note_[256]    = {};
        bool         lang_restart_pending_ = false;
        int          section_index_        = 0;
        bool         dock_swipe_active_    = false;
        bool         dock_swipe_triggered_ = false;
        DockEdge     dock_swipe_edge_      = DockEdge::Left;
        ImVec2       dock_swipe_start_ {};
        ui::Overlay &overlay_;
    };

}  // namespace d3::gui2::ui::windows
