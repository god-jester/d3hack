#include "program/tagnx.hpp"

#include "lib/hook/replace.hpp"
#include "lib/hook/trampoline.hpp"
#include "lib/reloc/reloc.hpp"
#include "lib/util/modules.hpp"
#include "lib/log/logger_mgr.hpp"
#include "program/logging.hpp"

#include <cstdint>
#include <cstring>

// ported from https://gbatemp.net/threads/tagnx-a-wip-to-play-some-online-third-party-games-on-banned-switches.665752/
namespace d3::tagnx {
    namespace {
        constexpr char kDummyToken[] =
            "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJhdWQiOiJlZDllMmYwNWQyODZmN2I4Iiwic3ViIjoiMDAwMDAwMDAwMDAwMDAwMCIsIm5pbnRlbmRvIjp7ImF0IjoxOTk5OTk5OTk5LCJwZGkiOiIwMDAwMDAwMC0wMDAwLTAwMDAtMDAwMC0wMDAwMDAwMDAwMDAiLCJhdiI6IjAwMDAiLCJhaSI6IjAwMDAwMDAwMDAwMDAwMDAiLCJlZGkiOiIwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMCJ9LCJpc3MiOiJodHRwczovL2UwZDY3YzUwOWZiMjAzODU4ZWJjYjJmZTNmODhjMmFhLmJhYXMubmludGVuZG8uY29tIiwidHlwIjoiaWRfdG9rZW4iLCJleHAiOjE5OTk5OTk5OTksImlhdCI6MTk5OTk5OTk5OSwiYnM6ZGlkIjoiMDAwMDAwMDAwMDAwMDAwMCIsImp0aSI6IjAwMDAwMDAwLTAwMDAtMDAwMC0wMDAwLTAwMDAwMDAwMDAwMCJ9.Q0Hl0DOFQ0AUg728GZIrr9HjgYjdTHli8uuB_sl0MIg";

        constexpr char kSymEnsureAccount[] =
            "_ZN2nn7account36EnsureNetworkServiceAccountAvailableERKNS0_10UserHandleE";
        constexpr char kSymIsAccountAvailable[] =
            "_ZN2nn7account32IsNetworkServiceAccountAvailableEPbRKNS0_10UserHandleE";
        constexpr char kSymAsyncContextGetResult[] =
            "_ZN2nn7account12AsyncContext9GetResultEv";
        constexpr char kSymGetNetworkServiceAccountId[] =
            "_ZN2nn7account26GetNetworkServiceAccountIdEPNS0_23NetworkServiceAccountIdERKNS0_10UserHandleE";
        constexpr char kSymLoadNSAIDTokenCache[] =
            "_ZN2nn7account37LoadNetworkServiceAccountIdTokenCacheEPmPcmRKNS0_10UserHandleE";
        constexpr char kSymGetNetworkServiceLicenseKind[] =
            "_ZN2nn7account37AsyncNetworkServiceLicenseInfoContext28GetNetworkServiceLicenseKindEv";
        constexpr char kSymLoadModule[] =
            "_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi";

        struct HookState {
            bool ensure_account_installed       = false;
            bool is_account_available_installed = false;
            bool async_context_result_installed = false;
            bool get_account_id_installed       = false;
            bool load_token_cache_installed     = false;
            bool get_license_kind_installed     = false;
            bool load_module_installed          = false;

            bool ensure_account_missing_logged       = false;
            bool is_account_available_missing_logged = false;
            bool async_context_result_missing_logged = false;
            bool get_account_id_missing_logged       = false;
            bool load_token_cache_missing_logged     = false;
            bool get_license_kind_missing_logged     = false;
            bool load_module_missing_logged          = false;
        };

        HookState g_hook_state {};

        static auto LookupSymbolAddress(const char *name) -> void * {
            for (int i = static_cast<int>(exl::util::ModuleIndex::Start);
                 i < static_cast<int>(exl::util::ModuleIndex::End);
                 ++i) {
                const auto index = static_cast<exl::util::ModuleIndex>(i);
                if (!exl::util::HasModule(index)) {
                    continue;
                }
                const auto *sym = exl::reloc::GetSymbol(index, name);
                if (sym == nullptr || sym->st_value == 0) {
                    continue;
                }
                const auto &info = exl::util::GetModuleInfo(index);
                return reinterpret_cast<void *>(info.m_Total.m_Start + sym->st_value);
            }
            return nullptr;
        }

        template<typename HookT>
        static void InstallHookIfFound(const char *symbol, bool &installed, bool &missing_logged) {
            if (installed) {
                return;
            }
            void *addr = LookupSymbolAddress(symbol);
            if (addr == nullptr) {
                if (!missing_logged) {
                    exl::log::PrintFmt(EXL_LOG_PREFIX "TagNX: symbol not found (will retry if possible): %s", symbol);
                    missing_logged = true;
                }
                return;
            }
            HookT::InstallAtPtr(reinterpret_cast<uintptr_t>(addr));
            installed = true;
            exl::log::PrintFmt(EXL_LOG_PREFIX "TagNX: hook installed: %s", symbol);
        }

        static void TryInstallHooks();

        HOOK_DEFINE_REPLACE(EnsureNetworkServiceAccountAvailable) {
            static auto Callback(std::int64_t /*user_handle*/) -> std::int64_t {
                return 0;
            }
        };

        HOOK_DEFINE_REPLACE(IsNetworkServiceAccountAvailable) {
            static auto Callback(bool *out_available, std::int64_t /*user_handle*/) -> std::int64_t {
                if (out_available != nullptr) {
                    *out_available = true;
                }
                return 0;
            }
        };

        HOOK_DEFINE_REPLACE(AsyncContextGetResult) {
            static auto Callback(std::int64_t * /*ctx*/) -> std::int64_t {
                return 0;
            }
        };

        HOOK_DEFINE_REPLACE(GetNetworkServiceAccountId) {
            static auto Callback(std::int64_t *out_id, std::int64_t /*user_handle*/) -> std::int64_t {
                if (out_id != nullptr) {
                    *out_id = 0x69;
                }
                return 0;
            }
        };

        HOOK_DEFINE_REPLACE(LoadNSAIDTokenCache) {
            static auto Callback(unsigned long *out_size, const char **out_ptr, unsigned long /*buf_size*/, std::int64_t /*user_handle*/) -> std::int64_t {
                if (out_size != nullptr) {
                    *out_size = static_cast<unsigned long>(std::strlen(kDummyToken));
                }
                if (out_ptr != nullptr) {
                    *out_ptr = kDummyToken;
                }
                return 0;
            }
        };

        HOOK_DEFINE_REPLACE(GetNetworkServiceLicenseKind) {
            static auto Callback(std::int64_t /*ctx*/) -> std::int64_t {
                return 1;
            }
        };

        HOOK_DEFINE_TRAMPOLINE(LoadModule) {
            static auto Callback(std::int64_t a1, std::int64_t a2, std::int64_t a3, std::int64_t a4, int a5)
                -> std::int64_t {
                TryInstallHooks();
                auto ret = Orig(a1, a2, a3, a4, a5);
                TryInstallHooks();
                return ret;
            }
        };

        static void TryInstallHooks() {
            InstallHookIfFound<EnsureNetworkServiceAccountAvailable>(
                kSymEnsureAccount,
                g_hook_state.ensure_account_installed,
                g_hook_state.ensure_account_missing_logged
            );
            InstallHookIfFound<IsNetworkServiceAccountAvailable>(
                kSymIsAccountAvailable,
                g_hook_state.is_account_available_installed,
                g_hook_state.is_account_available_missing_logged
            );
            InstallHookIfFound<AsyncContextGetResult>(
                kSymAsyncContextGetResult,
                g_hook_state.async_context_result_installed,
                g_hook_state.async_context_result_missing_logged
            );
            InstallHookIfFound<GetNetworkServiceAccountId>(
                kSymGetNetworkServiceAccountId,
                g_hook_state.get_account_id_installed,
                g_hook_state.get_account_id_missing_logged
            );
            InstallHookIfFound<LoadNSAIDTokenCache>(
                kSymLoadNSAIDTokenCache,
                g_hook_state.load_token_cache_installed,
                g_hook_state.load_token_cache_missing_logged
            );
            InstallHookIfFound<GetNetworkServiceLicenseKind>(
                kSymGetNetworkServiceLicenseKind,
                g_hook_state.get_license_kind_installed,
                g_hook_state.get_license_kind_missing_logged
            );
        }

        static void TryInstallLoadModuleHook() {
            InstallHookIfFound<LoadModule>(
                kSymLoadModule,
                g_hook_state.load_module_installed,
                g_hook_state.load_module_missing_logged
            );
        }
    }  // namespace

    void InstallHooks() {
        TryInstallHooks();
        TryInstallLoadModuleHook();
    }
}  // namespace d3::tagnx

namespace d3::tagnx::curl {
    namespace {
        static uintptr_t g_curl_easy_init_addr        = 0;
        static uintptr_t g_curl_easy_setopt_addr      = 0;
        static uintptr_t g_curl_slist_append_addr     = 0;
        static uintptr_t g_curl_easy_perform_addr     = 0;
        static uintptr_t g_curl_multi_init_addr       = 0;
        static uintptr_t g_curl_multi_add_handle_addr = 0;
        static uintptr_t g_curl_multi_perform_addr    = 0;

        static bool g_curl_easy_init_missing_logged        = false;
        static bool g_curl_easy_setopt_missing_logged      = false;
        static bool g_curl_slist_append_missing_logged     = false;
        static bool g_curl_easy_perform_missing_logged     = false;
        static bool g_curl_multi_init_missing_logged       = false;
        static bool g_curl_multi_add_handle_missing_logged = false;
        static bool g_curl_multi_perform_missing_logged    = false;

        static auto ResolveSymbolCached(const char *name, uintptr_t &cache, bool &missing_logged) -> uintptr_t {
            if (cache != 0) {
                return cache;
            }
            void *addr = LookupSymbolAddress(name);
            if (addr == nullptr) {
                if (!missing_logged) {
                    exl::log::PrintFmt(EXL_LOG_PREFIX "TagNX curl: symbol not found: %s", name);
                    missing_logged = true;
                }
                return 0;
            }
            cache = reinterpret_cast<uintptr_t>(addr);
            return cache;
        }
    }  // namespace

    CURL *curl_easy_init() {
        const auto addr =
            ResolveSymbolCached("curl_easy_init", g_curl_easy_init_addr, g_curl_easy_init_missing_logged);
        if (addr == 0) {
            return nullptr;
        }
        return reinterpret_cast<CURL *(*)()>(addr)();
    }

    void curl_easy_setopt(CURL *curl, int opt, void *data) {
        const auto addr =
            ResolveSymbolCached("curl_easy_setopt", g_curl_easy_setopt_addr, g_curl_easy_setopt_missing_logged);
        if (addr == 0) {
            return;
        }
        return reinterpret_cast<void (*)(CURL *, int, void *)>(addr)(curl, opt, data);
    }

    curl_slist *curl_slist_append(curl_slist *headers, const char *data) {
        const auto addr =
            ResolveSymbolCached("curl_slist_append", g_curl_slist_append_addr, g_curl_slist_append_missing_logged);
        if (addr == 0) {
            return nullptr;
        }
        return reinterpret_cast<curl_slist *(*)(curl_slist *, const char *)>(addr)(headers, data);
    }

    int curl_easy_perform(CURL *curl) {
        const auto addr =
            ResolveSymbolCached("curl_easy_perform", g_curl_easy_perform_addr, g_curl_easy_perform_missing_logged);
        if (addr == 0) {
            return 0;
        }
        return reinterpret_cast<int (*)(CURL *)>(addr)(curl);
    }

    std::int64_t curl_multi_init() {
        const auto addr =
            ResolveSymbolCached("curl_multi_init", g_curl_multi_init_addr, g_curl_multi_init_missing_logged);
        if (addr == 0) {
            return 0;
        }
        return reinterpret_cast<std::int64_t (*)()>(addr)();
    }

    void curl_multi_add_handle(std::int64_t multi_handle, CURL *easy_handle) {
        const auto addr = ResolveSymbolCached(
            "curl_multi_add_handle",
            g_curl_multi_add_handle_addr,
            g_curl_multi_add_handle_missing_logged
        );
        if (addr == 0) {
            return;
        }
        return reinterpret_cast<void (*)(std::int64_t, CURL *)>(addr)(multi_handle, easy_handle);
    }

    std::int64_t curl_multi_perform(std::int64_t multi_handle, int *running) {
        const auto addr =
            ResolveSymbolCached("curl_multi_perform", g_curl_multi_perform_addr, g_curl_multi_perform_missing_logged);
        if (addr == 0) {
            return 0;
        }
        return reinterpret_cast<std::int64_t (*)(std::int64_t, int *)>(addr)(multi_handle, running);
    }
}  // namespace d3::tagnx::curl
