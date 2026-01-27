#pragma once

#include "nn/nn_common.hpp"
#include "nn/os/os_user_exception_handler_types.hpp"

namespace nn::os {

    void SetUserExceptionHandler(
        UserExceptionHandler pHandler,
        void *stack,
        size_t stackSize,
        UserExceptionInfo *pExceptionInfo
    );

    void EnableUserExceptionHandlerOnDebugging(bool isEnabled);

}  // namespace nn::os
