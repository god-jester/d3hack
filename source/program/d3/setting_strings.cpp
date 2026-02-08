#include "program/d3/setting.hpp"

const char g_cSzSeasonSwap[] =
    "# Format for dates MUST be: \" ? ? ? , DD MMM YYYY hh : mm:ss UTC\"\n"
    "# The Day of Month(DD) MUST be 2-digit; use either preceding zero or trailing space\n"
    "[Season 29]\n"
    "Start \"Sat, 09 Feb 2025 00:00:00 GMT\"\n"
    "End \"Tue, 09 Feb 2026 01:00:00 GMT\"\n\n";

const char g_cSzConfigSwap[] =
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
