#pragma once

#include "symbols/common.hpp"
#include "nn/util/util_snprintf.hpp"
#include "program/build_info.hpp"
#include "program/build_git.hpp"
#include "program/logging.hpp"
#include <cstdio>

#define D3HACK_VER     "D3Hack v3.0"
#define D3HACK_AUTHOR  "jester"
#define D3HACK_WEB     "https://jester.dev"
#define CRLF           "\n"
#define D3HACK_DESC    "Realtime hack for Diablo III: Eternal Collection" CRLF D3HACK_WEB
#define D3HACK_BUILD   " (" D3HACK_GIT_DESCRIBE " @ " __DATE__ " " __TIME__ ")"
#define D3HACK_FULLVER D3HACK_VER " by " D3HACK_AUTHOR " - Diablo III: Eternal Collection [v" D3CLIENT_VER "]"
#define D3HACK_FULLFPS D3HACK_VER D3HACK_BUILD
#define D3HACK_FULLWWW D3HACK_VER D3HACK_BUILD CRLF CRLF D3HACK_DESC

// #ifdef EXL_DEBUG
// Compile-time debug logging gates.
// Override with -DD3HACK_ENABLE_DBGLOG=0 / -DD3HACK_ENABLE_DBGPRINT=0 if needed.
#ifndef D3HACK_ENABLE_DBGLOG
#define D3HACK_ENABLE_DBGLOG 1
#endif

#ifndef D3HACK_ENABLE_DBGPRINT
#define D3HACK_ENABLE_DBGPRINT 1
#endif

#if D3HACK_ENABLE_DBGLOG
#define DBGLOG
#endif

#if D3HACK_ENABLE_DBGPRINT
#define DBGPRINT
#endif
// #endif

#ifndef DBGLOG
#define LOG(...) (static_cast<void>(sizeof(__VA_ARGS__)));
#else
#define LOG(fmt, ...)                                                \
    {                                                                \
        exl::log::LogFmt("[" EXL_MODULE_NAME "] " fmt, __VA_ARGS__); \
    }
#endif

#ifndef DBGPRINT
#define PRINT(...) (static_cast<void>(sizeof(__VA_ARGS__)));
#define PRINT_LINE PRINT
#define PRINT_EXPR PRINT
#else
#define PRINT(fmt, ...)                                      \
    {                                                        \
        exl::log::PrintFmt(EXL_LOG_PREFIX fmt, __VA_ARGS__); \
    }
#define PRINT_LINE(buf) (exl::log::PrintFmt(EXL_LOG_PREFIX "%s", buf))
#define PRINT_EXPR(fmt, ...)                                                   \
    {                                                                          \
        exl::log::PrintFmt("\n"                                                \
                           "[" EXL_MODULE_NAME "] @ %s\n"                      \
                           "[" EXL_MODULE_NAME "]    " #__VA_ARGS__ " = " fmt, \
                           __PRETTY_FUNCTION__, ##__VA_ARGS__);                \
    }  // __PRETTY_FUNCTION__ | __FUNCTION__

#endif

#define SNPRINT(buf, fmt, ...)                                    \
    {                                                             \
        const int nSize = snprintf(nullptr, 0, fmt, __VA_ARGS__); \
        snprintf(buf, (nSize + 1), fmt, __VA_ARGS__);             \
    }

extern const char g_cSzSeasonSwap[];
extern const char g_cSzConfigSwap[];
