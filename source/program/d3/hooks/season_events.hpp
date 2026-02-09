#pragma once

#include "lib/hook/trampoline.hpp"
#include "program/config.hpp"
#include "d3/types/common.hpp"
#include "d3/_util.hpp"
#include "symbols/common.hpp"
#include <string>
// #include <string_view>

namespace d3 {
    namespace {
        auto IsLocalConfigReady() -> bool {
            return global_config.initialized;
        }

        // Mailbox UI is used by the season swap flow to deliver unequipped items
        // back to the player via save-backed mail. The stock UI gates the inbox
        // behind a lobby connectivity check and pops a "mail unavailable" error
        // when offline; for season swapping we want retrieval to work offline.
        //
        // Keep this scoped: only bypass the check when seasons are actively
        // overridden by d3hack.
        HOOK_DEFINE_TRAMPOLINE(MailboxCheckLobbyConnection) {
            static bool Callback(void *self) {
                if (global_config.initialized && global_config.seasons.active) {
                    return true;
                }
                return Orig(self);
            }
        };

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

        constexpr char   k_config_cache_name[]        = "config.txt";
        constexpr char   k_seasons_cache_fallback[]   = "seasons_config.txt";
        constexpr char   k_blacklist_cache_fallback[] = "blacklist_config.txt";
        constexpr size_t k_cache_path_max             = 512;

        const char *GetSeasonsConfigFilename() {
            const auto *params = CmdLineGetParams();
            if (params && params->szSeasonsConfigFilename[0] != '\0') {
                return params->szSeasonsConfigFilename;
            }
            return k_seasons_cache_fallback;
        }

        const char *GetBlacklistConfigFilename() {
            const auto *params = CmdLineGetParams();
            if (params && params->szBlacklistConfigFilename[0] != '\0') {
                return params->szBlacklistConfigFilename;
            }
            return k_blacklist_cache_fallback;
        }

        bool GetBlzStringData(const blz::shared_ptr<blz::string> *pszFileData, const char **out_data, size_t *out_size) {
            if (out_data == nullptr || out_size == nullptr) {
                return false;
            }
            *out_data = nullptr;
            *out_size = 0;
            if (pszFileData == nullptr || pszFileData->m_pointer == nullptr) {
                return false;
            }
            const blz::string *str = pszFileData->m_pointer;
            if (str == nullptr) {
                return false;
            }
            const char  *data = str->m_elements ? str->m_elements : str->m_storage;
            const size_t size = static_cast<size_t>(str->m_size);
            if (data == nullptr || size == 0) {
                return false;
            }
            *out_data = data;
            *out_size = size;
            return true;
        }

        bool HasPubfileData(const blz::shared_ptr<blz::string> *pszFileData) {
            const char *data = nullptr;
            size_t      size = 0;
            return GetBlzStringData(pszFileData, &data, &size);
        }

        bool CachePubfile(const char *cache_path, const blz::shared_ptr<blz::string> *pszFileData, const char *log_line) {
            const char *data = nullptr;
            size_t      size = 0;
            if (!GetBlzStringData(pszFileData, &data, &size)) {
                return false;
            }
            EnsurePubfileCacheDir();
            if (WriteTestFile(cache_path, const_cast<char *>(data), size)) {
                PRINT_LINE(log_line);
                return true;
            }
            return false;
        }

        bool LoadCachedPubfile(const char *cache_path, blz::shared_ptr<blz::string> *pszFileData, blz::string &fallback, const char *log_line) {
            if (pszFileData == nullptr) {
                return false;
            }
            u32 size = 0;
            if (char *buffer = ReadFileToBuffer(cache_path, &size); buffer) {
                EnsureSharedPtrData(pszFileData, fallback);
                ReplaceBlzString(*pszFileData->m_pointer, buffer, size);
                SigmaMemoryFree(&buffer, nullptr);
                PRINT_LINE(log_line);
                return true;
            }
            return false;
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
            auto       result = eResult;
            char       cache_path[k_cache_path_max] {};
            const bool has_path = BuildPubfileCachePath(cache_path, sizeof(cache_path), k_config_cache_name);
            const bool has_data = HasPubfileData(pszFileData);
            if (has_path && (result == 0 || has_data)) {
                CachePubfile(cache_path, pszFileData, "[pubfiles] cached config file");
            }
            if (has_path && (result != 0 || !has_data)) {
                static blz::string s_cached_config;
                if (LoadCachedPubfile(cache_path, pszFileData, s_cached_config, "[pubfiles] using cached config file")) {
                    result = 0;
                }
            }
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
            auto       result = eResult;
            char       cache_path[k_cache_path_max] {};
            const bool has_path = BuildPubfileCachePath(cache_path, sizeof(cache_path), GetSeasonsConfigFilename());
            const bool has_data = HasPubfileData(pszFileData);
            if (has_path && (result == 0 || has_data)) {
                CachePubfile(cache_path, pszFileData, "[pubfiles] cached seasons file");
            }
            if (has_path && (result != 0 || !has_data)) {
                static blz::string s_cached_seasons;
                if (LoadCachedPubfile(cache_path, pszFileData, s_cached_seasons, "[pubfiles] using cached seasons file")) {
                    result = 0;
                }
            }
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
            auto       result = eResult;
            char       cache_path[k_cache_path_max] {};
            const bool has_path = BuildPubfileCachePath(cache_path, sizeof(cache_path), GetBlacklistConfigFilename());
            const bool has_data = HasPubfileData(pszFileData);
            if (has_path && (result == 0 || has_data)) {
                CachePubfile(cache_path, pszFileData, "[pubfiles] cached blacklist file");
            }
            if (has_path && (result != 0 || !has_data)) {
                static blz::string s_cached_blacklist;
                if (LoadCachedPubfile(cache_path, pszFileData, s_cached_blacklist, "[pubfiles] using cached blacklist file")) {
                    result = 0;
                }
            }
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
        MailboxCheckLobbyConnection::
            InstallAtFuncPtr(MailCheckLobbyConnection);
    }
}  // namespace d3
