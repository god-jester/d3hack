#pragma once

#include "lib/hook/trampoline.hpp"
#include "../../config.hpp"
#include "d3/types/common.hpp"
#include <string>
#include <string_view>

namespace d3 {
    namespace {
        auto IsLocalConfigReady() -> bool {
            return global_config.initialized;
        }

        void ClearConfigRequestFlag() {
            auto *flag_ptr = reinterpret_cast<u32 **>(GameOffsetFromTable("season_config_request_flag_ptr"));
            if ((flag_ptr != nullptr) && (*flag_ptr != nullptr))
                **flag_ptr = 0;
        }

        void ClearSeasonsRequestFlag() {
            auto *flag_ptr = reinterpret_cast<u8 **>(GameOffsetFromTable("season_seasons_request_flag_ptr"));
            if ((flag_ptr != nullptr) && (*flag_ptr != nullptr))
                **flag_ptr = 0;
        }

        void ClearBlacklistRequestFlag() {
            auto *flag_ptr = reinterpret_cast<u8 **>(GameOffsetFromTable("season_blacklist_request_flag_ptr"));
            if ((flag_ptr != nullptr) && (*flag_ptr != nullptr))
                **flag_ptr = 0;
        }

        void ReplaceBlzString(blz::string &dst, const char *data, size_t len) {
            if (dst.m_elements) {
                const char *storage_start = &dst.m_storage[0];
                const char *storage_end   = storage_start + sizeof(dst.m_storage);
                if (dst.m_elements < storage_start || dst.m_elements >= storage_end)
                    SigmaMemoryFree(dst.m_elements, nullptr);
            }

            auto *buf = static_cast<char *>(SigmaMemoryNew(len + 1, 0, nullptr, true));
            if (!buf) {
                dst.m_elements             = nullptr;
                dst.m_size                 = 0;
                dst.m_capacity             = 0;
                dst.m_capacity_is_embedded = 0;
                return;
            }
            if ((len != 0u) && (data != nullptr))
                SigmaMemoryMove(buf, const_cast<char *>(data), len);
            buf[len]                   = '\0';
            dst.m_elements             = buf;
            dst.m_size                 = len;
            dst.m_capacity             = len;
            dst.m_capacity_is_embedded = 0;
        }

        void ReplaceBlzString(blz::string &dst, const char *data) {
            const char *safe_data = (data != nullptr) ? data : "";
            ReplaceBlzString(dst, safe_data, std::char_traits<char>::length(safe_data));
        }

        void ReplaceBlzString(blz::string &dst, const blz::string &data) {
            const char *safe_data = data.m_elements ? data.m_elements : "";
            ReplaceBlzString(dst, safe_data, static_cast<size_t>(data.m_size));
        }

        auto BuildSeasonSwapString(u32 seasonNumber) -> blz::string {
            return blz_make_stringf(
                "# Format for dates MUST be: \" ? ? ? , DD MMM YYYY hh : mm:ss UTC\"\n"
                "# The Day of Month(DD) MUST be 2 - digit; use either preceding zero or trailing space\n"
                "[Season %u]\n"
                "Start \"Sat, 09 Feb 2025 00:00:00 GMT\"\n"
                "End \"Tue, 09 Feb 2036 01:00:00 GMT\"\n\n",
                seasonNumber
            );
        }

        auto BoolToConfig(bool value) -> const char * {
            return value ? "1" : "0";
        }

        void AppendConfigLine(std::string &out, const char *key, const char *value) {
            out += key;
            out += " \"";
            out += value;
            out += "\"\n";
        }

        void AppendConfigBool(std::string &out, const char *key, bool value) {
            AppendConfigLine(out, key, BoolToConfig(value));
        }

        auto BuildConfigSwapString() -> std::string {
            std::string out;
            out.reserve(1024);
            AppendConfigLine(out, "HeroPublishFrequencyMinutes", "30");
            AppendConfigLine(out, "EnableCrossPlatformSaveMigration", "1");
            AppendConfigLine(out, "SeasonalGlobalLeaderboardsEnabled", "1");
            AppendConfigLine(out, "EnableDiablo4Advertisement", "1");
            AppendConfigLine(out, "CommunityBuffStart", "???, 16 Sep 2023 00:00:00 GMT");
            AppendConfigLine(out, "CommunityBuffEnd", "???, 01 Dec 2027 01:00:00 GMT");
            AppendConfigBool(out, "CommunityBuffDoubleGoblins", global_config.events.DoubleTreasureGoblins);
            AppendConfigBool(out, "CommunityBuffDoubleBountyBags", global_config.events.DoubleBountyBags);
            AppendConfigBool(out, "CommunityBuffRoyalGrandeur", global_config.events.RoyalGrandeur);
            AppendConfigBool(out, "CommunityBuffLegacyOfNightmares", global_config.events.LegacyOfNightmares);
            AppendConfigBool(out, "CommunityBuffTriunesWill", global_config.events.TriunesWill);
            AppendConfigBool(out, "CommunityBuffPandemonium", global_config.events.Pandemonium);
            AppendConfigBool(out, "CommunityBuffKanaiPowers", global_config.events.KanaiPowers);
            AppendConfigBool(out, "CommunityBuffTrialsOfTempests", global_config.events.TrialsOfTempests);
            AppendConfigBool(out, "CommunityBuffSeasonOnly", false);
            AppendConfigBool(out, "CommunityBuffShadowClones", global_config.events.ShadowClones);
            AppendConfigBool(out, "CommunityBuffFourthKanaisCubeSlot", global_config.events.FourthKanaisCubeSlot);
            AppendConfigBool(out, "CommunityBuffEtherealItems", global_config.events.EtherealItems);
            AppendConfigBool(out, "CommunityBuffSoulShards", global_config.events.SoulShards);
            AppendConfigBool(out, "CommunityBuffSwarmRifts", global_config.events.SwarmRifts);
            AppendConfigBool(out, "CommunityBuffSanctifiedItems", global_config.events.SanctifiedItems);
            AppendConfigBool(out, "CommunityBuffDarkAlchemy", global_config.events.DarkAlchemy);
            AppendConfigBool(out, "CommunityBuffParagonCap", false);
            AppendConfigBool(out, "CommunityBuffNestingPortals", global_config.events.NestingPortals);
            AppendConfigLine(out, "CommunityBuffLegendaryFind", "1337420666999.9");
            AppendConfigLine(out, "CommunityBuffGoldFind", "1337420666999.8");
            AppendConfigLine(out, "CommunityBuffXP", "1337420666999.7");
            AppendConfigBool(out, "CommunityBuffEasterEggWorld", global_config.events.EasterEggWorldEnabled);
            AppendConfigBool(out, "CommunityBuffDoubleRiftKeystones", global_config.events.DoubleRiftKeystones);
            AppendConfigBool(out, "CommunityBuffDoubleBloodShards", global_config.events.DoubleBloodShards);
            AppendConfigLine(out, "UpdateVersion", "1");
            return out;
        }

        void EnsureSharedPtrData(blz::shared_ptr<blz::string> *pszFileData, blz::string &fallback) {
            if (!pszFileData)
                return;
            if (!pszFileData->m_pointer) {
                pszFileData->m_pointer = &fallback;
            }
        }

        void OverrideConfigIfNeeded(blz::shared_ptr<blz::string> *pszFileData) {
            if (!pszFileData || !global_config.events.active)
                return;
            static blz::string s_fallback;
            EnsureSharedPtrData(pszFileData, s_fallback);
            auto swap = BuildConfigSwapString();
            ReplaceBlzString(*pszFileData->m_pointer, swap.c_str());
        }

        void OverrideSeasonsIfNeeded(blz::shared_ptr<blz::string> *pszFileData) {
            if (!pszFileData || !global_config.seasons.active)
                return;
            static blz::string s_fallback;
            EnsureSharedPtrData(pszFileData, s_fallback);
            auto swap = BuildSeasonSwapString(global_config.seasons.current_season);
            ReplaceBlzString(*pszFileData->m_pointer, swap);
        }

        void OverrideBlacklistIfNeeded(blz::shared_ptr<blz::string> *pszFileData) {
            if (!pszFileData)
                return;
            static constexpr const char kEmptyBlacklist[] =
                "# Blacklist overridden by d3hack\n"
                "[GBID]\n\n"
                "[SNO]\n";
            static blz::string s_fallback;
            EnsureSharedPtrData(pszFileData, s_fallback);
            ReplaceBlzString(*pszFileData->m_pointer, kEmptyBlacklist);
        }

    }  // namespace

    HOOK_DEFINE_TRAMPOLINE(ConfigFileRetrieved) {
        static void Callback(Console::Online::LobbyServiceInternal *self, int32 eResult, blz::shared_ptr<blz::string> *pszFileData) {
            if (!IsLocalConfigReady()) {
                ClearConfigRequestFlag();
                return;
            }
            auto result = eResult;
            if (global_config.events.active) {
                result = 0;
                OverrideConfigIfNeeded(pszFileData);
            }
            Orig(self, result, pszFileData);
        }
    };

    HOOK_DEFINE_TRAMPOLINE(SeasonsFileRetrieved) {
        static void Callback(Console::Online::LobbyServiceInternal *self, int32 eResult, blz::shared_ptr<blz::string> *pszFileData) {
            if (!IsLocalConfigReady()) {
                ClearSeasonsRequestFlag();
                return;
            }
            auto result = eResult;
            if (global_config.seasons.active) {
                result = 0;
                OverrideSeasonsIfNeeded(pszFileData);
            }
            Orig(self, result, pszFileData);
        }
    };

    HOOK_DEFINE_TRAMPOLINE(BlacklistFileRetrieved) {
        static void Callback(Console::Online::LobbyServiceInternal *self, int32 eResult, blz::shared_ptr<blz::string> *pszFileData) {
            if (!IsLocalConfigReady()) {
                ClearBlacklistRequestFlag();
                return;
            }
            /*
                # Blacklist of items for consoles

                # items using GBID that are blacklisted
                # format: <GBIDName:string>,<allowDrop:int>
                [GBID]

                # powers using SNO that are blacklisted
                # format: <SNOGroup:string>,<SNOName:string>,<allowSpawn:int>
                # note: strings should not contain quotes
                # note: SNOGroup is usually Power
                [SNO]            
            */
            auto result = eResult;
            OverrideBlacklistIfNeeded(pszFileData);
            Orig(self, result, pszFileData);
        }
    };

    void SetupSeasonEventHooks() {
        ConfigFileRetrieved::
            InstallAtFuncPtr(OnConfigFileRetrieved);
        SeasonsFileRetrieved::
            InstallAtFuncPtr(OnSeasonsFileRetrieved);
        BlacklistFileRetrieved::
            InstallAtFuncPtr(OnBlacklistFileRetrieved);
    }
}  // namespace d3
