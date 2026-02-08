#include "d3/util.hpp"

#include "lib/util/stack_trace.hpp"
#include "lib/util/strings.hpp"
#include "lib/util/sys/modules.hpp"
#include "program/config.hpp"
#include "d3/setting.hpp"
#include "d3/types/sno.hpp"

#include <nn/fs.hpp>

#include <cstdio>
#include <cstring>

#define HASHMAP(m, id) (((m) & (16777619 * ((16777619 * ((16777619 * ((16777619 * _BYTE((id) ^ 0x811C9DC5)) ^ BYTE1(id))) ^ BYTE2(id))) ^ HIBYTE(id)))))

namespace d3 {

    void PrintAddressWithModule(const char *label, uintptr_t address) {
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

    void DumpStackTrace(const char *tag, uintptr_t fp, size_t maxDepth) {
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

    auto GetItemPtr(GBID gbidItem) -> const Item * {
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

    void AllGBIDsOfType(GameBalanceType eType, std::vector<GBID> &ids) {
        GBHandleList listResults = {};
        memset(static_cast<void *>(&listResults), 0, 20);
        listResults.m_list.m_ptNodeAllocator = reinterpret_cast<XListMemoryPool<GBHandle> *>(GBGetHandlePool());
        listResults.m_ConstructorCalled      = 1;
        GBEnumerate(eType, &listResults);
        ids.clear();
        if (size_t nCount = listResults.m_list.m_nCount; nCount > 0) {
            ids.reserve(nCount);
            auto p_gbH = listResults.m_list.m_pHead;
            while (p_gbH && p_gbH->m_tData.eType == eType) {
                ids.push_back(p_gbH->m_tData.gbid);
                p_gbH = p_gbH->m_pNext;
            }
        }
    }

    auto SPlayerIsPrimary(Player *ptPlayer) -> bool {
        return ptPlayer && *FollowPtr<bool, 0xDB78>(ptPlayer);  // bIsPrimaryPlayerServerOnly
    }

    auto SPlayerGetHost() -> Player * {
        if (GameIsStartUp() != 0)
            return nullptr;

        Player *ret = PlayerGetFirstAll();
        for (auto *j = ret; j != nullptr; j = PlayerGetNextAll(j))
            if (SPlayerIsLocal(j) != 0)
                ret = j;
        return ret;
    }

    auto SPlayerGetHostOld() -> Player * {
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

    auto PlayerHeroMatchesLocal(Player *ptPlayer) -> bool {
        if (Player const *ptLocal = LocalPlayerGet(); (ptLocal != nullptr) && (ptPlayer != nullptr))
            return ptLocal->uHeroId == ptPlayer->uHeroId;
        return false;
    }

    auto IsACDIDHost(ACDID idPlayerACD) -> bool {
        if (Player *ptPlayer = PlayerGetByACD(idPlayerACD); ptPlayer)
            return PlayerHeroMatchesLocal(ptPlayer) || (SPlayerIsLocal(ptPlayer) != 0);
        if (Player const *ptHost = SPlayerGetHost(); ptHost)
            return idPlayerACD != ACDID_INVALID && ptHost->idPlayerACD == idPlayerACD;
        return false;
    }

    auto MakeAttribKey(Attrib eAttrib, int32 nParam) -> FastAttribKey {
        return FastAttribKey {(eAttrib | (nParam << 12))};
    }

    auto KeyGetFullAttrib(FastAttribKey tKey) -> int32 {
        return tKey.nValue ^ (tKey.nValue >> 12);
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

    auto GbidString(GBID gbid, GameBalanceType eGBType, const BOOL fUsedForXMLFields) -> LPCSTR {
        CRefString const result(GBIDToString(gbid, eGBType, fUsedForXMLFields));
        return result.str();
    }

    void IntAttribSet(ActorCommonData *tACD, int32 eAttribute, int32 nParam, int32 nIntValue) {
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
        auto attrDef = g_arAttribDefs[eKeyAttrib];
        if (g_mapCheckedAttribs[eKeyAttrib] <= 9) {
            auto eAttrib = eKeyAttrib;
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
            auto        eParam      = KeyGetParam(tKey);
            const auto *szAttrib    = AttribToStr(tKey);
            const auto *szParam     = ParamToStr(eKeyAttrib, eParam);
            auto        eStrToParam = AttribStringToParam(eKeyAttrib, szAttrib);

            PRINT(
                "%s eAttrib = 0x%x (%i) | eKeyAttrib = 0x%x (%i) | Int = %i | nValue = %i\n\teParam = %li szParam = %s eStrToParam = %i\n---",
                szAttrib, eAttrib, eAttrib, eKeyAttrib, eKeyAttrib, tValue._anon_0.nValue, tKey.nValue, eParam, szParam, eStrToParam
            );
        }
    }

    void CheckStringList(int gid, LPCSTR szLabel, bool bSkipDef) {
        return;  // NOLINT
        for (const auto &idx : kAllStringListSnoEnum) {
            if (bSkipDef && (idx == 52006 || idx == 52008 || idx == 87075))
                continue;
            const auto *fetch = StringListFetchLPCSTR(idx, szLabel);
            if (fetch != nullptr) {
                char const *pch = strstr(fetch, "!!Missing!!");
                if (pch == nullptr) {
                    PRINT("\n%i\t| snoindex[%i]\t(%s)\t=\t%s", gid, idx, szLabel, fetch)
                }
            }
        }
    }

    auto ByteToChar(uchar b) -> char {
        b &= 0xF;
        if (0 <= b && b <= 0x9)
            return '0' + b;
        return 'A' + (b - 0xA);
    }

    auto GetFilenameTail(const char *path) -> const char * {
        if (path == nullptr) {
            return nullptr;
        }
        const char *tail = path;
        for (const char *it = path; *it != '\0'; ++it) {
            if (*it == '/' || *it == '\\') {
                tail = it + 1;
            }
        }
        return tail;
    }

    bool BuildPubfileCachePath(char *out, size_t out_size, const char *filename) {
        if (out == nullptr || out_size == 0) {
            return false;
        }
        out[0] = '\0';
        if (filename == nullptr || filename[0] == '\0') {
            return false;
        }
        const char *tail = GetFilenameTail(filename);
        if (tail == nullptr || tail[0] == '\0') {
            return false;
        }
        const int written = snprintf(out, out_size, "%s/%s", g_szPubfileCacheDir, tail);
        if (written <= 0 || static_cast<size_t>(written) >= out_size) {
            out[0] = '\0';
            return false;
        }
        return true;
    }

    void EnsurePubfileCacheDir() {
        (void)nn::fs::CreateDirectory("sd:/config");
        (void)nn::fs::CreateDirectory(g_szBaseDir);
        (void)nn::fs::CreateDirectory(g_szPubfileCacheDir);
    }

    auto WriteTestFile(const std::string &szPath, void *ptBuf, size_type dwSize, bool bSuccess) -> bool {
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

    auto ReadFileToBuffer(const std::string &szPath, u32 *dwSize, FileReference tFileRef) -> char * {
        if (FileReferenceInit(&tFileRef, szPath.c_str()); FileExists(&tFileRef) != 0)
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

    auto BlizzStringFromFile(LPCSTR szFilenameSD, u32 dwSize) -> blz::string {
        if (char *pFileBuffer = ReadFileToBuffer(szFilenameSD, &dwSize); pFileBuffer) {
            blz::string sReturnString;
            ReplaceBlzString(sReturnString, pFileBuffer, dwSize);
            SigmaMemoryFree(&pFileBuffer, nullptr);
            return sReturnString;
        }
        return blz::string {};
    }

    auto PopulateChallengeRiftData(D3::ChallengeRifts::ChallengeData &ptChalConf, D3::Leaderboard::WeeklyChallengeData &ptChalData) -> bool {
        auto PopulateData = [](google::protobuf::MessageLite *dest, const std::string &szPath, u32 dwSize = 0) -> bool {
            if (char *pFileBuffer = ReadFileToBuffer(szPath, &dwSize); pFileBuffer) {
                blz::string sFileData;
                ReplaceBlzString(sFileData, pFileBuffer, dwSize);
                ParsePartialFromString(dest, &sFileData);
                SigmaMemoryFree(&pFileBuffer, nullptr);
                return true;
            }
            return false;
        };

        auto PopulateDataWithFallback = [&](google::protobuf::MessageLite *dest,
                                            const std::string             &cache_path,
                                            const std::string             &local_path,
                                            const char                    *cache_log,
                                            const char                    *local_log,
                                            bool                          &used_cache) -> bool {
            if (PopulateData(dest, cache_path)) {
                used_cache = true;
                PRINT_LINE(cache_log);
                return true;
            }
            if (PopulateData(dest, local_path)) {
                PRINT_LINE(local_log);
                return true;
            }
            PRINT("Missing challenge rift file: %s", local_path.c_str());
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

        const std::string baseDir  = g_szBaseDir;
        const std::string cacheDir = g_szPubfileCacheDir;

        bool       config_from_cache = false;
        const auto configOk          = PopulateDataWithFallback(
            &ptChalConf,
            cacheDir + "/challengerift_config.dat",
            baseDir + "/rift_data/challengerift_config.dat",
            "[pubfiles] challenge rift config cache hit",
            "[pubfiles] challenge rift config fallback to rift_data",
            config_from_cache
        );
        if (configOk) {
            if (!config_from_cache) {
                ptChalConf.challenge_number_ = GameRandRangeInt(0, 900);
            }
            ptChalConf.challenge_start_unix_time_          = 0;
            ptChalConf.challenge_last_broadcast_unix_time_ = 0;
            ptChalConf.challenge_end_unix_time_console_    = 0x7FFFFFFFLL;
        }

        char               cache_name[64] {};
        const unsigned int cache_pick = config_from_cache
                                            ? static_cast<unsigned int>(ptChalConf.challenge_number_)
                                            : static_cast<unsigned int>(nPick);
        snprintf(cache_name, sizeof(cache_name), "challengerift_%02u.dat", cache_pick);

        char local_name[64] {};
        snprintf(local_name, sizeof(local_name), "challengerift_%02u.dat", static_cast<unsigned int>(nPick));

        bool       data_from_cache = false;
        const auto dataOk          = PopulateDataWithFallback(
            &ptChalData,
            cacheDir + "/" + cache_name,
            baseDir + "/rift_data/" + local_name,
            "[pubfiles] challenge rift data cache hit",
            "[pubfiles] challenge rift data fallback to rift_data",
            data_from_cache
        );
        PRINT_EXPR("%d | %d", configOk, dataOk)
        return configOk && dataOk;
    }

    [[maybe_unused]] void InterceptChallengeRift(void *bind, Console::Online::StorageResult *eRes, D3::ChallengeRifts::ChallengeData &ptChalConf, D3::Leaderboard::WeeklyChallengeData &ptChalData) {
        if (!PopulateChallengeRiftData(ptChalConf, ptChalData))
            return;
        *eRes = Console::Online::STORAGE_SUCCESS;
        sOnGetChallengeRiftData(bind, eRes, *(&ptChalConf), *(&ptChalData));
        PRINT("Called sOnGetChallengeRiftData! Result: %d", *eRes)
    }

}  // namespace d3
