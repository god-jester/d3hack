#pragma once

#include <string>
#include <string_view>

namespace d3::fs_util {

    // Best-effort create: sd:/config and sd:/config/d3hack-nx, then verify the hack dir exists.
    auto EnsureConfigRootDirs(std::string &error_out) -> bool;

    auto DoesFileExist(const char *path) -> bool;

    // Atomic write pattern:
    // - write <path>.tmp
    // - rename existing <path> to <path>.bak (best-effort)
    // - rename tmp to final
    // - delete bak
    auto WriteAllAtomic(const char *path, std::string_view text, std::string_view what, std::string &error_out) -> bool;

}  // namespace d3::fs_util
