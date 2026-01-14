#pragma once

#include <common.hpp>
#include <lib/util/sys/modules.hpp>
#include "program/build_info.hpp"
#include <cstring>
#include <string_view>

namespace exl::util {

    /* Add versions here if you want to take advantage of this feature. */
    enum class UserVersion {
        UNKNOWN = -1,
        DEFAULT = 0,
        /* Members can be assigned any constant of your choosing. */
        /* OTHER = 123 */
    };

    namespace impl {
        inline const char *FindStringInRange(const exl::util::Range &range, std::string_view needle) {
            if (needle.empty() || range.m_Size < needle.size())
                return nullptr;
            const char *start = reinterpret_cast<const char *>(range.m_Start);
            const char *end   = start + range.m_Size - needle.size();
            for (const char *p = start; p <= end; ++p) {
                if (std::memcmp(p, needle.data(), needle.size()) == 0)
                    return p;
            }
            return nullptr;
        }

        ALWAYS_INLINE UserVersion DetermineUserVersion() {
            const auto &mod = exl::util::GetMainModuleInfo();
            if (FindStringInRange(mod.m_Rodata, D3CLIENT_VER) != nullptr)
                return UserVersion::DEFAULT;
            return UserVersion::UNKNOWN;
        }
    }  // namespace impl
}  // namespace exl::util
