#pragma once

#include "nn/nn_common.hpp"

namespace nn::os {

    using Bit32 = u32;
    using Bit64 = u64;
    using Bit128 = u128;

    enum UserExceptionType {
        UserExceptionType_None                       = 0x0000,
        UserExceptionType_InvalidInstructionAccess   = 0x0100,
        UserExceptionType_InvalidDataAccess          = 0x0101,
        UserExceptionType_UnalignedInstructionAccess = 0x0102,
        UserExceptionType_UnalignedDataAccess        = 0x0103,
        UserExceptionType_UndefinedInstruction       = 0x0104,
        UserExceptionType_ExceptionalInstruction     = 0x0105,
        UserExceptionType_MemorySystemError          = 0x0106,
        UserExceptionType_FloatingPointException     = 0x0200,
        UserExceptionType_InvalidSystemCall          = 0x0301,
    };

    // AArch64 layout from nn::os user exception handler types.
    struct UserExceptionInfoDetail {
        union {
            struct {
                Bit64 r[31];
            };
            struct {
                Bit64 _padding[29];
                Bit64 fp;
                Bit64 lr;
            };
        };
        Bit64  sp;
        Bit64  pc;
        Bit128 v[32];
        Bit32  pstate;
        Bit32  fpcr;
        Bit32  fpsr;
        Bit32  afsr0;
        Bit32  afsr1;
        Bit32  esr;
        Bit64  far;
    };

    struct UserExceptionInfo {
        Bit32                   exceptionType;
        UserExceptionInfoDetail detail;
    };

    using UserExceptionHandler = void (*)(UserExceptionInfo *info);

}  // namespace nn::os
