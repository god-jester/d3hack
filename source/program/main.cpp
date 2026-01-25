#include "lib/hook/trampoline.hpp"
#include "lib/diag/assert.hpp"
#include "lib/util/version.hpp"
#include "program/build_info.hpp"
#include "nn/fs/fs_mount.hpp"
#include "d3/_util.hpp"
#include "d3/patches.hpp"
#include "config.hpp"
#include "d3/hooks/util.hpp"
#include "d3/hooks/resolution.hpp"
#include "d3/hooks/debug.hpp"
#include "d3/hooks/season_events.hpp"
#include "d3/hooks/lobby.hpp"
#include "d3/types/gfx.hpp"
#include "program/gui2/imgui_overlay.hpp"
#include "program/runtime_apply.hpp"
#include "idadefs.h"

#include <cstring>

#define TO_FLOAT(v)         __coerce<float>(v)
#define TO_S32(v)           vcvtps_s32_f32(v)
#define TO_U32(v)           vcvtps_u32_f32(v)

#define FINDTRUNC(v, n)     (strstr(pBuf, v) && strlen(pBuf) >= (n))
#define FINDTRUNCP(v, p, n) (strstr(pBuf, v) && strstr(pBuf, v) - pBuf == (p) && strlen(pBuf) >= (n))

namespace d3 {
    namespace {
        auto VerifySignature() -> bool {
            return true;
            if (exl::util::GetUserVersion() == exl::util::UserVersion::DEFAULT)
                return true;
            PRINT("Signature guard failed; build string \"%s\" not found", D3CLIENT_VER);
            return false;
        }

        bool g_configHooksInstalled = false;
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
            PRINT_EXPR("%d", g_ptGfxData->dwModeCount);
            if (g_ptGfxNVNGlobals != nullptr) {
                PRINT(
                    "GFXNVN globals: dev=%ux%u reduced=%ux%u",
                    g_ptGfxNVNGlobals->dwDeviceWidth,
                    g_ptGfxNVNGlobals->dwDeviceHeight,
                    g_ptGfxNVNGlobals->dwReducedDeviceWidth,
                    g_ptGfxNVNGlobals->dwReducedDeviceHeight
                )
            }
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
            g_requestSeasonsLoad = true;

            // EXL_ABORT("Hello!");

            return ret;
        }
    };
    HOOK_DEFINE_TRAMPOLINE(SGameInitialize){
        static auto Callback(GameParams *tParams, const OnlineService::PlayerResolveData *ptResolvedPlayer) -> SGameID {
            auto ret        = Orig(tParams, ptResolvedPlayer);
            auto sGameCurID = AppServerGetOnlyGame();
            PRINT_EXPR("%x, %i", sGameCurID, ServerIsLocal());
            // d3::imgui_overlay::PostOverlayNotification(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 4.0f, "SGame initialized");
            return ret;
        }
    };
    HOOK_DEFINE_TRAMPOLINE(sInitializeWorld){
        static void Callback(SNO snoWorld, uintptr_t *ptSWorld) {
            Orig(snoWorld, ptSWorld);
            auto *ptSGameGlobals = SGameGlobalsGet();
            auto  sGameCurID     = AppServerGetOnlyGame();
            g_idGameConnection   = ServerGetOnlyGameConnection();
            PRINT("NEW sInitializeWorld! (SGame: %x | Connection: %x | Primary for connection: %p) %s %s", sGameCurID, g_idGameConnection, GetPrimaryPlayerForGameConnection(g_idGameConnection), ptSGameGlobals->uszCreatorAccountName, ptSGameGlobals->uszCreatorHeroName);
            // GfxWindowChangeDisplayModeHook::Callback(&g_ptGfxData->tCurrentMode);
        }
    };
    HOOK_DEFINE_TRAMPOLINE(MainInit){
        static void Callback() {
            if (!VerifySignature()) {
                PRINT("Signature guard failed; expected build %s", D3CLIENT_VER);
                EXL_ABORT("Signature guard failed: version string not found!");
            }

            // Require our SD to be mounted before running nnMain()
            R_ABORT_UNLESS(nn::fs::MountSdCardForDebug("sd"));
            LoadPatchConfig();
            if (global_config.resolution_hack.active) {
                const u32 outW   = global_config.resolution_hack.OutputWidthPx();
                const u32 outH   = global_config.resolution_hack.OutputHeightPx();
                const u32 clampH = global_config.resolution_hack.ClampTextureHeightPx();
                const u32 clampW = global_config.resolution_hack.ClampTextureWidthPx();
                PRINT(
                    "ResolutionHack config: target=%u output=%ux%u min_scale=%.1f max_scale=%.1f clamp_h=%u clamp=%ux%u defaults_only=%u",
                    global_config.resolution_hack.target_resolution,
                    outW,
                    outH,
                    global_config.resolution_hack.min_res_scale,
                    global_config.resolution_hack.max_res_scale,
                    clampH,
                    clampW,
                    clampH,
                    static_cast<u32>(global_config.defaults_only)
                );
            } else {
                PRINT_LINE("ResolutionHack config: inactive");
            }

            PatchRenderTargetCurrentResolutionScale(
                global_config.resolution_hack.active ? global_config.resolution_hack.HandheldScaleFraction() : 0.0f
            );

            // Apply patches based on config
            if (!g_configHooksInstalled) {
                SetupUtilityHooks();
                SetupResolutionHooks();
                SetupDebuggingHooks();
                SetupSeasonEventHooks();
                PatchVarResLabel();
                PatchReleaseFPSLabel();
                g_configHooksInstalled = true;
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

            // GUI bringup
            d3::imgui_overlay::Initialize();

            // Allow game loop to start
            Orig();
        }
    };

    extern "C" void exl_main(void * /*x0*/, void * /*x1*/) {
        /* Setup hooking environment. */
        exl::hook::Initialize();
        PRINT_LINE("Compiled at " __DATE__ " " __TIME__);

        PatchGraphicsPersistentHeapEarly();

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
    }

    void ApplyPatchConfigRuntime(const PatchConfig &config, RuntimeApplyResult *out) {
        RuntimeApplyResult result {};

        const PatchConfig prev      = global_config;
        global_config               = config;
        global_config.initialized   = true;
        global_config.defaults_only = false;

        // Safe, per-frame-gated features should apply immediately just by updating global_config.
        // For enable-only static patches, re-run patch entrypoints when turning them on.

        auto append_note = [&](const char *text) -> void {
            if (text == nullptr || text[0] == '\0') {
                return;
            }
            if (result.note[0] == '\0') {
                snprintf(result.note, sizeof(result.note), "%s", text);
                return;
            }
            const size_t len = std::strlen(result.note);
            if (len + 2 >= sizeof(result.note)) {
                return;
            }
            std::strncat(result.note, ", ", sizeof(result.note) - len - 1);
            std::strncat(result.note, text, sizeof(result.note) - std::strlen(result.note) - 1);
        };

        // XVars that can be updated at runtime.
        XVarBool_Set(&g_varOnlineServicePTR, global_config.seasons.spoof_ptr, 3u);
        XVarBool_Set(&g_varExperimentalScheduling, global_config.resolution_hack.exp_scheduler, 3u);

        // Enable-only patches.
        if (global_config.overlays.active && global_config.overlays.buildlocker_watermark &&
            !(prev.overlays.active && prev.overlays.buildlocker_watermark)) {
            PatchBuildlocker();
            result.applied_enable_only = true;
            append_note("BuildLocker");
        } else if ((prev.overlays.active && prev.overlays.buildlocker_watermark) &&
                   !(global_config.overlays.active && global_config.overlays.buildlocker_watermark)) {
            result.restart_required = true;
        }

        if (prev.resolution_hack.active != global_config.resolution_hack.active ||
            prev.resolution_hack.output_handheld_scale != global_config.resolution_hack.output_handheld_scale) {
            result.restart_required = true;
            append_note("RT scale");
        }

        // Dynamic/runtime patches.
        if (global_config.events.active) {
            PatchDynamicEvents();
        } else if (prev.events.active) {
            result.restart_required = true;
        }

        if (global_config.seasons.active) {
            PatchDynamicSeasonal();
        } else if (prev.seasons.active) {
            result.restart_required = true;
        }

        if (prev.resolution_hack.active != global_config.resolution_hack.active) {
            result.restart_required = true;
        }

        const bool resolution_target_changed =
            prev.resolution_hack.target_resolution != global_config.resolution_hack.target_resolution ||
            prev.resolution_hack.OutputHandheldHeightPx() != global_config.resolution_hack.OutputHandheldHeightPx();
        if (resolution_target_changed) {
            append_note("Resolution targets");
            result.restart_required = true;
        }
        // PatchResolutionTargetDeviceDims();
        // RequestDockNotify();
        // RequestPerfNotify();
        // g_ptGfxData->dwFlags &= ~0x400u;
        // // GfxForceDeviceReset(true);
        // // GfxDeviceReset();
        // scheck_gfx_ready(true);

        // Hooks/patches gated at boot.
        if (!prev.debug.enable_crashes && global_config.debug.enable_crashes) {
            result.restart_required = true;
        }
        if (prev.seasons.allow_online != global_config.seasons.allow_online) {
            result.restart_required = true;
        }

        if (out != nullptr) {
            *out = result;
        }
    }

}  // namespace d3

extern "C" NORETURN void exl_exception_entry() {
    /* TODO: exception handling */
    EXL_ABORT("Default exception handler called!");
}
