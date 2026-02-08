#include "lib/hook/trampoline.hpp"
#include "lib/diag/assert.hpp"
#include "lib/armv8.hpp"
#include "lib/util/stack_trace.hpp"
#include "lib/util/version.hpp"
#include "lib/util/sys/mem_layout.hpp"
#include "lib/util/sys/modules.hpp"
#include "program/build_info.hpp"
#include "program/boot_report.hpp"
#include "program/hook_registry.hpp"
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
#include "program/logging.hpp"
#include "program/fs_util.hpp"
#include "idadefs.h"
#include "program/exception_handler.hpp"

#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <string>

#define TO_FLOAT(v)         __coerce<float>(v)
#define TO_S32(v)           vcvtps_s32_f32(v)
#define TO_U32(v)           vcvtps_u32_f32(v)

#define FINDTRUNC(v, n)     (strstr(pBuf, v) && strlen(pBuf) >= (n))
#define FINDTRUNCP(v, p, n) (strstr(pBuf, v) && strstr(pBuf, v) - pBuf == (p) && strlen(pBuf) >= (n))

namespace d3 {
    namespace {
        constexpr uintptr_t kBuildVersionFullOffset     = 0x0E45863;  // rodata: "C" D3CLIENT_VER (BuildVersionFull)
        constexpr char      kBuildVersionFullExpected[] = "C" D3CLIENT_VER;

        // HookRegistry stores pointers to these setup functions. They are defined
        // as `inline` in headers, so we must ODR-use them in at least one TU to
        // ensure a definition is emitted for linkage.
        [[gnu::used]] static void (*const k_emit_setup_utility)()       = &SetupUtilityHooks;
        [[gnu::used]] static void (*const k_emit_setup_resolution)()    = &SetupResolutionHooks;
        [[gnu::used]] static void (*const k_emit_setup_debugging)()     = &SetupDebuggingHooks;
        [[gnu::used]] static void (*const k_emit_setup_season_events)() = &SetupSeasonEventHooks;
        [[gnu::used]] static void (*const k_emit_setup_lobby)()         = &SetupLobbyHooks;

        static auto VerifyBuildVersionFull() -> bool {
            const auto  base = exl::util::GetMainModuleInfo().m_Total.m_Start;
            const void *p    = reinterpret_cast<const void *>(base + kBuildVersionFullOffset);
            return std::memcmp(p, kBuildVersionFullExpected, sizeof(kBuildVersionFullExpected) - 1) == 0;
        }

        bool g_configHooksInstalled = false;

        enum class BootStage : u8 {
            ExlMainStart,
            VerifyBuild,
            ValidateOffsets,
            EarlyPatches,
            InstallBootHooks,
            MountSd,
            LoadConfig,
            ConfigureLogging,
            InstallHooks,
            ApplyPatches,
            InitGui,
            StartGameLoop,
        };

        static auto BootStageName(BootStage stage) -> const char * {
            switch (stage) {
            case BootStage::ExlMainStart:
                return "ExlMainStart";
            case BootStage::VerifyBuild:
                return "VerifyBuild";
            case BootStage::ValidateOffsets:
                return "ValidateOffsets";
            case BootStage::EarlyPatches:
                return "EarlyPatches";
            case BootStage::InstallBootHooks:
                return "InstallBootHooks";
            case BootStage::MountSd:
                return "MountSd";
            case BootStage::LoadConfig:
                return "LoadConfig";
            case BootStage::ConfigureLogging:
                return "ConfigureLogging";
            case BootStage::InstallHooks:
                return "InstallHooks";
            case BootStage::ApplyPatches:
                return "ApplyPatches";
            case BootStage::InitGui:
                return "InitGui";
            case BootStage::StartGameLoop:
                return "StartGameLoop";
            }
            return "Unknown";
        }

        static void PrintBootStage(BootStage stage) {
            PRINT("[boot] stage=%s", BootStageName(stage))
        }

    }  // namespace

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
            PRINT(
                "SigmaGlobals: em=%p emp=%p perf=%p refstr=%p",
                g_tSigmaGlobals.ptEMGlobals,
                g_tSigmaGlobals.ptEMPGlobals,
                g_tSigmaGlobals.ptGamePerfGlobals,
                g_tSigmaGlobals.ptRefStringGlobals
            )
            if (g_tSigmaGlobals.ptEMGlobals != nullptr) {
                const auto &em = *g_tSigmaGlobals.ptEMGlobals;
                PRINT(
                    "ErrorMgr: dump=%d mode=%d issue=%d sev=%d os=%d mem=%llu inspector=%u",
                    em.eDumpType,
                    em.eErrorManagerBlizzardErrorCrashMode,
                    em.tErrorManagerBlizzardErrorData.eIssueType,
                    em.tErrorManagerBlizzardErrorData.eSeverity,
                    em.tSystemInfo.eOSType,
                    static_cast<unsigned long long>(em.tSystemInfo.uTotalPhysicalMemory),
                    em.dwInspectorProjectID
                )
            }
            g_requestSeasonsLoad = true;
            exl::log::ConfigureGameFileLogging();
            d3::boot_report::PrintOnce();

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
            if (global_config.rare_cheats.active && global_config.rare_cheats.super_god_mode)
                EnableGod();
            // GfxWindowChangeDisplayModeHook::Callback(&g_ptGfxData->tCurrentMode);
        }
    };
    HOOK_DEFINE_TRAMPOLINE(MainInit){
        static void Callback() {
            // Require our SD to be mounted before running nnMain()
            PrintBootStage(BootStage::MountSd);
            R_ABORT_UNLESS(nn::fs::MountSdCardForDebug("sd"));

            PrintBootStage(BootStage::LoadConfig);
            LoadPatchConfig();

            PrintBootStage(BootStage::ConfigureLogging);
            exl::log::ConfigureGameFileLogging();
            if (global_config.debug.enable_exception_handler) {
                InstallExceptionHandler();
                PRINT_LINE("User exception handler enabled");
            }

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

            // Apply patches based on config
            if (!g_configHooksInstalled) {
                PrintBootStage(BootStage::InstallHooks);
                for (const auto &entry : hook_registry::Entries()) {
                    const bool enabled = (entry.enabled == nullptr) ? true : entry.enabled(global_config);
                    if (!enabled) {
                        continue;
                    }
                    EXL_ABORT_UNLESS(entry.install != nullptr, "Hook entry missing install: %s", entry.name);
                    entry.install();
                    d3::boot_report::RecordHook(entry.name);
                }
                g_configHooksInstalled = true;
            }

            PrintBootStage(BootStage::ApplyPatches);
            if (global_config.events.active) {
                d3::boot_report::RecordPatch("DynamicEvents");
                PatchDynamicEvents();
            }
            if (global_config.seasons.active) {
                d3::boot_report::RecordPatch("DynamicSeasonal");
                PatchDynamicSeasonal();
            }
            if (global_config.overlays.active && global_config.overlays.buildlocker_watermark) {
                d3::boot_report::RecordPatch("BuildLocker");
                PatchBuildlocker();
            }
            if (global_config.resolution_hack.active) {
                d3::boot_report::RecordPatch("ResolutionTargets");
                PatchResolutionTargets();
            }
            d3::boot_report::RecordPatch("Base");
            PatchBase();

            // GUI bringup
            PrintBootStage(BootStage::InitGui);
            d3::imgui_overlay::Initialize();

            // Allow game loop to start
            PrintBootStage(BootStage::StartGameLoop);
            Orig();
        }
    };

    extern "C" void exl_main(void * /*x0*/, void * /*x1*/) {
        PrintBootStage(BootStage::ExlMainStart);
        /* Setup hooking environment. */
        exl::hook::Initialize();

        PrintBootStage(BootStage::VerifyBuild);
        // Build guard: validate the expected build string exists at the known rodata offset.
        EXL_ABORT_UNLESS(VerifyBuildVersionFull(), "Unsupported build; expected %s", kBuildVersionFullExpected);
        PRINT_LINE("Compiled at " __DATE__ " " __TIME__);

        // Validate a small set of required offsets/symbols up-front. This prevents
        // late null derefs that look like "random crash" when running on an
        // unsupported build.
        PrintBootStage(BootStage::ValidateOffsets);
        {
            for (const char *key : offsets::kRequiredBootLookupKeys) {
                if (key == nullptr || key[0] == '\0') {
                    continue;
                }
                const uintptr_t addr = GameOffsetFromTable(key);
                EXL_ABORT_UNLESS(addr != 0, "Required lookup resolved to 0: %s", key);
            }
        }

        PrintBootStage(BootStage::EarlyPatches);
        PatchGraphicsPersistentHeapEarly();
        d3::boot_report::RecordPatch("GraphicsPersistentHeapEarly");

        PrintBootStage(BootStage::InstallBootHooks);
        MainInit::InstallAtFuncPtr(main_init);
        d3::boot_report::RecordHook("boot:MainInit");
        GfxInit::InstallAtFuncPtr(gfx_init);
        d3::boot_report::RecordHook("boot:GfxInit");
        ShellInitialize::InstallAtFuncPtr(shell_initialize);
        d3::boot_report::RecordHook("boot:ShellInitialize");
        GameCommonDataInit::InstallAtFuncPtr(game_common_data_init);
        d3::boot_report::RecordHook("boot:GameCommonDataInit");
        sInitializeWorld::InstallAtFuncPtr(sinitialize_world);
        d3::boot_report::RecordHook("boot:sInitializeWorld");
    }

}  // namespace d3

extern "C" NORETURN void exl_exception_entry() {
    constexpr char k_exl_exception_dump_path[] = "sd:/config/d3hack-nx/exl_exception.txt";

    std::string error;
    if (!d3::fs_util::EnsureConfigRootDirs(error)) {
        PRINT_LINE("[exl_exception_entry] EnsureConfigRootDirs failed");
    }

    FileReference tFileRef;
    FileReferenceInit(&tFileRef, k_exl_exception_dump_path);
    FileCreate(&tFileRef);
    if (FileOpen(&tFileRef, 2u) == 0) {
        EXL_ABORT("Default exception handler called (dump open failed)");
    }

    struct Writer {
        FileReference *ref;
        u32            offset;

        void Write(const char *fmt, ...) {
            char    buf[512] = {};
            va_list vl;
            va_start(vl, fmt);
            const int n = vsnprintf(buf, sizeof(buf), fmt, vl);
            va_end(vl);
            if (n <= 0) {
                return;
            }
            const u32 size = (n >= static_cast<int>(sizeof(buf))) ? static_cast<u32>(sizeof(buf) - 1) : static_cast<u32>(n);
            FileWrite(ref, buf, offset, size, ERROR_FILE_WRITE);
            offset += size;
        }

        void WriteAddressWithModule(const char *label, uintptr_t address) {
            const auto *module = exl::util::TryGetModule(address);
            if (module == nullptr) {
                Write("%s: 0x%016llx\n", label, static_cast<unsigned long long>(address));
                return;
            }
            const auto name   = module->GetModuleName();
            const auto offset = address - module->m_Total.m_Start;
            Write("%s: 0x%016llx (", label, static_cast<unsigned long long>(address));
            FileWrite(ref, name.data(), this->offset, static_cast<u32>(name.size()), ERROR_FILE_WRITE);
            this->offset += static_cast<u32>(name.size());
            Write("+0x%llx)\n", static_cast<unsigned long long>(offset));
        }
    };

    Writer writer {&tFileRef, 0u};
    writer.Write("\n--- exl_exception_entry ---\n");
    writer.Write("core=%d\n", nn::os::GetCurrentCoreNumber());

    const uintptr_t fp = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
    writer.Write("fp=0x%016llx\n", static_cast<unsigned long long>(fp));
    if (fp != 0) {
        writer.Write("Stack trace:\n");
        auto   it    = exl::util::stack_trace::Iterator(fp, exl::util::mem_layout::s_Stack);
        size_t depth = 0;
        while (it.Step() && depth < 64) {
            char label[24];
            snprintf(label, sizeof(label), "  #%zu", depth);
            writer.WriteAddressWithModule(label, it.GetReturnAddress());
            depth++;
        }
    }

    writer.Write("Modules:\n");
    for (int i = static_cast<int>(exl::util::ModuleIndex::Start); i < static_cast<int>(exl::util::ModuleIndex::End); ++i) {
        const auto index = static_cast<exl::util::ModuleIndex>(i);
        if (!exl::util::HasModule(index)) {
            continue;
        }
        const auto &module = exl::util::GetModuleInfo(index);
        const auto  name   = module.GetModuleName();
        writer.Write(
            "  %02d: 0x%016llx-0x%016llx ",
            i,
            static_cast<unsigned long long>(module.m_Total.m_Start),
            static_cast<unsigned long long>(module.m_Total.GetEnd())
        );
        FileWrite(&tFileRef, name.data(), writer.offset, static_cast<u32>(name.size()), ERROR_FILE_WRITE);
        writer.offset += static_cast<u32>(name.size());
        writer.Write("\n");
    }

    FileClose(&tFileRef);
    EXL_ABORT("Default exception handler called (dump written)");
}
