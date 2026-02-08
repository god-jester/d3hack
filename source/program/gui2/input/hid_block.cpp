#include "program/gui2/input/hid_block.hpp"

#include <array>
#include <atomic>
#include <cstdint>

#include "lib/diag/assert.hpp"
#include "lib/hook/trampoline.hpp"
#include "lib/reloc/reloc.hpp"
#include "lib/util/sys/mem_layout.hpp"
#include <nn/nn_common.hpp>
#include "nn/hid.hpp"  // IWYU pragma: keep
#include "nn/os.hpp"   // IWYU pragma: keep
#include "program/d3/setting.hpp"
#include "symbols/common.hpp"

namespace d3::gui2::input::hid_block {
    namespace {
        std::atomic<bool>      g_block_gamepad_input_to_game {false};
        std::atomic<bool>      g_allow_left_stick_passthrough {false};
        bool                   g_hid_hooks_installed = false;
        std::atomic<int>       g_hid_passthrough_depth {0};
        std::atomic<uintptr_t> g_hid_passthrough_thread {0};

        static auto GetCurrentThreadToken() -> uintptr_t {
            const auto *thread = nn::os::GetCurrentThread();
            return reinterpret_cast<uintptr_t>(thread);
        }

        enum class GamepadBlockMode {
            None,
            Full,
            LeftStickOnly,
        };

        static auto GetGamepadBlockMode() -> GamepadBlockMode {
            if (!g_block_gamepad_input_to_game.load(std::memory_order_relaxed)) {
                return GamepadBlockMode::None;
            }

            const bool allow_left_stick =
                g_allow_left_stick_passthrough.load(std::memory_order_relaxed);

            const int depth = g_hid_passthrough_depth.load(std::memory_order_relaxed);
            if (depth <= 0) {
                return allow_left_stick ? GamepadBlockMode::LeftStickOnly : GamepadBlockMode::Full;
            }

            const auto passthrough_thread = g_hid_passthrough_thread.load(std::memory_order_acquire);
            if (passthrough_thread != 0 && passthrough_thread == GetCurrentThreadToken()) {
                return GamepadBlockMode::None;
            }

            return allow_left_stick ? GamepadBlockMode::LeftStickOnly : GamepadBlockMode::Full;
        }

        template<typename TState>
        static void ClearNpadButtonsAndRightStick(TState *st) {
            if (st == nullptr) {
                return;
            }
            st->mButtons      = {};
            st->mAnalogStickR = {};
        }

        template<typename TState>
        static void ResetNpadState(TState *st) {
            if (st == nullptr) {
                return;
            }
            *st = {};
        }

        template<typename TState>
        static void ResetNpadStates(TState *out, int count) {
            if (out == nullptr || count <= 0) {
                return;
            }
            for (int i = 0; i < count; ++i) {
                ResetNpadState(out + i);
            }
        }

        using GetNpadStateFullKeyFn  = void (*)(nn::hid::NpadFullKeyState *, uint const &);
        using GetNpadStateHandheldFn = void (*)(nn::hid::NpadHandheldState *, uint const &);
        using GetNpadStateJoyDualFn  = void (*)(nn::hid::NpadJoyDualState *, uint const &);
        using GetNpadStateJoyLeftFn  = void (*)(nn::hid::NpadJoyLeftState *, uint const &);
        using GetNpadStateJoyRightFn = void (*)(nn::hid::NpadJoyRightState *, uint const &);

        using GetNpadStatesFullKeyFn  = void (*)(nn::hid::NpadFullKeyState *, int, uint const &);
        using GetNpadStatesHandheldFn = void (*)(nn::hid::NpadHandheldState *, int, uint const &);
        using GetNpadStatesJoyDualFn  = void (*)(nn::hid::NpadJoyDualState *, int, uint const &);
        using GetNpadStatesJoyLeftFn  = void (*)(nn::hid::NpadJoyLeftState *, int, uint const &);
        using GetNpadStatesJoyRightFn = void (*)(nn::hid::NpadJoyRightState *, int, uint const &);

        using GetNpadStatesDetailFullKeyFn  = int (*)(int *, nn::hid::NpadFullKeyState *, int, uint const &);
        using GetNpadStatesDetailHandheldFn = int (*)(int *, nn::hid::NpadHandheldState *, int, uint const &);
        using GetNpadStatesDetailJoyDualFn  = int (*)(int *, nn::hid::NpadJoyDualState *, int, uint const &);
        using GetNpadStatesDetailJoyLeftFn  = int (*)(int *, nn::hid::NpadJoyLeftState *, int, uint const &);
        using GetNpadStatesDetailJoyRightFn = int (*)(int *, nn::hid::NpadJoyRightState *, int, uint const &);

        using GetMouseStateFn    = void (*)(nn::hid::MouseState *);
        using GetKeyboardStateFn = void (*)(nn::hid::KeyboardState *);

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

        // clang-format off
#define DEFINE_HID_GET_STATE_HOOK(HOOK_NAME, STATE_TYPE)                           \
    HOOK_DEFINE_TRAMPOLINE(HOOK_NAME) {                                            \
        static auto Callback(nn::hid::STATE_TYPE *out, uint const &port) -> void { \
            const auto mode = GetGamepadBlockMode();                               \
            if (mode != GamepadBlockMode::None) {                                  \
                if (mode == GamepadBlockMode::LeftStickOnly) {                     \
                    Orig(out, port);                                               \
                    ClearNpadButtonsAndRightStick(out);                            \
                } else {                                                           \
                    ResetNpadState(out);                                           \
                }                                                                  \
                return;                                                            \
            }                                                                      \
            Orig(out, port);                                                       \
        }                                                                          \
    };

#define DEFINE_HID_GET_STATES_HOOK(HOOK_NAME, STATE_TYPE)                               \
    HOOK_DEFINE_TRAMPOLINE(HOOK_NAME) {                                                 \
        static auto Callback(nn::hid::STATE_TYPE *out, int count, uint const &port)     \
            -> void {                                                                   \
            const auto mode = GetGamepadBlockMode();                                    \
            if (mode != GamepadBlockMode::None) {                                       \
                if (mode == GamepadBlockMode::LeftStickOnly) {                          \
                    Orig(out, count, port);                                             \
                    if (out != nullptr && count > 0) {                                  \
                        for (int i = 0; i < count; ++i) {                               \
                            ClearNpadButtonsAndRightStick(out + i);                     \
                        }                                                               \
                    }                                                                   \
                } else {                                                                \
                    ResetNpadStates(out, count);                                        \
                }                                                                       \
                return;                                                                 \
            }                                                                           \
            Orig(out, count, port);                                                     \
        }                                                                               \
    };

#define DEFINE_HID_DETAIL_GET_STATES_HOOK(HOOK_NAME, STATE_TYPE)                        \
    HOOK_DEFINE_TRAMPOLINE(HOOK_NAME) {                                                 \
        static auto Callback(int *out_count, nn::hid::STATE_TYPE *out, int count,       \
                             uint const &port) -> int {                                 \
            const auto mode = GetGamepadBlockMode();                                    \
            if (mode != GamepadBlockMode::None) {                                       \
                if (mode == GamepadBlockMode::LeftStickOnly) {                          \
                    const int rc = Orig(out_count, out, count, port);                   \
                    if (out != nullptr && count > 0) {                                  \
                        for (int i = 0; i < count; ++i) {                               \
                            ClearNpadButtonsAndRightStick(out + i);                     \
                        }                                                               \
                    }                                                                   \
                    return rc;                                                          \
                }                                                                       \
                if (out_count != nullptr) {                                             \
                    *out_count = 0;                                                     \
                }                                                                       \
                ResetNpadStates(out, count);                                            \
                return 0;                                                               \
            }                                                                           \
            return Orig(out_count, out, count, port);                                   \
        }                                                                               \
    };
        // clang-format on

        DEFINE_HID_GET_STATE_HOOK(HidGetNpadStateFullKeyHook, NpadFullKeyState)
        DEFINE_HID_GET_STATE_HOOK(HidGetNpadStateHandheldHook, NpadHandheldState)
        DEFINE_HID_GET_STATE_HOOK(HidGetNpadStateJoyDualHook, NpadJoyDualState)
        DEFINE_HID_GET_STATE_HOOK(HidGetNpadStateJoyLeftHook, NpadJoyLeftState)
        DEFINE_HID_GET_STATE_HOOK(HidGetNpadStateJoyRightHook, NpadJoyRightState)

        DEFINE_HID_GET_STATES_HOOK(HidGetNpadStatesFullKeyHook, NpadFullKeyState)
        DEFINE_HID_GET_STATES_HOOK(HidGetNpadStatesHandheldHook, NpadHandheldState)
        DEFINE_HID_GET_STATES_HOOK(HidGetNpadStatesJoyDualHook, NpadJoyDualState)
        DEFINE_HID_GET_STATES_HOOK(HidGetNpadStatesJoyLeftHook, NpadJoyLeftState)
        DEFINE_HID_GET_STATES_HOOK(HidGetNpadStatesJoyRightHook, NpadJoyRightState)

        DEFINE_HID_DETAIL_GET_STATES_HOOK(HidDetailGetNpadStatesFullKeyHook, NpadFullKeyState)
        DEFINE_HID_DETAIL_GET_STATES_HOOK(HidDetailGetNpadStatesHandheldHook, NpadHandheldState)
        DEFINE_HID_DETAIL_GET_STATES_HOOK(HidDetailGetNpadStatesJoyDualHook, NpadJoyDualState)
        DEFINE_HID_DETAIL_GET_STATES_HOOK(HidDetailGetNpadStatesJoyLeftHook, NpadJoyLeftState)
        DEFINE_HID_DETAIL_GET_STATES_HOOK(HidDetailGetNpadStatesJoyRightHook, NpadJoyRightState)

        HOOK_DEFINE_TRAMPOLINE(HidGetMouseStateHook) {
            static auto Callback(nn::hid::MouseState *out) -> void {
                if (GetGamepadBlockMode() != GamepadBlockMode::None) {
                    if (out != nullptr) {
                        *out = {};
                    }
                    return;
                }
                Orig(out);
            }
        };

        HOOK_DEFINE_TRAMPOLINE(HidGetKeyboardStateHook) {
            static auto Callback(nn::hid::KeyboardState *out) -> void {
                if (GetGamepadBlockMode() != GamepadBlockMode::None) {
                    if (out != nullptr) {
                        *out = {};
                    }
                    return;
                }
                Orig(out);
            }
        };

#undef DEFINE_HID_DETAIL_GET_STATES_HOOK
#undef DEFINE_HID_GET_STATES_HOOK
#undef DEFINE_HID_GET_STATE_HOOK

    }  // namespace

    void SetBlockGamepadInputToGame(bool block) {
        g_block_gamepad_input_to_game.store(block, std::memory_order_relaxed);
    }

    void SetAllowLeftStickPassthrough(bool allow) {
        g_allow_left_stick_passthrough.store(allow, std::memory_order_relaxed);
    }

    ScopedHidPassthroughForOverlay::ScopedHidPassthroughForOverlay() {
        const uintptr_t token = GetCurrentThreadToken();
        EXL_ABORT_UNLESS(token != 0, "[hid_block] Failed to get current thread token");

        const int prev = g_hid_passthrough_depth.fetch_add(1, std::memory_order_acq_rel);
        EXL_ABORT_UNLESS(prev >= 0, "[hid_block] HID passthrough depth underflow on enter (prev=%d)", prev);

        if (prev == 0) {
            g_hid_passthrough_thread.store(token, std::memory_order_release);
        } else {
            const auto owner = g_hid_passthrough_thread.load(std::memory_order_acquire);
            EXL_ABORT_UNLESS(
                owner == token,
                "[hid_block] HID passthrough entered on different thread (owner=0x%lx current=0x%lx depth=%d)",
                owner,
                token,
                prev + 1
            );
        }
    }

    ScopedHidPassthroughForOverlay::~ScopedHidPassthroughForOverlay() {
        const uintptr_t token = GetCurrentThreadToken();
        EXL_ABORT_UNLESS(token != 0, "[hid_block] Failed to get current thread token");

        const auto owner = g_hid_passthrough_thread.load(std::memory_order_acquire);
        EXL_ABORT_UNLESS(
            owner == 0 || owner == token,
            "[hid_block] HID passthrough exited on different thread (owner=0x%lx current=0x%lx)",
            owner,
            token
        );

        const int prev = g_hid_passthrough_depth.fetch_sub(1, std::memory_order_acq_rel);
        EXL_ABORT_UNLESS(prev > 0, "[hid_block] HID passthrough depth underflow on exit (prev=%d)", prev);

        if (prev <= 1) {
            g_hid_passthrough_depth.store(0, std::memory_order_relaxed);
            g_hid_passthrough_thread.store(0, std::memory_order_release);
        }
    }

    void InstallHidHooks() {
        if (g_hid_hooks_installed) {
            return;
        }

        HidGetNpadStateFullKeyHook::InstallAtFuncPtr(static_cast<GetNpadStateFullKeyFn>(&nn::hid::GetNpadState));
        HidGetNpadStateHandheldHook::InstallAtFuncPtr(static_cast<GetNpadStateHandheldFn>(&nn::hid::GetNpadState));
        HidGetNpadStateJoyDualHook::InstallAtFuncPtr(static_cast<GetNpadStateJoyDualFn>(&nn::hid::GetNpadState));
        HidGetNpadStateJoyLeftHook::InstallAtFuncPtr(static_cast<GetNpadStateJoyLeftFn>(&nn::hid::GetNpadState));
        HidGetNpadStateJoyRightHook::InstallAtFuncPtr(static_cast<GetNpadStateJoyRightFn>(&nn::hid::GetNpadState));

        HidGetNpadStatesFullKeyHook::InstallAtFuncPtr(static_cast<GetNpadStatesFullKeyFn>(&nn::hid::GetNpadStates));
        HidGetNpadStatesHandheldHook::InstallAtFuncPtr(static_cast<GetNpadStatesHandheldFn>(&nn::hid::GetNpadStates));
        HidGetNpadStatesJoyDualHook::InstallAtFuncPtr(static_cast<GetNpadStatesJoyDualFn>(&nn::hid::GetNpadStates));
        HidGetNpadStatesJoyLeftHook::InstallAtFuncPtr(static_cast<GetNpadStatesJoyLeftFn>(&nn::hid::GetNpadStates));
        HidGetNpadStatesJoyRightHook::InstallAtFuncPtr(static_cast<GetNpadStatesJoyRightFn>(&nn::hid::GetNpadStates));

        HidGetMouseStateHook::InstallAtFuncPtr(static_cast<GetMouseStateFn>(&nn::hid::GetMouseState));
        HidGetKeyboardStateHook::InstallAtFuncPtr(static_cast<GetKeyboardStateFn>(&nn::hid::GetKeyboardState));

        struct DetailHookEntry {
            const char *name;
            void (*install)(uintptr_t);
        };

        const std::array<DetailHookEntry, 5> detail_hooks = {{
            {.name    = "_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_"
                        "16NpadFullKeyStateEiRKj",
             .install = [](uintptr_t addr) -> void {
                 HidDetailGetNpadStatesFullKeyHook::InstallAtPtr(addr);
             }},
            {.name    = "_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_"
                        "17NpadHandheldStateEiRKj",
             .install = [](uintptr_t addr) -> void {
                 HidDetailGetNpadStatesHandheldHook::InstallAtPtr(addr);
             }},
            {.name    = "_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_"
                        "16NpadJoyDualStateEiRKj",
             .install = [](uintptr_t addr) -> void {
                 HidDetailGetNpadStatesJoyDualHook::InstallAtPtr(addr);
             }},
            {.name    = "_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_"
                        "16NpadJoyLeftStateEiRKj",
             .install = [](uintptr_t addr) -> void {
                 HidDetailGetNpadStatesJoyLeftHook::InstallAtPtr(addr);
             }},
            {.name    = "_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_"
                        "17NpadJoyRightStateEiRKj",
             .install = [](uintptr_t addr) -> void {
                 HidDetailGetNpadStatesJoyRightHook::InstallAtPtr(addr);
             }},
        }};

        for (const auto &hook : detail_hooks) {
            void const *addr = LookupSymbolAddress(hook.name);
            if (addr == nullptr) {
                PRINT("[hid_block] nn::hid detail symbol missing: %s", hook.name);
                continue;
            }
            hook.install(reinterpret_cast<uintptr_t>(addr));
        }

        g_hid_hooks_installed = true;
        PRINT_LINE("[hid_block] nn::hid input hooks installed");
    }

}  // namespace d3::gui2::input::hid_block
