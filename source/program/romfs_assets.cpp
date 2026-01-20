#include "program/romfs_assets.hpp"

#include <algorithm>
#include <cstring>
#include <string_view>

#include "lib/alloc.hpp"
#include "nn/fs.hpp"
#include "program/d3/setting.hpp"

namespace d3::romfs {
    namespace {

        constexpr size_t      kRomfsMountAlign = 0x1000;
        constexpr const char *kRomfsMountName  = "romfs";

        static bool   s_romfs_mount_tried      = false;
        static bool   s_romfs_mounted          = false;
        static void  *s_romfs_mount_cache      = nullptr;
        static size_t s_romfs_mount_cache_size = 0;

        static size_t AlignUp(size_t v, size_t a) {
            return (v + (a - 1)) & ~(a - 1);
        }

        static void EnsureRomfsMountedOnce() {
            if (s_romfs_mounted || s_romfs_mount_tried) {
                return;
            }

            s_romfs_mount_tried = true;

            u64  cache_size_u64 = 0;
            auto rc             = nn::fs::QueryMountRomCacheSize(&cache_size_u64);
            if (R_FAILED(rc) || cache_size_u64 == 0) {
                PRINT("[romfs] QueryMountRomCacheSize failed (rc=0x%x)", rc);
                return;
            }

            s_romfs_mount_cache_size = static_cast<size_t>(cache_size_u64);
            const size_t alloc_size  = AlignUp(s_romfs_mount_cache_size, kRomfsMountAlign);

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

        static bool ReadFileNnFs(const char *path, std::vector<unsigned char> &out, size_t max_size) {
            nn::fs::FileHandle fh {};
            auto               rc = nn::fs::OpenFile(&fh, path, nn::fs::OpenMode_Read);
            if (R_FAILED(rc)) {
                return false;
            }

            long size = 0;
            rc        = nn::fs::GetFileSize(&size, fh);
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

        static bool EndsWithCaseInsensitive(std::string_view s, std::string_view suffix) {
            if (suffix.size() > s.size()) {
                return false;
            }

            s.remove_prefix(s.size() - suffix.size());

            for (size_t i = 0; i < suffix.size(); ++i) {
                const auto a  = static_cast<unsigned char>(s[i]);
                const auto b  = static_cast<unsigned char>(suffix[i]);
                const auto la = static_cast<char>((a >= 'A' && a <= 'Z') ? (a - 'A' + 'a') : a);
                const auto lb = static_cast<char>((b >= 'A' && b <= 'Z') ? (b - 'A' + 'a') : b);
                if (la != lb) {
                    return false;
                }
            }

            return true;
        }

    }  // namespace

    bool ReadFileToBytes(const char *path, std::vector<unsigned char> &out, size_t max_size) {
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

    bool ReadFileToString(const char *path, std::string &out, size_t max_size) {
        out.clear();

        std::vector<unsigned char> bytes;
        if (!ReadFileToBytes(path, bytes, max_size)) {
            return false;
        }

        out.assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        return true;
    }

    bool FindFirstFileWithSuffix(const char *dir_path, const char *suffix, std::string &out_path) {
        out_path.clear();

        if (dir_path == nullptr || dir_path[0] == '\0' || suffix == nullptr || suffix[0] == '\0') {
            return false;
        }

        if (std::strncmp(dir_path, "romfs:/", 7) == 0) {
            EnsureRomfsMountedOnce();
            if (!s_romfs_mounted) {
                return false;
            }
        }

        nn::fs::DirectoryHandle dh {};
        auto                    rc = nn::fs::OpenDirectory(&dh, dir_path, nn::fs::OpenDirectoryMode_File);
        if (R_FAILED(rc)) {
            // Some nn::fs paths want a trailing slash; retry once.
            std::string retry = dir_path;
            if (retry.back() != '/') {
                retry.push_back('/');
            }
            rc = nn::fs::OpenDirectory(&dh, retry.c_str(), nn::fs::OpenDirectoryMode_File);
            if (R_FAILED(rc)) {
                return false;
            }
        }

        s64 entry_count = 0;
        rc              = nn::fs::GetDirectoryEntryCount(&entry_count, dh);
        if (R_FAILED(rc) || entry_count <= 0) {
            nn::fs::CloseDirectory(dh);
            return false;
        }

        constexpr s64 kMaxEntries = 512;
        entry_count               = std::min(entry_count, kMaxEntries);

        std::vector<nn::fs::DirectoryEntry> entries(static_cast<size_t>(entry_count));
        s64                                 read_count = 0;
        rc                                             = nn::fs::ReadDirectory(&read_count, entries.data(), dh, entry_count);
        nn::fs::CloseDirectory(dh);

        if (R_FAILED(rc) || read_count <= 0) {
            return false;
        }

        std::vector<std::string> matches;
        matches.reserve(static_cast<size_t>(read_count));
        for (s64 i = 0; i < read_count; ++i) {
            const char *name = entries[static_cast<size_t>(i)].m_Name;
            if (name == nullptr || name[0] == '\0') {
                continue;
            }
            if (EndsWithCaseInsensitive(std::string_view {name}, std::string_view {suffix})) {
                matches.emplace_back(name);
            }
        }

        if (matches.empty()) {
            return false;
        }

        std::sort(matches.begin(), matches.end());

        out_path = dir_path;
        if (!out_path.empty() && out_path.back() != '/') {
            out_path.push_back('/');
        }
        out_path += matches.front();
        return true;
    }

}  // namespace d3::romfs
