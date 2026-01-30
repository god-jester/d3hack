#pragma once

#include "d3/types/sno.hpp"
#include "d3/types/error_manager.hpp"
#include "d3/setting.hpp"
#include "../symbols/common.hpp"
#include "../config.hpp"
#include "lib/util/stack_trace.hpp"
#include "lib/util/sys/modules.hpp"
#include "lib/util/strings.hpp"
#include <cstddef>
#include <cstdio>

#define HASHMAP(m, id) (((m) & (16777619 * ((16777619 * ((16777619 * ((16777619 * _BYTE((id) ^ 0x811C9DC5)) ^ BYTE1(id))) ^ BYTE2(id))) ^ HIBYTE(id)))))

namespace GFXNX64NVN {
    struct Globals;
}

namespace d3 {

    // NOTE: Keep this header-only and constant-initialized.
    // - Avoids dynamic initialization in a header (clang-tidy warning)
    // - Avoids depending on `std::string` at namespace-scope initialization time
    inline constexpr char g_szBaseDir[] = "sd:/config/d3hack-nx";
    // Runtime globals are declared here and defined once in a .cpp file.
    // This prevents ODR/multiple-definition issues when including this header from multiple TUs.
    extern bool                     sg_bServerCode;
    extern SigmaThreadLocalStorage &g_tThreadData;
    extern AttribDef *const         g_arAttribDefs;
    extern AppGlobals              &g_tAppGlobals;
    extern OnlineService::ItemId   &g_itemInvalid;
    extern WorldPlace              &g_cPlaceNull;
    extern SigmaGlobals            &g_tSigmaGlobals;
    extern u32                     *g_ptDevMemMode;
    extern ::GFXNX64NVN::Globals   *g_ptGfxNVNGlobals;
    extern GameCommonData          *g_ptGCData;
    extern GfxInternalData         *g_ptGfxData;
    extern RWindow                 *g_ptMainRWindow;
    extern VariableResRWindowData  *g_ptRWindowData;
    extern void                    *g_ptLobbyServiceInternal;
    extern GameConnectionID         g_idGameConnection;
    extern bool                     g_requestSeasonsLoad;

    extern uintptr_t &g_varLocalLoggingEnable;
    extern uintptr_t &g_varOnlineServicePTR;
    extern uintptr_t &g_varFreeToPlay;
    extern uintptr_t &g_varSeasonsOverrideEnabled;
    extern uintptr_t &g_varChallengeRiftEnabled;
    extern uintptr_t &g_varSeasonNum;
    extern uintptr_t &g_varSeasonState;
    extern uintptr_t &g_varMaxParagonLevel;
    extern uintptr_t &g_varExperimentalScheduling;
    extern uintptr_t &g_varDoubleRiftKeystones;
    extern uintptr_t &g_varDoubleBloodShards;
    extern uintptr_t &g_varDoubleTreasureGoblins;
    extern uintptr_t &g_varDoubleBountyBags;
    extern uintptr_t &g_varRoyalGrandeur;
    extern uintptr_t &g_varLegacyOfNightmares;
    extern uintptr_t &g_varTriunesWill;
    extern uintptr_t &g_varPandemonium;
    extern uintptr_t &g_varKanaiPowers;
    extern uintptr_t &g_varTrialsOfTempests;
    extern uintptr_t &g_varShadowClones;
    extern uintptr_t &g_varFourthKanaisCubeSlot;
    extern uintptr_t &g_varEtherealItems;
    extern uintptr_t &g_varSoulShards;
    extern uintptr_t &g_varSwarmRifts;
    extern uintptr_t &g_varSanctifiedItems;
    extern uintptr_t &g_varDarkAlchemy;
    extern uintptr_t &g_varNestingPortals;
    extern uintptr_t &g_varParagonCap;

    extern std::unordered_map<ACDID, bool>   g_mapLobby;
    extern std::unordered_map<Attrib, int32> g_mapCheckedAttribs;

    inline void PrintAddressWithModule(const char *label, uintptr_t address) {
        const auto *module = exl::util::TryGetModule(address);
        if (module != nullptr) {
            char name_buf[exl::util::ModuleInfo::s_ModulePathLengthMax + 1];
            exl::util::CopyString(name_buf, module->GetModuleName());
            const auto offset = address - module->m_Total.m_Start;
            PRINT("%s: %s+0x%lx (0x%lx)", label, name_buf, offset, address);
        } else {
            PRINT("%s: 0x%lx", label, address);
        }
    }

    inline void DumpStackTrace(const char *tag, uintptr_t fp, size_t maxDepth = 16) {
        if (fp == 0) {
            PRINT("%s stack trace unavailable (fp=0)", tag);
            return;
        }
        auto it = exl::util::stack_trace::Iterator(fp, exl::util::mem_layout::s_Stack);
        PRINT("%s stack trace:", tag);
        size_t depth = 0;
        while (it.Step() && depth < maxDepth) {
            char label[24];
            snprintf(label, sizeof(label), "  #%zu", depth);
            PrintAddressWithModule(label, it.GetReturnAddress());
            depth++;
        }
    }

    inline auto GetItemPtr(GBID gbidItem) -> const Item * {
        if (ItemAppGlobals const *ptItemAppGlobals = g_tAppGlobals.ptItemAppGlobals; ptItemAppGlobals) {
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

    inline void AllGBIDsOfType(GameBalanceType eType, std::vector<GBID> &ids) {
        GBHandleList listResults = {};
        memset(static_cast<void *>(&listResults), 0, 20);
        listResults.m_list.m_ptNodeAllocator = reinterpret_cast<XListMemoryPool<GBHandle> *>(GBGetHandlePool());
        listResults.m_ConstructorCalled      = 1;
        GBEnumerate(eType, &listResults);
        ids.clear();
        if (size_t nCount = listResults.m_list.m_nCount; nCount > 0) {
            PRINT_EXPR("%zu", nCount)
            // ids.reserve(nCount);
            auto p_gbH = listResults.m_list.m_pHead;
            while (p_gbH && p_gbH->m_tData.eType == eType) {
                // auto eItem = p_gbH->m_tData.gbid;
                // ids.emplace_back(p_gbH->m_tData.gbid);
                ids.insert(ids.end(), p_gbH->m_tData.gbid);
                p_gbH = p_gbH->m_pNext;
                // auto gbString = d3::GbidStringAll(eItem);
                // PRINT_EXPR("Trying to unlock %i = %s", eItem, gbString)
                // _SERVERCODE_OFF
            }
            PRINT_EXPR("DONE: %zu", nCount)
            PRINT_EXPR("%d (%ld, %ld) %i | %i", eType, ids.size(), ids.capacity(), ids.front(), ids.back())
        }
        ids.shrink_to_fit();
        return;
        if (u32 nCount = listResults.m_list.m_nCount; nCount > 0) {
            PRINT_EXPR("%d", nCount)
            ids.reserve(nCount);
            // PRINT_EXPR("RESERVED %d", nCount)
            auto tData = listResults.m_list.m_pHead;
            while (tData) {
                ids.emplace_back(tData->m_tData.gbid);
                // PRINT("%d", tData->m_tData.gbid)
                tData = tData->m_pNext;
            }
            // PRINT_EXPR("%d (%ld, %ld) %i | %i", eType, ids.size(), ids.capacity(), ids.front(), ids.back())
        }
        ids.shrink_to_fit();
        // return ids;
    }

    inline auto SPlayerIsPrimary(Player *ptPlayer) -> bool { return ptPlayer && *FollowPtr<bool, 0xDB78>(ptPlayer); }  // bIsPrimaryPlayerServerOnly

    inline auto SPlayerGetHost() -> Player * {
        if (GameIsStartUp() != 0)
            return nullptr;

        Player *ret = PlayerGetFirstAll();
        for (auto *j = ret; j != nullptr; j = PlayerGetNextAll(j))
            if (SPlayerIsLocal(j) != 0)
                ret = j;
        return ret;
    }

    inline auto SPlayerGetHostOld() -> Player * {
        if (!_HOSTCHK)
            return nullptr;
        _SERVERCODE_ON
        Player *ret             = nullptr;
        auto   *tSGameGlobals   = SGameGlobalsGet();
        auto    sGameConnection = ServerGetOnlyGameConnection();
        auto    sGameCurID      = AppServerGetOnlyGame();
        auto   *ptPrimary       = GetPrimaryPlayerForGameConnection(sGameConnection);
        PRINT(
            "NEW SPlayerGetHostDbg! (SGame: %x | Connection: %x | Primary for connection: %p->%lx) %s %s",
            sGameCurID, sGameConnection, ptPrimary, ptPrimary->uHeroId, tSGameGlobals->uszCreatorAccountName, tSGameGlobals->uszCreatorHeroName
        );
        for (auto *j = PlayerGetFirstAll(); j != nullptr; j = PlayerGetNextAll(j)) {
            PRINT_EXPR("PlayerGet: %p->%lx | %i", j, j->uHeroId, SPlayerIsLocal(j))
            if (SPlayerIsLocal(j) != 0)
                ret = j;
        }
        _SERVERCODE_OFF
        return ret;
    }

    inline auto PlayerHeroMatchesLocal(Player *ptPlayer) -> bool {
        if (Player const *ptLocal = LocalPlayerGet(); (ptLocal != nullptr) && (ptPlayer != nullptr))
            return ptLocal->uHeroId == ptPlayer->uHeroId;
        return false;
    }

    inline auto IsACDIDHost(ACDID idPlayerACD) -> bool {  // need ANNtoACD() to check LocalPlayer
        if (Player *ptPlayer = PlayerGetByACD(idPlayerACD); ptPlayer)
            return PlayerHeroMatchesLocal(ptPlayer) || (SPlayerIsLocal(ptPlayer) != 0);
        if (Player const *ptHost = SPlayerGetHost(); ptHost)
            return idPlayerACD != ACDID_INVALID && ptHost->idPlayerACD == idPlayerACD;
        return false;
    }

    inline auto MakeAttribKey(Attrib eAttrib, int32 nParam = -1) -> FastAttribKey {
        return FastAttribKey {(eAttrib | (nParam << 12))};
    }

    inline auto KeyGetFullAttrib(FastAttribKey tKey) -> int32 {
        return tKey.nValue ^ (tKey.nValue >> 12);  // ^ 0xFFFFF000; }
    }

    inline auto KeyGetAttrib(FastAttribKey tKey) -> int32 {
        return tKey.nValue & 0xFFF;
    }

    inline auto KeyGetParam(FastAttribKey tKey) -> int64 {
        return tKey.nValue >> 12;
    }

    inline auto GbidStringAll(GBID gbid) -> LPCSTR {
        return GBIDToStringAll(&gbid).str();
    }

    inline auto GbidCrefStringAll(GBID gbid) -> CRefString {
        return GBIDToStringAll(&gbid);
    }

    inline auto GbidString(GBID gbid, GameBalanceType eGBType = GB_ITEMS, const BOOL fUsedForXMLFields = 0) -> LPCSTR {
        CRefString const result(GBIDToString(gbid, eGBType, fUsedForXMLFields));
        return result.str();
    }

    inline void IntAttribSet(ActorCommonData *tACD, int32 eAttribute, int32 nParam = -1, int32 nIntValue = 1) {
        ACD_AttributesSetInt(tACD, MakeAttribKey(eAttribute, nParam), nIntValue);
    }

    inline auto ParamToStr(int attr, uint param) -> LPCSTR {
        return AttribParamToString(attr, param).str();  // NOLINT
    }

    inline auto AttribToStr(FastAttribKey tKey) -> LPCSTR {
        return FastAttribToString(tKey).str();
    }

    inline void AttribStringInfo(FastAttribKey tKey, FastAttribValue tValue) {
        auto eKeyAttrib = KeyGetAttrib(tKey);
        ++g_mapCheckedAttribs[eKeyAttrib];
        auto attrDef = g_arAttribDefs[eKeyAttrib];
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

    inline void CheckStringList(int gid, LPCSTR szLabel, bool bSkipDef = false) {
        return;  // NOLINT
        for (const auto &idx : kAllStringListSnoEnum) {
            if (bSkipDef && (idx == 52006 || idx == 52008 || idx == 87075))
                continue;
            const auto *fetch = StringListFetchLPCSTR(idx, szLabel);
            if (fetch != nullptr) {
                char const *pch = strstr(fetch, "!!Missing!!");
                if (pch == nullptr) {
                    PRINT("\n%i\t| snoindex[%i]\t(%s)\t=\t%s", gid, idx, szLabel, fetch)
                    // PRINT_EXPR("[0x%x | %s] %i = %s", i, g_tGBTables[i].name, tGBID, gbidString)
                    // PRINT_EXPR("%s\t=\tsnoindex[%i] = %s", gbidString, idx, fetch)
                }
            }
        }
    }

    inline auto ByteToChar(uchar b) -> char {
        b &= 0xF;
        if (0 <= b && b <= 0x9)
            return '0' + b;
        return 'A' + (b - 0xA);
    };

    inline auto WriteTestFile(const std::string &szPath, void *ptBuf, size_type dwSize, bool bSuccess = false) -> bool {
        FileReference tFileRef;
        FileReferenceInit(&tFileRef, szPath.c_str());
        FileCreate(&tFileRef);
        if (FileOpen(&tFileRef, 2u) != 0) {
            if (FileWrite(&tFileRef, ptBuf, -1, dwSize, ERROR_FILE_WRITE) != 0)
                bSuccess = true;
            FileClose(&tFileRef);
        }
        return bSuccess;
    }

    inline auto ReadFileToBuffer(const std::string &szPath, u32 *dwSize, FileReference tFileRef = {}) -> char * {
        if (FileReferenceInit(&tFileRef, szPath.c_str()); FileExists(&tFileRef) != 0)
            if (char *lpBuf = reinterpret_cast<char *>(FileReadIntoMemory(&tFileRef, dwSize, ERROR_FILE_READ)); lpBuf)
                return lpBuf;
        return nullptr;
    }

    inline void ReplaceBlzString(blz::string &dst, const char *data, size_t len) {
        if (dst.m_elements) {
            const char *storage_start = &dst.m_storage[0];
            const char *storage_end   = storage_start + sizeof(dst.m_storage);
            if (dst.m_elements < storage_start || dst.m_elements >= storage_end)
                SigmaMemoryFree(dst.m_elements, nullptr);
        }

        auto *buf = static_cast<char *>(SigmaMemoryNew(len + 1, 0, nullptr, 1));
        if (buf == nullptr) {
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

    inline auto BlizzStringFromFile(LPCSTR szFilenameSD, u32 dwSize = 0) -> blz::string {
        if (char *pFileBuffer = ReadFileToBuffer(szFilenameSD, &dwSize); pFileBuffer) {
            blz::string sReturnString;
            ReplaceBlzString(sReturnString, pFileBuffer, dwSize);
            SigmaMemoryFree(&pFileBuffer, nullptr);
            return sReturnString;
        }
        return blz::string {};
    }

    inline auto PopulateChallengeRiftData(D3::ChallengeRifts::ChallengeData &ptChalConf, D3::Leaderboard::WeeklyChallengeData &ptChalData) -> bool {
        auto PopulateData = [](google::protobuf::MessageLite *dest, const std::string &szPath, u32 dwSize = 0) -> bool {
            PRINT_EXPR("%s", szPath.c_str())
            if (char *pFileBuffer = ReadFileToBuffer(szPath, &dwSize); pFileBuffer) {
                blz::string sFileData;
                ReplaceBlzString(sFileData, pFileBuffer, dwSize);
                ParsePartialFromString(dest, &sFileData);
                SigmaMemoryFree(&pFileBuffer, nullptr);
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

        constexpr char szFormat[] = "/rift_data/challengerift_%02d.dat";
        char           lpBuf[sizeof(szFormat) + 16] {};
        snprintf(lpBuf, sizeof(lpBuf), szFormat, nPick);

        const std::string baseDir = g_szBaseDir;

        const auto configOk = PopulateData(&ptChalConf, baseDir + "/rift_data/challengerift_config.dat");
        if (configOk) {
            // PRINT_EXPR("%li", ptChalConf.challenge_end_unix_time_console_);
            // Force the challenge window to be "always active" offline.
            // Use 32-bit safe end time in case the game casts to s32.
            ptChalConf.challenge_number_                   = GameRandRangeInt(0, 900);
            ptChalConf.challenge_start_unix_time_          = 0;
            ptChalConf.challenge_last_broadcast_unix_time_ = 0;
            ptChalConf.challenge_end_unix_time_console_    = 0x7FFFFFFFLL;
            // PRINT_EXPR("post: %li", ptChalConf.challenge_end_unix_time_console_);
        }
        const auto dataOk = PopulateData(&ptChalData, baseDir + lpBuf);
        // PRINT_EXPR("%u", ptChalData.bnet_account_id_);
        PRINT_EXPR("%d | %d", configOk, dataOk)
        return configOk && dataOk;
    }

    [[maybe_unused]] inline void InterceptChallengeRift(void *bind, Console::Online::StorageResult *eRes, D3::ChallengeRifts::ChallengeData &ptChalConf, D3::Leaderboard::WeeklyChallengeData &ptChalData) {
        if (!PopulateChallengeRiftData(ptChalConf, ptChalData))
            return;
        *eRes = Console::Online::STORAGE_SUCCESS;
        sOnGetChallengeRiftData(bind, eRes, *(&ptChalConf), *(&ptChalData));
        PRINT("Called sOnGetChallengeRiftData! Result: %d", *eRes)
    };

}  // namespace d3
