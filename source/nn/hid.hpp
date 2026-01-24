#pragma once

#include "nn/time.hpp"
#include "nn/util/bit_flag_set.hpp"
#include "types.h"

namespace nn::hid {

    enum class NpadButton : u64 {
        A = 0,
        B = 1,
        X = 2,
        Y = 3,
        StickL = 4,
        StickR = 5,
        L = 6,
        R = 7,
        ZL = 8,
        ZR = 9,
        Plus = 10,
        Minus = 11,
        Left = 12,
        Up = 13,
        Right = 14,
        Down = 15,
        StickLLeft = 16,
        StickLUp = 17,
        StickLRight = 18,
        StickLDown = 19,
        StickRLeft = 20,
        StickRUp = 21,
        StickRRight = 22,
        StickRDown = 23,
        LeftSL = 24,
        LeftSR = 25,
        RightSL = 26,
        RightSR = 27,
        Palma = 28,
        Verification = 29,
        HandheldLeftB = 30,
        LeftC = 31,
        UpC = 32,
        RightC = 33,
        DownC = 34,
    };

    enum class NpadAttribute : u32 {
        IsConnected = 0,
        IsWired = 1,
        IsLeftConnected = 2,
        IsLeftWired = 3,
        IsRightConnected = 4,
        IsRightWired = 5,
    };

    enum class NpadStyleTag : u32 {
        NpadStyleFullKey = 0,
        NpadStyleHandheld = 1,
        NpadStyleJoyDual = 2,
        NpadStyleJoyLeft = 3,
        NpadStyleJoyRight = 4,
        NpadStyleGc = 5,
        NpadStylePalma = 6,
        NpadStyleLark = 7,
        NpadStyleHandheldLark = 8,
        NpadStyleLucia = 9,
        NpadStyleLagon = 10,
        NpadStyleLager = 11,
        NpadStyleSystemExt = 29,
        NpadStyleSystem = 30,
    };

    using NpadButtonSet    = nn::util::BitFlagSet<64, NpadButton>;
    using NpadAttributeSet = nn::util::BitFlagSet<32, NpadAttribute>;
    using NpadStyleSet     = nn::util::BitFlagSet<32, NpadStyleTag>;

    struct AnalogStickState {
        s32 X;
        s32 Y;
    };

    struct NpadBaseState {
        u64 mSamplingNumber;
        NpadButtonSet mButtons;
        AnalogStickState mAnalogStickL;
        AnalogStickState mAnalogStickR;
        NpadAttributeSet mAttributes;
        char reserved[4];
    };

    struct NpadFullKeyState : NpadBaseState {
    };
    struct NpadHandheldState : NpadBaseState {
    };
    struct NpadJoyDualState : NpadBaseState {
    };
    struct NpadJoyLeftState : NpadBaseState {
    };
    struct NpadJoyRightState : NpadBaseState {
    };
    struct NpadPalmaState : NpadBaseState {
    };
    struct NpadSystemState : NpadBaseState {
    };
    struct NpadSystemExtState : NpadBaseState {
    };

    enum class MouseButton : u32 {
        Left = 0,
        Right = 1,
        Middle = 2,
        Forward = 3,
        Back = 4,
    };

    enum class MouseAttribute : u32 {
        Transferable = 0,
        IsConnected = 1,
    };

    enum class KeyboardModifier : u32 {
        Control = 0,
        Shift = 1,
        LeftAlt = 2,
        RightAlt = 3,
        Gui = 4,
        CapsLock = 5,
        ScrollLock = 6,
        NumLock = 7,
        Katakana = 8,
        Hiragana = 9,
    };

    enum class KeyboardKey : u32 {
        A = 4,
        B = 5,
        C = 6,
        D = 7,
        E = 8,
        F = 9,
        G = 10,
        H = 11,
        I = 12,
        J = 13,
        K = 14,
        L = 15,
        M = 16,
        N = 17,
        O = 18,
        P = 19,
        Q = 20,
        R = 21,
        S = 22,
        T = 23,
        U = 24,
        V = 25,
        W = 26,
        X = 27,
        Y = 28,
        Z = 29,

        D1 = 30,
        D2 = 31,
        D3 = 32,
        D4 = 33,
        D5 = 34,
        D6 = 35,
        D7 = 36,
        D8 = 37,
        D9 = 38,
        D0 = 39,

        Return = 40,
        Escape = 41,
        Backspace = 42,
        Tab = 43,
        Space = 44,

        LeftArrow = 80,
        RightArrow = 79,
        UpArrow = 82,
        DownArrow = 81,

        LeftControl = 224,
        LeftShift = 225,
        LeftAlt = 226,
        LeftGui = 227,
        RightControl = 228,
        RightShift = 229,
        RightAlt = 230,
        RightGui = 231,
    };

    void InitializeNpad();
    void SetSupportedNpadIdType(const u32*, u64);
    void SetSupportedNpadStyleSet(NpadStyleSet);
    NpadStyleSet GetNpadStyleSet(const u32& port);

    void GetNpadState(nn::hid::NpadFullKeyState*, uint const& port);
    void GetNpadState(nn::hid::NpadHandheldState*, uint const& port);
    void GetNpadState(nn::hid::NpadJoyDualState*, uint const& port);
    void GetNpadState(nn::hid::NpadJoyLeftState*, uint const& port);
    void GetNpadState(nn::hid::NpadJoyRightState*, uint const& port);

    void GetNpadStates(nn::hid::NpadFullKeyState*, int count, uint const& port);
    void GetNpadStates(nn::hid::NpadHandheldState*, int count, uint const& port);
    void GetNpadStates(nn::hid::NpadJoyDualState*, int count, uint const& port);
    void GetNpadStates(nn::hid::NpadJoyLeftState*, int count, uint const& port);
    void GetNpadStates(nn::hid::NpadJoyRightState*, int count, uint const& port);

    namespace detail {
        int GetNpadStates(int* out_count, nn::hid::NpadFullKeyState*, int count, uint const& port);
        int GetNpadStates(int* out_count, nn::hid::NpadHandheldState*, int count, uint const& port);
        int GetNpadStates(int* out_count, nn::hid::NpadJoyDualState*, int count, uint const& port);
        int GetNpadStates(int* out_count, nn::hid::NpadJoyLeftState*, int count, uint const& port);
        int GetNpadStates(int* out_count, nn::hid::NpadJoyRightState*, int count, uint const& port);
    }  // namespace detail

    using MouseButtonSet = nn::util::BitFlagSet<32, MouseButton>;
    using MouseAttributeSet = nn::util::BitFlagSet<32, MouseAttribute>;

    struct MouseState {
        u64 samplingNumber;
        s32 x;
        s32 y;
        s32 deltaX;
        s32 deltaY;
        s32 wheelDeltaX;
        s32 wheelDeltaY;
        MouseButtonSet buttons;
        MouseAttributeSet attributes;
    };

    using KeyboardModifierSet = nn::util::BitFlagSet<32, KeyboardModifier>;
    using KeyboardKeySet = nn::util::BitFlagSet<256, KeyboardKey>;

    struct KeyboardState {
        u64 samplingNumber;
        KeyboardModifierSet modifiers;
        KeyboardKeySet keys;
    };

    constexpr int TouchStateCountMax = 16;
    constexpr int TouchScreenStateCountMax = 16;

    enum class TouchAttribute : u32 {
      Start = 0,
      End = 1,
    };

    using TouchAttributeSet = nn::util::BitFlagSet<32, TouchAttribute>;

    struct TouchState {
      TimeSpanType deltaTime;
      TouchAttributeSet attributes;
      s32 fingerId;
      s32 x;
      s32 y;
      s32 diameterX;
      s32 diameterY;
      s32 rotationAngle;
      u8 _reserved[4];
    };

    template <size_t N>
    struct TouchScreenState {
      static_assert(0 < N);
      static_assert(N <= TouchStateCountMax);
      u64 samplingNumber = 0;
      s32 count = 0;
      u8 _reserved[4];
      TouchState touches[N] = {};
    };

    void InitializeTouchScreen();

    template <size_t N>
    void GetTouchScreenState(nn::hid::TouchScreenState<N>*);

    template <size_t N>
    int GetTouchScreenStates(nn::hid::TouchScreenState<N>*, int count);

    void InitializeMouse();
    void InitializeKeyboard();

    void GetMouseState(nn::hid::MouseState*);
    void GetKeyboardState(nn::hid::KeyboardState*);

}  // namespace nn::hid
