#pragma once

#include "symbols/common.hpp"
#include "d3/types/sno.hpp"
#include "d3/types/attributes.hpp"
#include "d3/types/maps.hpp"
#include "d3/types/namespaces.hpp"
#include "d3/gamebalance.hpp"
#include "d3/setting.hpp"
#include "../config.hpp"
#include "lib/util/stack_trace.hpp"
#include "lib/util/sys/modules.hpp"
#include "lib/util/strings.hpp"
#include <cstddef>
#include <cstdio>

#define HASHMAP(m, id) ((m & (16777619 * ((16777619 * ((16777619 * ((16777619 * _BYTE(id ^ 0x811C9DC5)) ^ BYTE1(id))) ^ BYTE2(id))) ^ HIBYTE(id)))))

// namespace blz {

//     template<class CharT, class TraitsT, class AllocatorT>
//     blz::basic_string<CharT, TraitsT, AllocatorT>::basic_string() {
//         AllocatorT alloc;
//         blz_basic_string(this, "\0", alloc);
//         // return string;
//     }

//     template<class CharT, class TraitsT, class AllocatorT>
//     blz::basic_string<CharT, TraitsT, AllocatorT>::basic_string(char *str, u32 dwSize) {
//         PRINT_EXPR("entering ctor for %p", this)
//         AllocatorT alloc;
//         auto      *lpBuf = static_cast<char *>(alloca(dwSize));
//         memset(lpBuf, 0x6A, dwSize);
//         blz_basic_string(this, lpBuf, alloc);  // *this = blz_string_ctor(lpBuf, alloc);
//         PRINT("3 %p", this)
//         PRINT("4 %s", this->m_elements)
//         PRINT("4.1 %ld", this->m_size)
//         m_elements = str;
//         PRINT("7 %s", this->m_elements)
//         PRINT("7.1 %ld", this->m_size)
//     }

// }  // namespace blz

namespace d3 {
    std::string              g_szBaseDir     = "sd:/config/d3hack-nx";
    constinit bool           sg_bServerCode  = true;
    SigmaThreadLocalStorage &s_tThreadData   = *reinterpret_cast<SigmaThreadLocalStorage *const>(GameOffsetFromTable("sigma_thread_data"));
    AttribDef *const        &cg_arAttribDefs = reinterpret_cast<AttribDef *const>(GameOffsetFromTable("attrib_defs"));
    AppGlobals              &sg_tAppGlobals  = *reinterpret_cast<AppGlobals *const>(GameOffsetFromTable("app_globals"));
    OnlineService::ItemId   &ItemInvalid     = *reinterpret_cast<OnlineService::ItemId *const>(GameOffsetFromTable("item_invalid"));
    WorldPlace              &cgplaceNull     = *reinterpret_cast<WorldPlace *const>(GameOffsetFromTable("world_place_null"));
    GameCommonData          *g_ptGCData;
    GfxInternalData         *g_ptGfxData;
    RWindow                 *g_ptMainRWindow;
    VariableResRWindowData  *g_ptRWindowData;
    void                    *g_ptLobbyServiceInternal;
    GameConnectionID         gs_idGameConnection;
    constinit bool           g_request_seasons_load = false;

    auto &s_varLocalLoggingEnable     = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_local_logging_enable_ptr"));
    auto &s_varOnlineServicePTR       = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_online_service_ptr"));
    auto &s_varFreeToPlay             = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_free_to_play_ptr"));
    auto &s_varSeasonsOverrideEnabled = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_seasons_override_enabled_ptr"));
    auto &s_varChallengeRiftEnabled   = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_challengerift_enabled_ptr"));
    auto &s_varSeasonNum              = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_season_num_ptr"));
    auto &s_varSeasonState            = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_season_state_ptr"));
    auto &s_varMaxParagonLevel        = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_max_paragon_level_ptr"));
    auto &s_varExperimentalScheduling = **reinterpret_cast<uintptr_t **const>(GameOffsetFromTable("xvar_experimental_scheduling_ptr"));
    auto &s_varDoubleRiftKeystones    = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_double_keystones"));
    auto &s_varDoubleBloodShards      = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_double_blood_shards"));
    auto &s_varDoubleTreasureGoblins  = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_double_goblins"));
    auto &s_varDoubleBountyBags       = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_double_bounty_bags"));
    auto &s_varRoyalGrandeur          = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_royal_grandeur"));
    auto &s_varLegacyOfNightmares     = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_legacy_of_nightmares"));
    auto &s_varTriunesWill            = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_triunes_will"));
    auto &s_varPandemonium            = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_pandemonium"));
    auto &s_varKanaiPowers            = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_kanai_powers"));
    auto &s_varTrialsOfTempests       = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_trials_of_tempests"));
    auto &s_varShadowClones           = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_shadow_clones"));
    auto &s_varFourthKanaisCubeSlot   = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_fourth_cube_slot"));
    auto &s_varEtherealItems          = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_ethereal_items"));
    auto &s_varSoulShards             = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_soul_shards"));
    auto &s_varSwarmRifts             = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_swarm_rifts"));
    auto &s_varSanctifiedItems        = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_sanctified_items"));
    auto &s_varDarkAlchemy            = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_dark_alchemy"));
    auto &s_varNestingPortals         = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_nesting_portals"));
    auto &s_varParagonCap             = *reinterpret_cast<uintptr_t *const>(GameOffsetFromTable("event_paragon_cap"));

    using std::unordered_map;
    using std::vector;

    unordered_map<ACDID, bool>   sg_mapLobby;
    unordered_map<Attrib, int32> g_mapCheckedAttribs;

    inline void PrintAddressWithModule(const char *label, uintptr_t address) {
        const auto *module = exl::util::TryGetModule(address);
        if (module) {
            char name_buf[exl::util::ModuleInfo::s_ModulePathLengthMax + 1];
            exl::util::CopyString(name_buf, module->GetModuleName());
            const auto offset = address - module->m_Total.m_Start;
            PRINT("%s: %s+0x%lx (0x%lx)", label, name_buf, offset, address);
        } else {
            PRINT("%s: 0x%lx", label, address);
        }
    }

    inline void DumpStackTrace(const char *tag, uintptr_t fp, size_t max_depth = 16) {
        if (fp == 0) {
            PRINT("%s stack trace unavailable (fp=0)", tag);
            return;
        }
        auto it = exl::util::stack_trace::Iterator(fp, exl::util::mem_layout::s_Stack);
        PRINT("%s stack trace:", tag);
        size_t depth = 0;
        while (it.Step() && depth < max_depth) {
            char label[24];
            snprintf(label, sizeof(label), "  #%zu", depth);
            PrintAddressWithModule(label, it.GetReturnAddress());
            depth++;
        }
    }

    auto GetItemPtr(GBID gbidItem) -> const Item * {
        if (ItemAppGlobals *ptItemAppGlobals = sg_tAppGlobals.ptItemAppGlobals; ptItemAppGlobals) {
            auto  g_mapItems = ptItemAppGlobals->mapItems;
            auto  nHashMask  = g_mapItems.m_map.m_nHashMask;
            auto  pDebugPtr  = g_mapItems.m_map.m_arHashTable.m_array.m_pData._anon_0.m_pDebugPtr;
            auto *pAssocItem = pDebugPtr[HASHMAP(nHashMask, gbidItem)];

            while (pAssocItem) {
                if (pAssocItem->m_Key != gbidItem) {
                    pAssocItem = pAssocItem->m_pNext;
                } else {
                    return pAssocItem->m_Value;
                }
            }
        }
        return nullptr;
    }

    void AllGBIDsOfType(GameBalanceType eType, vector<GBID> *ids) {
        GBHandleList listResults = {};
        memset((void *)&listResults, 0, 20);
        listResults.m_list.m_ptNodeAllocator = GBGetHandlePool();
        listResults.m_ConstructorCalled      = 1;
        GBEnumerate(eType, &listResults);
        ids->clear();
        if (s32 nCount = listResults.m_list.m_nCount; nCount > 0) {
            PRINT_EXPR("%d", nCount)
            ids->reserve(nCount);
            auto p_gbH = listResults.m_list.m_pHead;
            while (p_gbH && p_gbH->m_tData.eType == eType) {
                // auto eItem = p_gbH->m_tData.gbid;
                ids->emplace_back(p_gbH->m_tData.gbid);
                p_gbH = p_gbH->m_pNext;
                // auto gbString = d3::GbidStringAll(eItem);
                // PRINT_EXPR("Trying to unlock %i = %s", eItem, gbString)
                // _SERVERCODE_OFF
            }
            PRINT_EXPR("DONE: %d", nCount)
            PRINT_EXPR("%d (%ld, %ld) %i | %i", eType, ids->size(), ids->capacity(), ids->front(), ids->back())
        }
        ids->shrink_to_fit();
        return;
        if (u32 nCount = listResults.m_list.m_nCount; nCount > 0) {
            PRINT_EXPR("%d", nCount)
            ids->reserve(nCount);
            // PRINT_EXPR("RESERVED %d", nCount)
            auto tData = listResults.m_list.m_pHead;
            while (tData) {
                ids->emplace_back(tData->m_tData.gbid);
                // PRINT("%d", tData->m_tData.gbid)
                tData = tData->m_pNext;
            }
            // PRINT_EXPR("%d (%ld, %ld) %i | %i", eType, ids.size(), ids.capacity(), ids.front(), ids.back())
        }
        ids->shrink_to_fit();
        // return ids;
    }

    auto SPlayerIsPrimary(Player *ptPlayer) -> bool { return ptPlayer && *FollowPtr<bool, 0xDB78>(ptPlayer); }  // bIsPrimaryPlayerServerOnly

    auto SPlayerGetHost() -> Player * {
        if (GameIsStartUp())
            return nullptr;

        Player *ret = PlayerGetFirstAll();
        for (auto j = ret; j; j = PlayerGetNextAll(j))
            if (SPlayerIsLocal(j))
                ret = j;
        return ret;
    }

    auto SPlayerGetHostOld() -> Player * {
        if (!_HOSTCHK)
            return nullptr;
        _SERVERCODE_ON
        Player *ret             = nullptr;
        auto    tSGameGlobals   = SGameGlobalsGet();
        auto    sGameConnection = ServerGetOnlyGameConnection();
        auto    sGameCurID      = AppServerGetOnlyGame();
        auto    ptPrimary       = GetPrimaryPlayerForGameConnection(sGameConnection);
        PRINT(
            "NEW SPlayerGetHostDbg! (SGame: %x | Connection: %x | Primary for connection: %p->%lx) %s %s",
            sGameCurID, sGameConnection, ptPrimary, ptPrimary->uHeroId, tSGameGlobals->uszCreatorAccountName, tSGameGlobals->uszCreatorHeroName
        );
        for (auto j = PlayerGetFirstAll(); j; j = PlayerGetNextAll(j)) {
            PRINT_EXPR("PlayerGet: %p->%lx | %i", j, j->uHeroId, SPlayerIsLocal(j))
            if (SPlayerIsLocal(j))
                ret = j;
        }
        _SERVERCODE_OFF
        return ret;
    }

    auto PlayerHeroMatchesLocal(Player *ptPlayer) -> bool {
        if (Player *ptLocal = LocalPlayerGet(); ptLocal && ptPlayer)
            return ptLocal->uHeroId == ptPlayer->uHeroId;
        return false;
    }

    auto IsACDIDHost(ACDID idPlayerACD) -> bool {  // need ANNtoACD() to check LocalPlayer
        if (Player *ptPlayer = PlayerGetByACD(idPlayerACD); ptPlayer)
            return PlayerHeroMatchesLocal(ptPlayer) || SPlayerIsLocal(ptPlayer);
        if (Player *ptHost = SPlayerGetHost(); ptHost)
            return idPlayerACD != ACDID_INVALID && ptHost->idPlayerACD == idPlayerACD;
        return false;
    }

    auto MakeAttribKey(Attrib eAttrib, int32 nParam = -1) -> FastAttribKey {
        return FastAttribKey {(eAttrib | (nParam << 12))};
    }

    auto KeyGetFullAttrib(FastAttribKey tKey) -> int32 {
        return tKey.nValue ^ (tKey.nValue >> 12);  // ^ 0xFFFFF000; }
    }

    auto KeyGetAttrib(FastAttribKey tKey) -> int32 {
        return tKey.nValue & 0xFFF;
    }

    auto KeyGetParam(FastAttribKey tKey) -> int64 {
        return tKey.nValue >> 12;
    }

    auto GbidStringAll(GBID gbid) -> LPCSTR {
        return GBIDToStringAll(&gbid).str();
    }

    auto GbidCrefStringAll(GBID gbid) -> CRefString {
        return GBIDToStringAll(&gbid);
    }

    auto GbidString(GBID gbid, GameBalanceType eGBType = GB_ITEMS, const BOOL fUsedForXMLFields = 0) -> LPCSTR {
        CRefString const result(GBIDToString(gbid, eGBType, fUsedForXMLFields));
        return result.str();
    }

    void IntAttribSet(ActorCommonData *tACD, int32 eAttribute, int32 nParam = -1, int32 nIntValue = 1) {
        ACD_AttributesSetInt(tACD, MakeAttribKey(eAttribute, nParam), nIntValue);
    }

    auto ParamToStr(int attr, uint param) -> LPCSTR {
        return AttribParamToString(attr, param).str();  // NOLINT
    }

    auto AttribToStr(FastAttribKey tKey) -> LPCSTR {
        return FastAttribToString(tKey).str();
    }

    void AttribStringInfo(FastAttribKey tKey, FastAttribValue tValue) {
        auto eKeyAttrib = KeyGetAttrib(tKey);
        ++g_mapCheckedAttribs[eKeyAttrib];
        auto attrDef = cg_arAttribDefs[eKeyAttrib];
        // if (g_CheckedAttribs[eKeyAttrib] > 3) // || g_CheckedAttribs[eKeyAttrib] < 200)
        //     return;
        if (g_mapCheckedAttribs[eKeyAttrib] <= 9) {  // eParam > 0 && !isdigit((int)szParam[0]) && (g_CheckedAttribs[eKeyAttrib] < 9)) {
            auto eAttrib = eKeyAttrib;               // KeyGetFullAttrib(tKey);
            // if (eAttrib == eKeyAttrib)
            //     return;
            PRINT("\n"
                  "eAttrib: %i\n"
                  "eParameterType: %i\n"
                  "eAttributeOperator: %i\n"
                  "eAttributeDataType: %i\n"
                  "pszFormulaItem \"%s\"\n"
                  "pszFormulaCharacter \"%s\"\n"
                  "pszName \"%s\"\n"
                  "*ptCompressor: [%p]\n"
                  "uSyncFlags: 0x%x",
                  attrDef.eAttrib, attrDef.eParameterType, attrDef.eAttributeOperator, attrDef.eAttributeDataType, attrDef.pszFormulaItem, attrDef.pszFormulaCharacter, attrDef.pszName, attrDef.ptCompressor, attrDef.uSyncFlags);

            if (attrDef.eAttributeDataType == 1) {
                auto tDefaultValue = attrDef.tDefaultValue._anon_0.nValue;
                PRINT("%s.tDefaultValue: %i\n", attrDef.pszName, tDefaultValue)
            } else {
                auto tDefaultValue = attrDef.tDefaultValue._anon_0.flValue;
                PRINT("%s.tDefaultValue: %ff\n", attrDef.pszName, tDefaultValue)
            }
            // return;
            auto        eParam   = KeyGetParam(tKey);
            const auto *szAttrib = AttribToStr(tKey);
            const auto *szParam  = ParamToStr(eKeyAttrib, eParam);
            // int32 AttribStringToParam(Attrib eAttrib, LPCSTR szParam)
            auto eStrToParam = AttribStringToParam(eKeyAttrib, szAttrib);

            PRINT(
                "%s eAttrib = 0x%x (%i) | eKeyAttrib = 0x%x (%i) | Int = %i | nValue = %i\n\teParam = %li szParam = %s eStrToParam = %i\n---",
                szAttrib, eAttrib, eAttrib, eKeyAttrib, eKeyAttrib, tValue._anon_0.nValue, tKey.nValue, eParam, szParam, eStrToParam
            );
        }
        // PRINT("eAttrib = 0x%x (%i) | eKeyAttrib = 0x%x (%i) | nValue = %i", eAttrib, eAttrib, eKeyAttrib, eKeyAttrib, tKey.nValue)
    }

    void CheckStringList(int gid, LPCSTR szLabel, bool bSkipDef = false) {
        return;  // NOLINT
        for (auto &idx : AllStringListSnoEnum) {
            if (bSkipDef && (idx == 52006 || idx == 52008 || idx == 87075))
                continue;
            auto fetch = StringListFetchLPCSTR(idx, szLabel);
            if (fetch) {
                char *pch = strstr(fetch, "!!Missing!!");
                if (pch == NULL) {
                    PRINT("\n%i\t| snoindex[%i]\t(%s)\t=\t%s", gid, idx, szLabel, fetch)
                    // PRINT_EXPR("[0x%x | %s] %i = %s", i, g_tGBTables[i].name, tGBID, gbidString)
                    // PRINT_EXPR("%s\t=\tsnoindex[%i] = %s", gbidString, idx, fetch)
                }
            }
        }
    }

    char ByteToChar(uchar b) {
        b &= 0xF;
        if (0 <= b && b <= 0x9)
            return '0' + b;
        else
            return 'A' + (b - 0xA);
    };

    bool WriteTestFile(std::string szPath, void *ptBuf, size_type dwSize, bool bSuccess = false) {
        FileReference tFileRef;
        FileReferenceInit(&tFileRef, szPath.c_str());
        FileCreate(&tFileRef);
        if (FileOpen(&tFileRef, 2u)) {
            if (FileWrite(&tFileRef, ptBuf, -1, dwSize, ERROR_FILE_WRITE))
                bSuccess = true;
            FileClose(&tFileRef);
        }
        return bSuccess;
    }

    char *ReadFileToBuffer(std::string szPath, u32 *dwSize, FileReference tFileRef = {}) {
        if (FileReferenceInit(&tFileRef, szPath.c_str()); FileExists(&tFileRef))
            if (char *lpBuf = reinterpret_cast<char *>(FileReadIntoMemory(&tFileRef, dwSize, ERROR_FILE_READ)); lpBuf)
                return lpBuf;
        return nullptr;
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
        if (len && data)
            SigmaMemoryMove(buf, const_cast<char *>(data), len);
        buf[len]                   = '\0';
        dst.m_elements             = buf;
        dst.m_size                 = len;
        dst.m_capacity             = len;
        dst.m_capacity_is_embedded = 0;
    }

    blz::string BlizzStringFromFile(LPCSTR szFilenameSD, u32 dwSize = 0) {
        if (char *pFileBuffer = ReadFileToBuffer(szFilenameSD, &dwSize); pFileBuffer) {
            blz::string sReturnString;
            ReplaceBlzString(sReturnString, pFileBuffer, dwSize);
            SigmaMemoryFree(&pFileBuffer, 0LL);
            return sReturnString;
        }
        return blz::string {};
    }

    bool PopulateChallengeRiftData(D3::ChallengeRifts::ChallengeData &ptChalConf, D3::Leaderboard::WeeklyChallengeData &ptChalData) {
        auto PopulateData = [](google::protobuf::MessageLite *dest, const std::string &szPath, u32 dwSize = 0) -> bool {
            PRINT_EXPR("%s", szPath.c_str())
            if (char *pFileBuffer = ReadFileToBuffer(szPath, &dwSize); pFileBuffer) {
                blz::string sFileData;
                ReplaceBlzString(sFileData, pFileBuffer, dwSize);
                ParsePartialFromString(dest, &sFileData);
                SigmaMemoryFree(&pFileBuffer, 0LL);
                return true;
            }
            PRINT("Missing challenge rift file: %s", szPath.c_str());
            return false;
        };

        const u32 nMin  = global_config.challenge_rifts.range_start;
        const u32 nMax  = global_config.challenge_rifts.range_end;
        const u32 nPick = global_config.challenge_rifts.random
                              ? static_cast<u32>(GameRandRangeInt(static_cast<int>(nMin), static_cast<int>(nMax)))
                              : nMin;

        auto  szFormat = "/rift_data/challengerift_%02d.dat";
        auto  dwSize   = BITSIZEOF(szFormat) + 10;
        auto *lpBuf    = static_cast<char *>(alloca(dwSize));
        snprintf(lpBuf, dwSize + 1, szFormat, nPick);

        const auto config_ok = PopulateData(&ptChalConf, g_szBaseDir + "/rift_data/challengerift_config.dat");
        if (config_ok) {
            // PRINT_EXPR("%li", ptChalConf.challenge_end_unix_time_console_);
            // Force the challenge window to be "always active" offline.
            // Use 32-bit safe end time in case the game casts to s32.
            ptChalConf.challenge_number_                   = GameRandRangeInt(0, 900);
            ptChalConf.challenge_start_unix_time_          = 0;
            ptChalConf.challenge_last_broadcast_unix_time_ = 0;
            ptChalConf.challenge_end_unix_time_console_    = 0x7FFFFFFFLL;
            // PRINT_EXPR("post: %li", ptChalConf.challenge_end_unix_time_console_);
        }
        const auto data_ok = PopulateData(&ptChalData, g_szBaseDir + std::string(lpBuf));
        // PRINT_EXPR("%u", ptChalData.bnet_account_id_);
        PRINT_EXPR("%d | %d", config_ok, data_ok)
        return config_ok && data_ok;
    }

    [[maybe_unused]] void InterceptChallengeRift(void *bind, Console::Online::StorageResult *eRes, D3::ChallengeRifts::ChallengeData &ptChalConf, D3::Leaderboard::WeeklyChallengeData &ptChalData) {
        if (!PopulateChallengeRiftData(ptChalConf, ptChalData))
            return;
        *eRes = Console::Online::STORAGE_SUCCESS;
        sOnGetChallengeRiftData(bind, eRes, *(&ptChalConf), *(&ptChalData));
        PRINT("Called sOnGetChallengeRiftData! Result: %d", *eRes)
    };

}  // namespace d3
