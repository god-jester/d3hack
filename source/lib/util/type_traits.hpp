#pragma once

namespace exl::util {
    template<typename T>
    struct FuncPtrTraits;

    /* Regular C function pointer. */
    template<typename R, typename... Args>
    struct FuncPtrTraits<R (*)(Args...)> {
        using Ptr = R (*)(Args...);
        using CPtr = R (*)(Args...);
        using ReturnType = R;

        static constexpr bool IsVariadic = false;
        static constexpr bool IsMemberFunc = false;
    };

    /* Regular C function pointer (noexcept). */
    template<typename R, typename... Args>
    struct FuncPtrTraits<R (*)(Args...) noexcept> {
        using Ptr = R (*)(Args...) noexcept;
        using CPtr = R (*)(Args...) noexcept;
        using ReturnType = R;

        static constexpr bool IsVariadic = false;
        static constexpr bool IsMemberFunc = false;
    };

    /* Regular C function pointer with variadic args. */
    template<typename R, typename... Args>
    struct FuncPtrTraits<R (*)(Args..., ...)> {
        using Ptr = R (*)(Args..., ...);
        using CPtr = R (*)(Args..., ...);
        using ReturnType = R;

        static constexpr bool IsVariadic = true;
        static constexpr bool IsMemberFunc = false;
    };

    /* Regular C function pointer with variadic args (noexcept). */
    template<typename R, typename... Args>
    struct FuncPtrTraits<R (*)(Args..., ...) noexcept> {
        using Ptr = R (*)(Args..., ...) noexcept;
        using CPtr = R (*)(Args..., ...) noexcept;
        using ReturnType = R;

        static constexpr bool IsVariadic = true;
        static constexpr bool IsMemberFunc = false;
    };

    /* C++ function pointer with non-const T pointer. */
    template<typename R, typename T, typename... Args>
    struct FuncPtrTraits<R (T::*)(Args...)> {
        using Ptr = R (T::*)(Args...);
        using CPtr = R (*)(T*, Args...);
        using ReturnType = R;
        using ThisType = T;

        static constexpr bool IsVariadic = false;
        static constexpr bool IsMemberFunc = true;
    };

    /* C++ function pointer with non-const T pointer (noexcept). */
    template<typename R, typename T, typename... Args>
    struct FuncPtrTraits<R (T::*)(Args...) noexcept> {
        using Ptr = R (T::*)(Args...) noexcept;
        using CPtr = R (*)(T*, Args...) noexcept;
        using ReturnType = R;
        using ThisType = T;

        static constexpr bool IsVariadic = false;
        static constexpr bool IsMemberFunc = true;
    };

    /* C++ function pointer with const T pointer. */
    template<typename R, typename T, typename... Args>
    struct FuncPtrTraits<R (T::*)(Args...) const> {
        using Ptr = R (T::*)(Args...) const;
        using CPtr = R (*)(const T*, Args...);
        using ReturnType = R;
        using ThisType = const T;

        static constexpr bool IsVariadic = false;
        static constexpr bool IsMemberFunc = true;
    };

    /* C++ function pointer with const T pointer (noexcept). */
    template<typename R, typename T, typename... Args>
    struct FuncPtrTraits<R (T::*)(Args...) const noexcept> {
        using Ptr = R (T::*)(Args...) const noexcept;
        using CPtr = R (*)(const T*, Args...) noexcept;
        using ReturnType = R;
        using ThisType = const T;

        static constexpr bool IsVariadic = false;
        static constexpr bool IsMemberFunc = true;
    };

    /* C++ function pointer with non-const T pointer and variadic args. */
    template<typename R, typename T, typename... Args>
    struct FuncPtrTraits<R (T::*)(Args..., ...)> {
        using Ptr = R (T::*)(Args..., ...);
        using CPtr = R (*)(T*, Args..., ...);
        using ReturnType = R;
        using ThisType = T;

        static constexpr bool IsVariadic = true;
        static constexpr bool IsMemberFunc = true;
    };

    /* C++ function pointer with non-const T pointer and variadic args (noexcept). */
    template<typename R, typename T, typename... Args>
    struct FuncPtrTraits<R (T::*)(Args..., ...) noexcept> {
        using Ptr = R (T::*)(Args..., ...) noexcept;
        using CPtr = R (*)(T*, Args..., ...) noexcept;
        using ReturnType = R;
        using ThisType = T;

        static constexpr bool IsVariadic = true;
        static constexpr bool IsMemberFunc = true;
    };

    /* C++ function pointer with const T pointer and variadic args. */
    template<typename R, typename T, typename... Args>
    struct FuncPtrTraits<R (T::*)(Args..., ...) const> {
        using Ptr = R (T::*)(Args..., ...) const;
        using CPtr = R (*)(const T*, Args..., ...);
        using ReturnType = R;
        using ThisType = const T;

        static constexpr bool IsVariadic = true;
        static constexpr bool IsMemberFunc = true;
    };

    /* C++ function pointer with const T pointer and variadic args (noexcept). */
    template<typename R, typename T, typename... Args>
    struct FuncPtrTraits<R (T::*)(Args..., ...) const noexcept> {
        using Ptr = R (T::*)(Args..., ...) const noexcept;
        using CPtr = R (*)(const T*, Args..., ...) noexcept;
        using ReturnType = R;
        using ThisType = const T;

        static constexpr bool IsVariadic = true;
        static constexpr bool IsMemberFunc = true;
    };

    /* TODO: more variations if needed (volatile/ref-qualifiers). */
}
