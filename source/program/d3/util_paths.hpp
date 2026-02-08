#pragma once

#include <cstddef>

namespace d3 {

    // NOTE: keep these constant-initialized.
    inline constexpr char g_szBaseDir[]         = "sd:/config/d3hack-nx";
    inline constexpr char g_szPubfileCacheDir[] = "sd:/config/d3hack-nx/pubfiles";

    auto GetFilenameTail(const char *path) -> const char *;
    bool BuildPubfileCachePath(char *out, size_t out_size, const char *filename);

    void EnsurePubfileCacheDir();

}  // namespace d3
