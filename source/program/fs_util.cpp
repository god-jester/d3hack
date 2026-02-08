#include "program/fs_util.hpp"

#include "types.h"
#include "nn/fs.hpp"  // IWYU pragma: keep
#include "lib/nx/result.h"

namespace d3::fs_util {
    namespace {
        constexpr const char *kRootDir = "sd:/config";
        constexpr const char *kHackDir = "sd:/config/d3hack-nx";

        auto DoesFileExistImpl(const char *path, nn::fs::DirectoryEntryType expected) -> bool {
            if (path == nullptr || path[0] == '\0') {
                return false;
            }
            nn::fs::DirectoryEntryType type {};
            const auto                 rc = nn::fs::GetEntryType(&type, path);
            if (R_FAILED(rc)) {
                return false;
            }
            return type == expected;
        }
    }  // namespace

    auto EnsureConfigRootDirs(std::string &error_out) -> bool {
        (void)nn::fs::CreateDirectory(kRootDir);
        (void)nn::fs::CreateDirectory(kHackDir);

        if (!DoesFileExistImpl(kHackDir, nn::fs::DirectoryEntryType_Directory)) {
            error_out = "Failed to ensure config directory exists";
            return false;
        }

        return true;
    }

    auto DoesFileExist(const char *path) -> bool {
        return DoesFileExistImpl(path, nn::fs::DirectoryEntryType_File);
    }

    auto WriteAllAtomic(const char *path, std::string_view text, std::string_view what, std::string &error_out) -> bool {
        if (!EnsureConfigRootDirs(error_out)) {
            return false;
        }

        const std::string tmp_path = std::string(path) + ".tmp";
        const std::string bak_path = std::string(path) + ".bak";

        (void)nn::fs::DeleteFile(tmp_path.c_str());

        auto rc = nn::fs::CreateFile(tmp_path.c_str(), static_cast<s64>(text.size()));
        if (R_FAILED(rc)) {
            error_out = "Failed to create temp ";
            error_out += std::string(what);
            return false;
        }

        nn::fs::FileHandle fh {};
        rc = nn::fs::OpenFile(&fh, tmp_path.c_str(), nn::fs::OpenMode_Write);
        if (R_FAILED(rc)) {
            (void)nn::fs::DeleteFile(tmp_path.c_str());
            error_out = "Failed to open temp ";
            error_out += std::string(what);
            return false;
        }

        const auto opt = nn::fs::WriteOption::CreateOption(nn::fs::WriteOptionFlag_Flush);
        rc             = nn::fs::WriteFile(fh, 0, text.data(), static_cast<u64>(text.size()), opt);
        if (R_FAILED(rc)) {
            nn::fs::CloseFile(fh);
            (void)nn::fs::DeleteFile(tmp_path.c_str());
            error_out = "Failed to write temp ";
            error_out += std::string(what);
            return false;
        }

        (void)nn::fs::FlushFile(fh);
        nn::fs::CloseFile(fh);

        if (DoesFileExist(path)) {
            (void)nn::fs::DeleteFile(bak_path.c_str());
            rc = nn::fs::RenameFile(path, bak_path.c_str());
            if (R_FAILED(rc)) {
                (void)nn::fs::DeleteFile(tmp_path.c_str());
                error_out = "Failed to backup existing ";
                error_out += std::string(what);
                return false;
            }
        }

        rc = nn::fs::RenameFile(tmp_path.c_str(), path);
        if (R_FAILED(rc)) {
            if (DoesFileExist(bak_path.c_str())) {
                (void)nn::fs::RenameFile(bak_path.c_str(), path);
            }
            (void)nn::fs::DeleteFile(tmp_path.c_str());
            error_out = "Failed to move temp ";
            error_out += std::string(what);
            error_out += " into place";
            return false;
        }

        if (DoesFileExist(bak_path.c_str())) {
            (void)nn::fs::DeleteFile(bak_path.c_str());
        }

        return true;
    }

}  // namespace d3::fs_util
