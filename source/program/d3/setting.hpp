#pragma once

#include "symbols/common.hpp"
#include "nn/util/util_snprintf.hpp"
#include "program/build_info.hpp"
#include "program/logging.hpp"
#include <cstdio>

#define D3HACK_VER     "D3Hack v3.0"
#define D3HACK_AUTHOR  "jester"
#define D3HACK_WEB     "https://jester.dev"
#define CRLF           "\n"
#define D3HACK_DESC    "Realtime hack for Diablo III: Eternal Collection" CRLF D3HACK_WEB
#define D3HACK_BUILD   " (Built by " D3HACK_AUTHOR " @ " __DATE__ " " __TIME__ ")"
#define D3HACK_FULLVER D3HACK_VER " by " D3HACK_AUTHOR " - Diablo III: Eternal Collection [v" D3CLIENT_VER "]"
#define D3HACK_FULLFPS D3HACK_VER D3HACK_BUILD
#define D3HACK_FULLWWW D3HACK_VER D3HACK_BUILD CRLF CRLF D3HACK_DESC

#ifdef EXL_DEBUG
#define DBGLOG
#define DBGPRINT
#endif

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

constinit const char g_cSzSeasonSwap[] =
    "# Format for dates MUST be: \" ? ? ? , DD MMM YYYY hh : mm:ss UTC\"\n"
    "# The Day of Month(DD) MUST be 2 - digit; use either preceding zero or trailing space\n"
    "[Season 29]\n"
    "Start \"Sat, 09 Feb 2025 00:00:00 GMT\"\n"
    "End \"Tue, 09 Feb 2026 01:00:00 GMT\"\n\n";

constinit const char g_cSzConfigSwap[] =
    "HeroPublishFrequencyMinutes \"30\"\n"
    "EnableCrossPlatformSaveMigration \"1\"\n"
    "SeasonalGlobalLeaderboardsEnabled \"1\"\n"
    "EnableDiablo4Advertisement \"1\"\n"
    "CommunityBuffStart \"???, 16 Sep 2023 00:00:00 GMT\"\n"
    "CommunityBuffEnd \"???, 01 Dec 2027 01:00:00 GMT\"\n"
    "CommunityBuffDoubleGoblins \"1\"\n"
    "CommunityBuffDoubleBountyBags \"1\"\n"
    "CommunityBuffRoyalGrandeur \"1\"\n"
    "CommunityBuffLegacyOfNightmares \"1\"\n"
    "CommunityBuffTriunesWill \"1\"\n"
    "CommunityBuffPandemonium \"1\"\n"
    "CommunityBuffKanaiPowers \"1\"\n"
    "CommunityBuffTrialsOfTempests \"1\"\n"
    // "CommunityBuffSeasonOnly \"1\"\n" // anything but 1 or notexist crashes on drop
    "CommunityBuffShadowClones \"1\"\n"
    "CommunityBuffFourthKanaisCubeSlot \"1\"\n"
    "CommunityBuffEtherealItems \"1\"\n"
    "CommunityBuffSoulShards \"1\"\n"
    "CommunityBuffSwarmRifts \"1\"\n"
    "CommunityBuffSanctifiedItems \"1\"\n"
    "CommunityBuffDarkAlchemy \"1\"\n"
    "CommunityBuffParagonCap \"0\"\n"
    "CommunityBuffNestingPortals \"1\"\n"
    "CommunityBuffLegendaryFind \"1337420666999.9\"\n"  // nonstandard
    "CommunityBuffGoldFind \"1337420666999.8\"\n"       // nonstandard
    "CommunityBuffXP \"1337420666999.7\"\n"             // nonstandard
    "CommunityBuffEasterEggWorld \"1\"\n"               // nonstandard
    "CommunityBuffDoubleRiftKeystones \"1\"\n"          // nonstandard
    "CommunityBuffDoubleBloodShards \"1\"\n"            // nonstandard
    "UpdateVersion \"1\"\n";
