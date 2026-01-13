#pragma once

#include "program/gui2/ui/overlay.hpp"

namespace d3::imgui_overlay {
    void Initialize();
    void PrepareFonts();

    const d3::gui2::ui::GuiFocusState& GetGuiFocusState();
}
