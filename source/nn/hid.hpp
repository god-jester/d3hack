#pragma once

#include "types.h"
#include "nn/util/bit_flag_set.hpp"

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

}  // namespace nn::hid
