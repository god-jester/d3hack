#pragma once

#include "symbols/common.hpp"
#include "nn/util/util_snprintf.hpp"

#define DBGLOG
#define DBGPRINT
#define D3HACK_VER     "D3Hack v2.2"
#define D3HACK_AUTHOR  "jester"
#define D3HACK_WEB     "https://jester.dev"
#define D3CLIENT_VER   "2.7.6.90885"
#define CRLF           "\n"
#define D3HACK_DESC    "Realtime hack for Diablo III: Eternal Collection" CRLF D3HACK_WEB
#define D3HACK_BUILD   "(Built by " D3HACK_AUTHOR " @ " __DATE__ " " __TIME__ ")"
#define D3HACK_FULLVER D3HACK_VER " by " D3HACK_AUTHOR " - Diablo III: Eternal Collection [v" D3CLIENT_VER "]"
#define D3HACK_FULLFPS D3HACK_VER D3HACK_BUILD
#define D3HACK_FULLWWW D3HACK_VER D3HACK_BUILD CRLF CRLF D3HACK_DESC

#ifndef DBGLOG
#define LOG(...) (static_cast<void>(sizeof(__VA_ARGS__)));
#else
#define LOG(fmt, ...) \
    (TraceInternal_Log(SLVL_INFO, 3u, OUTPUTSTREAM_DEFAULT, ": " fmt "\n", __VA_ARGS__));
#endif

#ifndef DBGPRINT
#define PRINT(...) (static_cast<void>(sizeof(__VA_ARGS__)));
// #define PRINT      LOG
#define PRINT_EXPR PRINT
#else
#define PRINT(fmt, ...)                                                     \
    {                                                                       \
        LOG(fmt, __VA_ARGS__)                                               \
        const auto nSize   = snprintf(nullptr, 0, fmt, __VA_ARGS__);        \
        char      *pBuf    = static_cast<char *>(alloca(nSize + 1));        \
        const auto nLength = snprintf(pBuf, (nSize + 1), fmt, __VA_ARGS__); \
        svcOutputDebugString(pBuf, nLength);                                \
    }
#define PRINT_EXPR(fmt, ...) \
    PRINT("\n> %s\n\t | " #__VA_ARGS__ ": " fmt, __PRETTY_FUNCTION__, __VA_ARGS__)  // __PRETTY_FUNCTION__ | __FUNCTION__
#endif

#define SNPRINT(buf, fmt, ...)                                     \
    {                                                              \
        const auto nSize = snprintf(nullptr, 0, fmt, __VA_ARGS__); \
        snprintf(buf, (nSize + 1), fmt, __VA_ARGS__);              \
    }

constinit const char c_szSeasonSwap[] =
    "# Format for dates MUST be: \" ? ? ? , DD MMM YYYY hh : mm:ss UTC\"\n"
    "# The Day of Month(DD) MUST be 2 - digit; use either preceding zero or trailing space\n"
    "[Season 29]\n"
    "Start \"Sat, 09 Feb 2025 00:00:00 GMT\"\n"
    "End \"Tue, 09 Feb 2026 01:00:00 GMT\"\n\n";

constinit const char c_szConfigSwap[] =
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
