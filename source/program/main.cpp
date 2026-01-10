#include "lib/hook/inline.hpp"
#include "lib/hook/trampoline.hpp"
#include "lib/diag/assert.hpp"
#include "lib/armv8/instructions.hpp"
#include "lib/util/sys/modules.hpp"
#include "lib/util/version.hpp"
#include "program/build_info.hpp"
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
        bool VerifySignature() {
            if (exl::util::GetUserVersion() == exl::util::UserVersion::DEFAULT)
                return true;
            PRINT("Signature guard failed; build string \"%s\" not found", D3CLIENT_VER);
            return false;
        }

        bool g_config_hooks_installed = false;
    }  // namespace

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
            g_ptGfxData = *reinterpret_cast<GfxInternalData **const>(GameOffsetFromTable("gfx_internal_data_ptr"));
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
            g_ptMainRWindow = reinterpret_cast<RWindow *const>(GameOffsetFromTable("main_rwindow"));
            PRINT_LINE("ShellInitialized");
            g_request_seasons_load = true;

            // EXL_ABORT(420);
            return ret;
        }
    };
    HOOK_DEFINE_TRAMPOLINE(SGameInitialize){
        static auto Callback(GameParams *tParams, const OnlineService::PlayerResolveData *ptResolvedPlayer) -> SGameID {
            auto ret        = Orig(tParams, ptResolvedPlayer);
            auto sGameCurID = AppServerGetOnlyGame();
            PRINT_EXPR("%x, %i", sGameCurID, ServerIsLocal());
            return ret;
        }
    };
    HOOK_DEFINE_TRAMPOLINE(sInitializeWorld){
        static void Callback(SNO snoWorld, uintptr_t *ptSWorld) {
            Orig(snoWorld, ptSWorld);
            auto tSGameGlobals   = SGameGlobalsGet();
            auto sGameCurID      = AppServerGetOnlyGame();
            gs_idGameConnection  = ServerGetOnlyGameConnection();
            PRINT("NEW sInitializeWorld! (SGame: %x | Connection: %x | Primary for connection: %p) %s %s", sGameCurID, gs_idGameConnection, GetPrimaryPlayerForGameConnection(gs_idGameConnection), tSGameGlobals->uszCreatorAccountName, tSGameGlobals->uszCreatorHeroName);
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

        MainInit::InstallAtFuncPtr(main_init);
        GfxInit::InstallAtFuncPtr(gfx_init);
        ShellInitialize::InstallAtFuncPtr(shell_initialize);
        GameCommonDataInit::InstallAtFuncPtr(game_common_data_init);
        SGameInitialize::InstallAtFuncPtr(sgame_initialize);
        sInitializeWorld::InstallAtFuncPtr(sinitialize_world);
        // PostFXInitClampDims::InstallAtOffset(0x12F218);
        // GfxSetRenderAndDepthTargetSwapchainFix::InstallAtOffset(0x29C670);
        // GfxSetDepthTargetSwapchainFix::InstallAtOffset(0x29C4E0);
        // LobbyServiceIdleInternal::InstallAtFuncPtr(lobby_service_idle_internal);
        // StubCopyright::InstallAtFuncPtr(nn::oe::SetCopyrightVisibility);
    }

}  // namespace d3

extern "C" NORETURN void exl_exception_entry() {
    /* TODO: exception handling */
    EXL_ABORT("Default exception handler called!");
}
