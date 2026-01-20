#pragma once

#include "lib/diag/assert.hpp"

#define IM_ASSERT(_EXPR)        \
    do {                       \
        EXL_ABORT_UNLESS((_EXPR)); \
    } while (0)

// ImGui 1.92+ requires selecting at least one font loader backend at compile time.
// We use the built-in stb_truetype path (no external FreeType dependency).
#define IMGUI_ENABLE_STB_TRUETYPE

// Avoid pulling any file I/O into the Switch build (ImGui uses fopen/fread for ini/log helpers).
// Config lives on sd:/ and is handled by d3hack's own FS layer.
#define IMGUI_DISABLE_FILE_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_ALLOCATORS
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_WIN32_FUNCTIONS
#define IMGUI_DISABLE_TTY_FUNCTIONS
// #define IMGUI_DISABLE_DEFAULT_FONT
// #define IMGUI_DISABLE_OBSOLETE_FUNCTIONS

// Route stb_truetype allocations through ImGui's allocator.
#define STBTT_malloc(x, u) ((void)(u), IM_ALLOC(x))
#define STBTT_free(x, u)   ((void)(u), IM_FREE(x))
