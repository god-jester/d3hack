#include "program/build_stamp.hpp"

#include "program/build_git.hpp"
#include "program/d3/setting.hpp"

namespace d3::build_stamp {
#if D3HACK_GIT_DIRTY
#define D3HACK_GIT_DIRTY_SUFFIX " dirty"
#else
#define D3HACK_GIT_DIRTY_SUFFIX ""
#endif
#define D3HACK_BUILD_ID      "g" D3HACK_GIT_COMMIT D3HACK_GIT_DIRTY_SUFFIX
#define D3HACK_BUILD_DISPLAY " (" D3HACK_BUILD_ID " built " D3HACK_BUILD_TIMESTAMP ")"

    const char kBuildId[]        = D3HACK_BUILD_ID;
    const char kBuildTimestamp[] = D3HACK_BUILD_TIMESTAMP;
    const char kBuildDisplay[]   = D3HACK_BUILD_DISPLAY;
    const char kVersionLine[]    = D3HACK_VER D3HACK_BUILD_DISPLAY;
    const char kAutosaveString[] = CRLF D3HACK_VER D3HACK_BUILD_DISPLAY CRLF CRLF D3HACK_DESC CRLF CRLF CRLF;

#undef D3HACK_BUILD_DISPLAY
#undef D3HACK_BUILD_ID
#undef D3HACK_GIT_DIRTY_SUFFIX
}  // namespace d3::build_stamp
