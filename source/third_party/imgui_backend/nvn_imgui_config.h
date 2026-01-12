#pragma once

#include "lib/diag/assert.hpp"

#define IM_ASSERT(_EXPR)  EXL_ASSERT(_EXPR)

// Avoid pulling file I/O into the Switch build (ImGui uses fopen/fread for ini/log helpers).
// Config lives on sd:/ and is handled by d3hack's own FS layer.
#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
