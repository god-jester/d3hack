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
        bool         show_metrics_     = false;
        bool         restart_required_ = false;
        int          section_index_    = 0;
        ui::Overlay &overlay_;
    };

}  // namespace d3::gui2::ui::windows
