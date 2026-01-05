#include "lib/hook/inline.hpp"
#include "lib/hook/trampoline.hpp"
#include "lib/diag/assert.hpp"
#include "lib/armv8/instructions.hpp"
#include "lib/util/sys/modules.hpp"
#include "nn/fs/fs_mount.hpp"
#include "d3/_util.hpp"
#include "d3/_lobby.hpp"
#include "d3/patches.hpp"
#include "config.hpp"
#include "d3/hooks/util.hpp"
#include "d3/hooks/resolution.hpp"
#include "d3/hooks/debug.hpp"
#include "d3/hooks/season_events.hpp"
#include "d3/hooks/lobby.hpp"
#include "idadefs.h"

#define to_float(v)         __coerce<float>(v)
#define to_s32(v)           vcvtps_s32_f32(v)
#define to_u32(v)           vcvtps_u32_f32(v)

#define findtrunc(v, n)     (strstr(pBuf, v) && strlen(pBuf) >= n)
#define findtruncp(v, p, n) (strstr(pBuf, v) && strstr(pBuf, v) - pBuf == p && strlen(pBuf) >= n)

// namespace nn::oe {
//     void SetCopyrightVisibility(bool);
// };

namespace d3 {
    namespace {
        const char *FindStringInRange(const exl::util::Range &range, std::string_view needle) {
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

        bool CheckBuildString() {
            const auto &mod = exl::util::GetMainModuleInfo();
            return FindStringInRange(mod.m_Rodata, D3CLIENT_VER) != nullptr;
        }

        bool VerifySignature() {
            if (CheckBuildString())
                return true;
            PRINT("Signature guard failed; build string \"%s\" not found", D3CLIENT_VER);
            return false;
        }

        bool g_config_hooks_installed = false;
    }

    // static auto diablo_rounding(u32 nDamage) {
    //     float flTruncated = to_float(nDamage) / 1.0e12f;
    //     float flRemainder = 8388600.0;
    //     u32   nRounded;
    //     if (__builtin_fabsf(flTruncated) <= flRemainder) {
    //         if (flTruncated <= 0.0) {
    //             if (flTruncated < 0.0)
    //                 nRounded = -(to_u32(flTruncated + -flRemainder) & 0x7FFFFF);
    //             else
    //                 nRounded = 0;
    //             PRINT_EXPR("flTruncated <= 0.0: %x %i", nRounded, nRounded)
    //         } else {
    //             nRounded = COERCE_UNSIGNED_INT(flTruncated + flRemainder) & 0x7FFFFF;
    //             PRINT_EXPR("2 COERCE &x7FF: %x %i", nRounded, nRounded)
    //         }
    //     } else if (flTruncated >= 0.0) {
    //         nRounded = (__PAIR64__((flTruncated + 0.5), (flTruncated + 0.5)) - to_u32(0.0)) >> 32;
    //         PRINT_EXPR("3 >>32: %x %i (%.3f)", nRounded, nRounded, flTruncated)
    //     } else {
    //         nRounded = to_s32(flTruncated + -0.5);  // vcvtps_s32_f32
    //         PRINT_EXPR("4: %x %i (%i ? %.3f)", nRounded, nRounded, to_s32(flTruncated - 0.5f), flTruncated)
    //     }
    //     return nRounded;
    // }

    HOOK_DEFINE_TRAMPOLINE(GfxInit){
        static auto Callback() -> BOOL {
            auto ret    = Orig();
            g_ptGfxData = *reinterpret_cast<GfxInternalData **const>(GameOffset(0x1830F68));
            return ret;
        }
    };
    HOOK_DEFINE_TRAMPOLINE(GameCommonDataInit) {
        static void Callback(GameCommonData *_this, const GameParams *tParams) {
            Orig(_this, tParams);
            g_ptGCData = _this;
            // PRINT_EXPR("%x", self->nNumPlayers)
            // PRINT_EXPR("%x", self->eDifficultyLevel)
            // PRINT_EXPR("%x", tParams->idGameConnection)
            // PRINT_EXPR("%x", tParams->idSGame)
            // PRINT_EXPR("%x", tParams->nGameParts)
            // PRINT_EXPR("%i", ServerIsLocal())
        }
    };
    HOOK_DEFINE_TRAMPOLINE(ShellInitialize){
        static auto Callback(uint32 hInstance, uint32 hWndParent) -> BOOL {
            auto ret        = Orig(hInstance, hWndParent);
            g_ptMainRWindow = reinterpret_cast<RWindow *const>(GameOffset(0x17DE610));

            // char *pBuf = static_cast<char *>(alloca(0xFF));
            // for (u32 nDamage = 0x5dde0b6b; nDamage < 0x62e8d4a6; nDamage += 0x1) {
            //     nDamage           = 0x5e065fd7;
            //     auto  nD3Rounded  = diablo_rounding(nDamage);
            //     auto  flDamage    = COERCE_FLOAT(nDamage);
            //     float flTruncated = to_float(nDamage) / 1.0e12f;
            //     snprintf(pBuf, 0xFF, "%.0fT", (float)nD3Rounded);
            //     // PRINT("%x | %x | %.0F, %0.3F", nDamage, nD3Rounded, flDamage, COERCE_FLOAT(n_flDamage))
            //     if (
            //         nDamage == 0x5e065fd7 ||
            //         findtrunc("98765432", 0) || findtrunc("654321T", 0) ||
            //         findtrunc("420666T", 0) || findtrunc("666420T", 0) || findtrunc("6666666", 0) || findtrunc("00000000T", 0) ||
            //         findtruncp("4206660", 0, 10) || findtruncp("1234567", 0, 10) ||
            //         findtruncp("1337666", 0, 11) || findtruncp("133700", 0, 11)
            //     ) {
            //         // 0x60d629d3 = 123456784T || 123,456,784T
            //         // 0x6285da24 = 1234567808T || 1,234,567,808T
            //         // 0x6290f535 = 1337000064T || 1,337,000,064T
            //         // 0x629100e2 = 1337420800T || 1,337,420,800T
            //         // 0x62910879 = 1337694208T || 1,337,694,208T
            //         // 0x62910886 = 1337696000T || 1,337,696,000T
            //         // svcOutputDebugString(pBuf, nLength);
            //         // PRINT("%x | %s", nDamage, FormatTruncatedNumber(flDamage, 1, 1).m_pszString)
            //         PRINT("0x%x = %.0fT (%.0fT) || %s\n", nDamage, (float)nD3Rounded, flTruncated, FormatTruncatedNumber(flDamage, 1, 1).m_pszString)
            //         break;
            //     }
            // }

            PRINT("%s", "FINISHED!")

            PRINT_EXPR("local logging: %d", *((BOOL *)(GameOffset(0x1A482C8) + 0xB0)))
            // auto sLocalLogging = *reinterpret_cast<uintptr_t *>(GameOffset(0x115A408));
            // auto sLocalLogging = FollowPtr<uintptr_t, 0>(GameOffset(0x115A408));
            // XVarBool_Set(sLocalLogging, true, 3);
            // XVarBool_Set(FollowPtr<uintptr_t, 0>(GameOffset(0x115A408)), true, 3);
            // XVarBool_Set(&s_varFreeToPlay, true, 3u);
            // XVarBool_Set(&s_varSeasonsOverrideEnabled, true, 3u);
            // XVarBool_Set(&s_varChallengeEnabled, true, 3u);
            // XVarBool_Set(&s_varExperimentalScheduling, true, 3u);
            // PRINT_EXPR("bool: %s", XVarBool_ToString(&s_varLocalLoggingEnable).m_elements)
            // PRINT_EXPR("bool: %s", XVarBool_ToString(&s_varOnlineServicePTR).m_elements)
            // PRINT_EXPR("bool: %s", XVarBool_ToString(&s_varFreeToPlay).m_elements)
            // PRINT_EXPR("bool: %s", XVarBool_ToString(&s_varSeasonsOverrideEnabled).m_elements)
            // PRINT_EXPR("bool: %s", XVarBool_ToString(&s_varChallengeEnabled).m_elements)
            // PRINT_EXPR("bool: %s", XVarBool_ToString(&s_varExperimentalScheduling).m_elements)
            // blz::shared_ptr<blz::basic_string<char,blz::char_traits<char>,blz::allocator<char> > > *pszFileData
            // auto pszFileData = std::make_shared<std::string>(c_szSeasonSwap);
            // OnSeasonsFileRetrieved(nullptr, 0, &pszFileData);

            // EXL_ABORT(420);
            return ret;
        }
    };
    HOOK_DEFINE_TRAMPOLINE(SGameInitialize){
        static auto Callback(GameParams *tParams, const OnlineService::PlayerResolveData *ptResolvedPlayer) -> SGameID {
            auto ret        = Orig(tParams, ptResolvedPlayer);
            auto sGameCurID = AppServerGetOnlyGame();
            PRINT_EXPR("NEW SGameInitialize! %x : %i", sGameCurID, ServerIsLocal())
            return ret;
        }
    };
    HOOK_DEFINE_TRAMPOLINE(sInitializeWorld){
        static void Callback(SNO snoWorld, uintptr_t *ptSWorld) {
            Orig(snoWorld, ptSWorld);
            auto tSGameGlobals   = SGameGlobalsGet();
            auto sGameConnection = ServerGetOnlyGameConnection();
            auto sGameCurID      = AppServerGetOnlyGame();
            PRINT("NEW sInitializeWorld! (SGame: %x | Connection: %x | Primary for connection: %p) %s %s", sGameCurID, sGameConnection, GetPrimaryPlayerForGameConnection(sGameConnection), tSGameGlobals->uszCreatorAccountName, tSGameGlobals->uszCreatorHeroName)
        }
    };
    HOOK_DEFINE_TRAMPOLINE(MainInit){
        static void Callback() {
            if (!VerifySignature()) {
                PRINT("Signature guard failed; expected build %s", D3CLIENT_VER);
                EXL_ABORT("Signature guard failed: offsets mismatch.");
            }

            // Require our SD to be mounted before running nnMain()
            R_ABORT_UNLESS(nn::fs::MountSdCardForDebug("sd"));
            LoadPatchConfig();

            // Apply patches based on config
            if (!g_config_hooks_installed) {
                SetupUtilityHooks();
                SetupResolutionHooks();
                SetupDebuggingHooks();
                SetupSeasonEventHooks();
                PatchVarResLabel();
                PatchReleaseFPSLabel();
                g_config_hooks_installed = true;
            }
            if (global_config.debug.enable_crashes)
                SetupLobbyHooks();
            if (global_config.events.active)
                PatchDynamicEvents();
            if (global_config.seasons.active)
                PatchDynamicSeasonal();
            if (global_config.overlays.active && global_config.overlays.buildlocker_watermark)
                PatchBuildlocker();
            if (global_config.overlays.active && global_config.overlays.ddm_labels)
                PatchDDMLabels();
            if (global_config.resolution_hack.active)
                PatchResolutionTargets();
            PatchBase();

            // Allow game loop to start
            Orig();
        }
    };
    HOOK_DEFINE_TRAMPOLINE(StubCopyright) {
        static void Callback(bool enabled) {
            Orig(false);
        }
    };

    extern "C" void exl_main(void * /*x0*/, void * /*x1*/) {
        /* Setup hooking environment. */
        exl::hook::Initialize();

        MainInit::InstallAtOffset(0x480);
        GfxInit::InstallAtOffset(0x298BD0);
        ShellInitialize::InstallAtOffset(0x667830);
        GameCommonDataInit::InstallAtOffset(0x4CABA0);
        // SGameInitialize::InstallAtOffset(0x7B08A0);
        // sInitializeWorld::InstallAtOffset(0x8127A0);
        // StubCopyright::InstallAtFuncPtr(nn::oe::SetCopyrightVisibility);
    }

}  // namespace d3

extern "C" NORETURN void exl_exception_entry() {
    /* TODO: exception handling */
    EXL_ABORT("Default exception handler called!");
}
