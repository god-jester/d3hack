// clangd-only compatibility shim for missing target features.
#ifndef D3HACK_CLANGD_COMPAT_HPP
#define D3HACK_CLANGD_COMPAT_HPP

#if defined(__clang__)
namespace std {
#if !defined(__STDCPP_FLOAT128_T__)
using float128_t = long double;
#endif
#if !defined(__STDCPP_FLOAT16_T__)
using float16_t = unsigned short;
#endif
}

#ifndef __builtin_stack_address
#define __builtin_stack_address() __builtin_frame_address(0)
#endif
#endif

#endif
