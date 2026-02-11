#pragma once

#include "nn/nn_common.hpp"
#include "nn/os/os_user_exception_handler_types.hpp"

namespace nn::os {

    inline constexpr uintptr_t HandlerStackUsesThreadStackValue = 0;
    inline constexpr int       UserExceptionInfoUsesHandlerStackValue = 0;
    inline constexpr int       UserExceptionInfoUsesThreadStackValue  = 1;

    inline constexpr size_t HandlerStackAlignment = 16;

    inline const void* HandlerStackUsesThreadStack =
        reinterpret_cast<void*>(HandlerStackUsesThreadStackValue);
    inline UserExceptionInfo *const UserExceptionInfoUsesHandlerStack =
        reinterpret_cast<UserExceptionInfo *>(static_cast<intptr_t>(UserExceptionInfoUsesHandlerStackValue));
    inline UserExceptionInfo *const UserExceptionInfoUsesThreadStack =
        reinterpret_cast<UserExceptionInfo *>(static_cast<intptr_t>(UserExceptionInfoUsesThreadStackValue));

}  // namespace nn::os
