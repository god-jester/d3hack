#pragma once

// This header wraps an auto-generated build header (from CMake) that encodes git
// metadata. It must be safe to include even when the generated file is missing.

#if defined(__has_include)
#if __has_include("d3hack_build_git.hpp")
#include "d3hack_build_git.hpp"
#endif
#endif

#ifndef D3HACK_GIT_DESCRIBE
#define D3HACK_GIT_DESCRIBE "unknown"
#endif

#ifndef D3HACK_GIT_COMMIT
#define D3HACK_GIT_COMMIT "unknown"
#endif

#ifndef D3HACK_GIT_DIRTY
#define D3HACK_GIT_DIRTY 0
#endif
