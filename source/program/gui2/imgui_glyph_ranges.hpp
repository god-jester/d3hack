#pragma once

#include "imgui/imgui.h"

namespace d3::imgui_overlay::glyph_ranges {
    const ImWchar *GetDefault();
    const ImWchar *GetKorean();
    const ImWchar *GetChineseFull();
    const ImWchar *GetChineseSimplifiedCommon();
    const ImWchar *GetJapanese();
    const ImWchar *GetCyrillic();
}  // namespace d3::imgui_overlay::glyph_ranges
