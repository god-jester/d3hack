#pragma once

#include "program/gui2/ui/overlay.hpp"

namespace d3::imgui_overlay {
    void Initialize();
    void PrepareFonts();

    const d3::gui2::ui::GuiFocusState &GetGuiFocusState();
    // Post a formatted notification to the overlay notifications queue.
    // Safe to call from other modules; does nothing if overlay/windows are not yet created.
    void PostOverlayNotification(const ImVec4 &color, float ttl_s, const char *fmt, ...);
}  // namespace d3::imgui_overlay
