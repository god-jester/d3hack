#pragma once

#include "d3/setting.hpp"
#include "d3/util_globals.hpp"
#include "d3/util_paths.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace d3 {

    void PrintAddressWithModule(const char *label, uintptr_t address);
    void DumpStackTrace(const char *tag, uintptr_t fp, size_t maxDepth = 16);

    auto GetItemPtr(GBID gbidItem) -> const Item *;
    void AllGBIDsOfType(GameBalanceType eType, std::vector<GBID> &ids);

    auto SPlayerIsPrimary(Player *ptPlayer) -> bool;
    auto SPlayerGetHost() -> Player *;
    auto SPlayerGetHostOld() -> Player *;
    auto PlayerHeroMatchesLocal(Player *ptPlayer) -> bool;
    auto IsACDIDHost(ACDID idPlayerACD) -> bool;

    auto MakeAttribKey(Attrib eAttrib, int32 nParam = -1) -> FastAttribKey;
    auto KeyGetFullAttrib(FastAttribKey tKey) -> int32;
    auto KeyGetAttrib(FastAttribKey tKey) -> int32;
    auto KeyGetParam(FastAttribKey tKey) -> int64;

    auto GbidStringAll(GBID gbid) -> LPCSTR;
    auto GbidCrefStringAll(GBID gbid) -> CRefString;
    auto GbidString(GBID gbid, GameBalanceType eGBType = GB_ITEMS, const BOOL fUsedForXMLFields = 0) -> LPCSTR;

    void IntAttribSet(ActorCommonData *tACD, int32 eAttribute, int32 nParam = -1, int32 nIntValue = 1);
    auto ParamToStr(int attr, uint param) -> LPCSTR;
    auto AttribToStr(FastAttribKey tKey) -> LPCSTR;
    void AttribStringInfo(FastAttribKey tKey, FastAttribValue tValue);

    void CheckStringList(int gid, LPCSTR szLabel, bool bSkipDef = false);

    auto ByteToChar(uchar b) -> char;

    auto WriteTestFile(const std::string &szPath, void *ptBuf, size_type dwSize, bool bSuccess = false) -> bool;
    auto ReadFileToBuffer(const std::string &szPath, u32 *dwSize, FileReference tFileRef = {}) -> char *;
    void ReplaceBlzString(blz::string &dst, const char *data, size_t len);
    auto BlizzStringFromFile(LPCSTR szFilenameSD, u32 dwSize = 0) -> blz::string;

    auto                  PopulateChallengeRiftData(D3::ChallengeRifts::ChallengeData &ptChalConf, D3::Leaderboard::WeeklyChallengeData &ptChalData) -> bool;
    [[maybe_unused]] void InterceptChallengeRift(void *bind, Console::Online::StorageResult *eRes, D3::ChallengeRifts::ChallengeData &ptChalConf, D3::Leaderboard::WeeklyChallengeData &ptChalData);

}  // namespace d3
