#include "program/romfs_assets.hpp"

#include <cstring>

#include "lib/diag/assert.hpp"
#include "lib/alloc.hpp"
#include "nn/fs.hpp"
#include "program/d3/setting.hpp"

namespace d3::romfs {
namespace {

constexpr size_t kRomfsMountAlign = 0x1000;
constexpr const char* kRomfsMountName = "romfs";

static bool s_romfs_mount_tried = false;
static bool s_romfs_mounted = false;
static void* s_romfs_mount_cache = nullptr;
static size_t s_romfs_mount_cache_size = 0;

static size_t AlignUp(size_t v, size_t a) {
    return (v + (a - 1)) & ~(a - 1);
}

static void EnsureRomfsMountedOnce() {
    if (s_romfs_mounted || s_romfs_mount_tried) {
        return;
    }

    s_romfs_mount_tried = true;

    u64 cache_size_u64 = 0;
    auto rc = nn::fs::QueryMountRomCacheSize(&cache_size_u64);
    if (R_FAILED(rc) || cache_size_u64 == 0) {
        PRINT("[romfs] QueryMountRomCacheSize failed (rc=0x%x)", rc);
        return;
    }

    s_romfs_mount_cache_size = static_cast<size_t>(cache_size_u64);
    const size_t alloc_size = AlignUp(s_romfs_mount_cache_size, kRomfsMountAlign);

    s_romfs_mount_cache = aligned_alloc(kRomfsMountAlign, alloc_size);
    if (s_romfs_mount_cache == nullptr) {
        PRINT("[romfs] aligned_alloc failed (size=%lu)", static_cast<unsigned long>(alloc_size));
        return;
    }

    rc = nn::fs::MountRom(kRomfsMountName, s_romfs_mount_cache, static_cast<ulong>(s_romfs_mount_cache_size));
    if (R_FAILED(rc)) {
        PRINT("[romfs] MountRom('%s') failed (rc=0x%x)", kRomfsMountName, rc);
        return;
    }

    s_romfs_mounted = true;
    PRINT("[romfs] Mounted %s:/ (cache=%lu bytes)", kRomfsMountName, static_cast<unsigned long>(s_romfs_mount_cache_size));
}

static bool ReadFileNnFs(const char* path, std::vector<unsigned char>& out, size_t max_size) {
    nn::fs::FileHandle fh{};
    auto rc = nn::fs::OpenFile(&fh, path, nn::fs::OpenMode_Read);
    if (R_FAILED(rc)) {
        return false;
    }

    long size = 0;
    rc = nn::fs::GetFileSize(&size, fh);
    if (R_FAILED(rc) || size <= 0) {
        nn::fs::CloseFile(fh);
        return false;
    }

    const size_t size_st = static_cast<size_t>(size);
    if (size_st > max_size) {
        nn::fs::CloseFile(fh);
        return false;
    }

    out.resize(size_st);
    rc = nn::fs::ReadFile(fh, 0, out.data(), static_cast<ulong>(size_st));
    nn::fs::CloseFile(fh);

    if (R_FAILED(rc)) {
        out.clear();
        return false;
    }

    return true;
}

}  // namespace

bool ReadFileToBytes(const char* path, std::vector<unsigned char>& out, size_t max_size) {
    out.clear();

    if (path == nullptr || path[0] == '\0') {
        return false;
    }

    // "romfs:/..." is a common convention in modding tools, but the game must have a corresponding
    // mount name registered. We mount our own "romfs" device once, then only attempt OpenFile when
    // the mount is known to be in place (some titles abort on invalid/unmounted device names).
    if (std::strncmp(path, "romfs:/", 7) == 0) {
        EnsureRomfsMountedOnce();
        if (!s_romfs_mounted) {
            return false;
        }

        return ReadFileNnFs(path, out, max_size);
    }

    return ReadFileNnFs(path, out, max_size);
}

bool ReadFileToString(const char* path, std::string& out, size_t max_size) {
    out.clear();

    std::vector<unsigned char> bytes;
    if (!ReadFileToBytes(path, bytes, max_size)) {
        return false;
    }

    out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

}  // namespace d3::romfs
