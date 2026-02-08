#pragma once

#include <string>

struct ImFont;

namespace d3::gui2::ui {
    class Overlay;
}  // namespace d3::gui2::ui

namespace d3::gui2::fonts::font_loader {
    struct PrepareResult {
        bool atlas_built       = false;
        bool atlas_invalidated = false;
    };

    auto PrepareFonts(d3::gui2::ui::Overlay &overlay, const std::string &desired_lang)
        -> PrepareResult;

    auto IsFontAtlasBuilt() -> bool;
    auto GetTitleFont() -> ImFont *;
    auto GetBodyFont() -> ImFont *;
}  // namespace d3::gui2::fonts::font_loader
