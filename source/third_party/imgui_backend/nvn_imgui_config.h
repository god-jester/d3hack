#pragma once

#include "lib/diag/assert.hpp"

#define IM_ASSERT(_EXPR)           \
    do {                          \
        EXL_ASSERT((_EXPR));      \
    } while (0)

// ImGui 1.92+ requires selecting at least one font loader backend at compile time.
// We use the built-in stb_truetype path (no external FreeType dependency).
#define IMGUI_ENABLE_STB_TRUETYPE

// Avoid pulling any file I/O into the Switch build (ImGui uses fopen/fread for ini/log helpers).
// Config lives on sd:/ and is handled by d3hack's own FS layer.
#define IMGUI_DISABLE_FILE_FUNCTIONS
