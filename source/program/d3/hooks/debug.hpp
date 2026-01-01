#include "lib/hook/inline.hpp"
#include "lib/hook/replace.hpp"
#include "lib/hook/trampoline.hpp"
#include "../../config.hpp"
#include <string>
#include <string_view>

namespace d3 {
    namespace {
        auto IsLocalConfigReady() -> bool {
            return global_config.initialized;
        }

        void ClearConfigRequestFlag() {
            auto flag_ptr = reinterpret_cast<uint32_t **>(GameOffset(0x114AD48));
            if (flag_ptr && *flag_ptr)
                **flag_ptr = 0;
        }

        void ClearSeasonsRequestFlag() {
            auto flag_ptr = reinterpret_cast<uint8_t **>(GameOffset(0x114AD50));
            if (flag_ptr && *flag_ptr)
                **flag_ptr = 0;
        }

        void ClearBlacklistRequestFlag() {
            auto flag_ptr = reinterpret_cast<uint8_t **>(GameOffset(0x114AD68));
            if (flag_ptr && *flag_ptr)
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
            if (len && data)
                SigmaMemoryMove(buf, const_cast<char *>(data), len);
            buf[len]                   = '\0';
            dst.m_elements             = buf;
            dst.m_size                 = len;
            dst.m_capacity             = len;
            dst.m_capacity_is_embedded = 0;
        }

        void ReplaceBlzString(blz::string &dst, const char *data) {
            const char *safe_data = data ? data : "";
            ReplaceBlzString(dst, safe_data, std::char_traits<char>::length(safe_data));
        }

        void ReplaceBlzString(blz::string &dst, const blz::string &data) {
            const char *safe_data = data.m_elements ? data.m_elements : "";
            ReplaceBlzString(dst, safe_data, static_cast<size_t>(data.m_size));
        }

        auto BuildSeasonSwapString(u32 season_number) -> blz::string {
            return blz_make_stringf(
                "# Format for dates MUST be: \" ? ? ? , DD MMM YYYY hh : mm:ss UTC\"\n"
                "# The Day of Month(DD) MUST be 2 - digit; use either preceding zero or trailing space\n"
                "[Season %u]\n"
                "Start \"Sat, 09 Feb 2025 00:00:00 GMT\"\n"
                "End \"Tue, 09 Feb 2036 01:00:00 GMT\"\n\n",
                season_number
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
            AppendConfigBool(out, "CommunityBuffEtherealItems", global_config.events.EthrealItems);
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
            auto swap = BuildSeasonSwapString(global_config.seasons.number);
            ReplaceBlzString(*pszFileData->m_pointer, swap);
        }

        void OverrideBlacklistIfNeeded(blz::shared_ptr<blz::string> *pszFileData) {
            if (!pszFileData || !global_config.rare_cheats.drop_anything)
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

    HOOK_DEFINE_INLINE(Store_AttribDefs) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            return;
            // ParagonCapEnabled
            /*
            Attrib eAttrib;
            FastAttribValue tDefaultValue;
            AttributeParameterType eParameterType;
            AttributeOperator eAttributeOperator;
            AttributeDataType eAttributeDataType;
            LPCSTR pszFormulaItem;
            LPCSTR pszFormulaCharacter;
            LPCSTR pszName;
            const AttributeCompressor *ptCompressor;
            uint8 uSyncFlags;
            */
            // PRINT_EXPR("%i", cg_arAttribDefs[1].eAttrib)
            auto                attrDef = cg_arAttribDefs[CURRENCIES_DISCOVERED];
            FastAttribKey const tKey {.nValue = CURRENCIES_DISCOVERED};
            AttribStringInfo(tKey, attrDef.tDefaultValue);
            for (auto i : AllAttributes) {
                attrDef = cg_arAttribDefs[i];
                FastAttribKey tKey {.nValue = i};
                if (attrDef.tDefaultValue._anon_0.nValue > 0)
                    AttribStringInfo(tKey, attrDef.tDefaultValue);
            }
        }
    };

    HOOK_DEFINE_INLINE(Print_SavedAttribs) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            return;
            namespace exl_ptr = exl::util::pointer_path;
            /* FastAttribSetValue(AttributeGroup, tKey, &tValue); */
            auto nSavedAttribId      = static_cast<Attrib>(ctx->W[10]);                    // paAttribsToSave[v13].nSavedAttribId
            auto ptCurAttrib         = *reinterpret_cast<u64 *>(&ctx->X[19]);              // tSavedAttributes->attributes_.elements_[v18];
            auto tOrigKey            = *FollowPtr<s32, 0x18>(ptCurAttrib);
            auto uAttribsToSaveCount = ctx->W[22];                                         // uint32
            auto paAttribsToSave     = *reinterpret_cast<SavedAttribDef **>(&ctx->X[23]);  // const SavedAttribDef *
            // auto idFastAttrib        = ctx->W[0];                                         // AttribGroupID
            auto tKey   = *reinterpret_cast<FastAttribKey *>(ctx->X[1]);    // tKey
            auto tValue = *reinterpret_cast<FastAttribValue *>(ctx->X[2]);  // tValue *

            auto index       = KeyGetAttrib(tKey);
            auto makeorigkey = MakeAttribKey(nSavedAttribId, (tKey.nValue >> 12)).nValue;
            auto attribstr   = AttribToStr(tKey);
            auto paramstr    = ParamToStr(index, (tKey.nValue >> 12));
            auto fullattr    = KeyGetFullAttrib(tKey);
            auto tTemp       = FastAttribKey {.nValue = fullattr};
            auto fullstr     = AttribToStr(tTemp);
            auto attrDef     = cg_arAttribDefs[index];

            for (u32 i = 0; i <= uAttribsToSaveCount; ++i) {
                if (paAttribsToSave[i].eAttrib == index) {
                    PRINT(
                        "\n----- (%s) %s = %i\n\t"
                        "OrigKey: %i (p: %i)| MakeOrigKey: %i (p: %i)\n\t"
                        "SavedAttribDef <0x%x, 0x%x>\n\t"
                        "tKey: %i (p: %i) | 0x%x\n",
                        (uAttribsToSaveCount > 23 ? "HERO" : "ACCOUNT"), attrDef.pszName, tValue._anon_0.nValue,
                        tOrigKey, (tOrigKey >> 12), makeorigkey, (fullattr >> 12),
                        nSavedAttribId, paAttribsToSave[i].eAttrib,
                        tKey.nValue, (tKey.nValue >> 12), tKey.nValue
                    );
                }
            }
            auto fullparamstr = ParamToStr(index, (tTemp.nValue >> 12));
            PRINT(
                "\n\t"
                "-> nSavedAttribId: %i\n\t"
                "KeyGetAttrib(tKey): 0x%x - fullattr: %i (p: %i)\n\t"
                "%i - AttribToStr: \"%s\"\n\t"
                "%i - ParamToStr: \"%s\"\n\t"
                "%i - AttribToStr: \"%s\"\n\t"
                "%i - ParamToStr: \"%s\"",
                nSavedAttribId,
                index, fullattr, (fullattr >> 12),
                tKey.nValue, attribstr,
                (tOrigKey >> 12), paramstr,
                fullattr, fullstr,
                (fullattr >> 12), fullparamstr
            );
            vector<GBID> eBonuses;
            AllGBIDsOfType(GB_PARAGON_BONUSES, &eBonuses);
            for (auto &bonusGbid : eBonuses) {
                // break;
                auto gbString = GbidStringAll(bonusGbid);  //, GB_PARAGON_BONUSES);
                // break;
                if (paramstr && gbString && std::string_view {paramstr}.find(gbString) != std::string_view::npos) {
                    PRINT("paramstr: %s | gbstring: %s (%i)", paramstr, gbString, bonusGbid)
                }

                // v44.nValue = MakeAttribKey(Paragon_Bonus, bonusGbid).nValue;
                // int ParaValue = ACD_AttributesGetInt(g_tHostPlayer.ptACDPlayer, v44);
                // if (ParaValue > 0) { PRINT_EXPR("int <%i> %i = %s", ParaValue, bonusGbid, gbString) }
                // float ParaValueF = ACD_AttributesGetFloat(g_tHostPlayer.ptACDPlayer, v44);
                // if (ParaValueF > 0.0f) { PRINT_EXPR("float <%f> %i = %s", ParaValueF, bonusGbid, gbString) }
            }
            /*  nSavedAttribId.nValue, index:
            -> .nValue: 320
            KeyGetAttrib(nSavedAttribId): 0x140 */

            // return;
            AttribStringInfo(tKey, tValue);
        }
    };

    HOOK_DEFINE_INLINE(Print_ErrorDisplay) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            // DisplayInternalError(LPCSTR pszMessage, LPCSTR szFile, int32 nLine, ErrorCode eErrorCode)
            PRINT_EXPR("\t!!!INTERNAL ERROR!!!\n\tpszMessage: %s szFile: %s nLine: %d", (LPCSTR)ctx->X[0], (LPCSTR)ctx->X[1], (u32)ctx->X[2])
            PRINT_EXPR("eErrorCode: 0x%x | FP: 0x%lx LR: 0x%lx", (u32)ctx->X[3], ctx->X[29], ctx->X[30])
        }
    };

    HOOK_DEFINE_INLINE(Print_Error) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT_EXPR("DisplayErrorMessage X1: %s", (LPCSTR)ctx->X[1])
        }
    };

    HOOK_DEFINE_INLINE(Print_ErrorString) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT_EXPR("DisplayErrorMessage strcopy: %s", (LPCSTR)ctx->X[3])
        }
    };

    HOOK_DEFINE_INLINE(Print_ErrorStringFinal) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT_EXPR("DisplayErrorMessage strfinal: %s", (LPCSTR)ctx->X[0])
        }
    };

    static int coscount = 0;
    HOOK_DEFINE_INLINE(CountCosmetics) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            ++coscount;
            if (!(coscount % 1000))
                PRINT_EXPR("Cosmetics so far: %d", coscount)
        }
    };

    HOOK_DEFINE_INLINE(TraceLogHook) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT_EXPR("DebugLog: %s", (LPSTR)ctx->X[22])
        }
    };

    HOOK_DEFINE_INLINE(OnlineLogHook) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT_EXPR("OnlineWriteLog: %s", (LPCSTR)ctx->X[4])
        }
    };

    HOOK_DEFINE_INLINE(ShowReg) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT("GAMETEXT: %s", (char *)(ctx->X[1]))
            // auto g_ptXVarBroadcast = (XVarString *const)(exl::util::GetMainModuleInfo().m_Total.m_Start + 0x1159808);
            // PRINT_EXPR("%lx", *reinterpret_cast<u64 *>(&g_ptXVarBroadcast))
            namespace exl_ptr = exl::util::pointer_path;
            // D3::Account::ConsoleData::legacy_license_bits_
            for (int i = 0; i < 9; i++) {
                // PRINT_EXPR("X%d: %lx", i, ctx->X[i])
                PRINT("X%d: %lx", i, ctx->X[i])
            }
            PRINT("FP: %lx", ctx->X[29])
            PRINT("LR: %lx", ctx->X[30])
            // auto savedConsoleData = exl_ptr::Follow<D3::Account::ConsoleData *, 0xC0>(ctx->X[0]);
            // auto savedConsoleData = *FollowPtr<D3::Account::ConsoleData *, 0xC0>(ctx->X[0]);
            // auto X8ConsoleData    = reinterpret_cast<D3::Account::ConsoleData *>(ctx->X[8]);
            // PRINT_EXPR("%lx", (u64)savedConsoleData)
            // PRINT_EXPR("%lx", (u64)X8ConsoleData)
            // PRINT_EXPR("%x", savedConsoleData->legacy_license_bits_)
            // PRINT_EXPR("%x", X8ConsoleData->legacy_license_bits_)
            // ++skipfirst;
            // if (!skipfirst)
            //     return;
            // (*savedConsoleData).legacy_license_bits_ = 2130706655;
            // // X8ConsoleData->legacy_license_bits_      = 0;

            // auto postsavedConsoleData = *FollowPtr<D3::Account::ConsoleData *, 0xC0>(ctx->X[0]);
            // auto postX8ConsoleData    = reinterpret_cast<D3::Account::ConsoleData *>(ctx->X[8]);
            // PRINT_EXPR("%x", postsavedConsoleData->legacy_license_bits_)
            // PRINT_EXPR("%x", postX8ConsoleData->legacy_license_bits_)
        }
    };

    HOOK_DEFINE_INLINE(ShowX0) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT("~[0x%lx]X0_TEXT: %s", ctx->X[30], (char *)(ctx->X[0]))
        }
    };

    HOOK_DEFINE_INLINE(ShowString) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto                  xx9        = *reinterpret_cast<StringTableEntry *>(ctx->X[9]);
            [[maybe_unused]] auto xx19       = *reinterpret_cast<StringListDefinition *>(ctx->X[19]);
            [[maybe_unused]] auto ww25       = *reinterpret_cast<StringLabelHandle *>(&ctx->W[25]);
            [[maybe_unused]] auto lowercheck = xx9.szText._anon_0.m_pDebugPtr;
            [[maybe_unused]] auto charptr    = (xx19.arStrings._anon_0.m_uPtr);
            // char *pch = strstr(fetch, "!!Missing!!");
            // if (pch == NULL) {

            // SNOFileEnumerateSNOsForGroup(31, &listSNOFiles);
            //  if ( GameCommonGameFlagIsSet("RetroAnniversaryEvent_Dungeon")
            // GameFlagSet("TotallyNotACowLevel_NotSpawned", 1);
            // v84 = FlagSetLookup(v83, "DoubleRiftKeystones");

            // if (strstr(strlwr(&lowercheck), "jester's") != NULL) {
            if (lowercheck && std::string_view {lowercheck}.find("Jester's") != std::string_view::npos) {
                auto szSNOTEXT = SNOToString(42, xx19.tHeader.snoHandle, 0);
                PRINT_EXPR("%i %s : %s", xx19.tHeader.snoHandle, szSNOTEXT.str(), (xx9.szText._anon_0.m_pDebugPtr))
                PRINT_EXPR("%x - %s", xx9.hLabel, *(char **)(xx19.arStrings._anon_0.m_uPtr))

                unsigned char *stringcontents;
                unsigned int   stringsize = 0L;
                // ConsoleUI:AutosaveWarningScreenText_Switc
                return;
#define DUMPFILE "ItemInstructions.stl"
                sReadFile("StringList\\" DUMPFILE, &stringcontents, &stringsize);
                // PRINT_EXPR("%x%x%x%x%x%x%x%x", *stringcontents, *(stringcontents + 1), *(stringcontents + 2), *(stringcontents + 3), *(stringcontents + 4), *(stringcontents + 5), *(stringcontents + 6), *(stringcontents + 7))
                FileReference tFileRef;
                FileReferenceInit(&tFileRef, "sd:\\" DUMPFILE);
                PRINT_EXPR("0x%x", FileOpen(&tFileRef, 2u))
                FileWrite(&tFileRef, stringcontents, -1, stringsize, ERROR_FILE_WRITE);
                EXL_ABORT("Debug abort: ShowString");
            }
        }
    };

    HOOK_DEFINE_INLINE(FontSnoop) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto tParams = reinterpret_cast<TextCreationParams *>(ctx->X[1]);
            if (tParams->snoFont == 72347 /* SCRIPT*/) {  // 72350 = SANSSERIF ?
                PRINT_EXPR("%i %i", tParams->nFontSize, tParams->snoFont)
                tParams->eResizeToFit = 1;
                PRINT_EXPR("%i", tParams->eResizeToFit)
                PRINT_EXPR("%i %i", tParams->eResizeToFit, tParams->fShrinkToFit)
                PRINT_EXPR("%f", tParams->rectTextPaddingUIC.top)
                PRINT_EXPR("%f", tParams->rectTextPaddingUIC.bottom)
                PRINT_EXPR("%f", tParams->rectTextPaddingUIC.left)
                PRINT_EXPR("%f", tParams->rectTextPaddingUIC.right)
                PRINT_EXPR("%f", tParams->vecTextInsetUIC.x)
                PRINT_EXPR("%f", tParams->vecTextInsetUIC.y)
            }
        }
    };

    HOOK_DEFINE_TRAMPOLINE(ParseFile) {
        // BFD8A0 @ bdRemoteTaskRef __fastcall bdStorage::getPublisherFile(bdStorage *this, const bdNChar8_0 *const fileName, bdFileData *const fileData)
        static auto Callback(bdStorage *_this, const bdNChar8 *const fileName, bdFileData *const fileData) -> void {
            // auto length = fileData->m_fileSize;
            // auto bytes  = (char *)fileData->m_fileData;
            // PRINT_EXPR("getPub_0 size: %d | first bytes: %0x %0xx %0x %0x", length, bytes[0], bytes[1], bytes[2], bytes[3])

            // [[maybe_unused]] auto ret = Orig(_this, fileName, fileData);
            Orig(_this, fileName, fileData);
            PRINT_EXPR("%s", fileName)
            // auto self = reinterpret_cast<bdStorage *>(_this);
            // PRINT_EXPR("%s", self->m_context)
            // PRINT_EXPR("%p", self)
            // PRINT_EXPR("%p", self->m_remoteTaskManager)

            // auto length_1 = fileData->m_fileSize;
            // auto bytes_1  = (char *)fileData->m_fileData;
            // PRINT_EXPR("getPub_1 size: %d | first bytes: %2x %2x %2x %2x", length_1, bytes_1[0], bytes_1[1], bytes_1[2], bytes_1[3])
        }
    };

    HOOK_DEFINE_INLINE(CurlData) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT("CURL %s", "ENTER")
            if (!(char *)(ctx->X[21]))
                return;
            PRINT("_curl: %s", (char *)(ctx->X[21]))
        }
    };

    HOOK_DEFINE_INLINE(ConfigFile) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto ConfigFileData = reinterpret_cast<blz::shared_ptr<blz::string> *>(ctx->X[2]);
            if (!ConfigFileData || !ConfigFileData->m_pointer)
                return;
            /*
                HeroPublishFrequencyMinutes "30"
                CommunityBuffStart "???, 16 Sep 2023 00:00:00 GMT"
                CommunityBuffEnd "???, 01 Dec 2025 01:00:00 GMT"
                CommunityBuffDoubleGoblins "0"
                CommunityBuffDoubleBountyBags "0"
                CommunityBuffRoyalGrandeur "0"
                CommunityBuffLegacyOfNightmares "0"
                CommunityBuffTriunesWill "0"
                CommunityBuffPandemonium "0"
                CommunityBuffKanaiPowers "0"
                CommunityBuffTrialsOfTempests "0"
                CommunityBuffSeasonOnly "1"
                CommunityBuffShadowClones "0"
                CommunityBuffFourthKanaisCubeSlot "0"
                CommunityBuffEtherealItems "0"
                CommunityBuffSoulShards "0"
                CommunityBuffSwarmRifts "0"
                CommunityBuffSanctifiedItems "0"
                CommunityBuffDarkAlchemy "0"
                CommunityBuffParagonCap "1"
                CommunityBuffNestingPortals "1"
                UpdateVersion "1"            
            */
            PRINT("CONFIG size %lx vs stored %lx", ConfigFileData->m_pointer->m_size, sizeof(c_szConfigSwap))
            PRINT("CONFIG str %s", ConfigFileData->m_pointer->m_elements)
            ReplaceBlzString(*ConfigFileData->m_pointer, c_szConfigSwap);
            PRINT("CONFIG POST str %s", ConfigFileData->m_pointer->m_elements)
        }
    };

    HOOK_DEFINE_INLINE(SeasonInline) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT("szLine: %s", (char *)(ctx->X[0]))
            PRINT("format: %s", (char *)(ctx->X[1]))
        }
    };

    HOOK_DEFINE_TRAMPOLINE(ConfigFileRetrieved) {
        static void Callback(void *self, int32 eResult, blz::shared_ptr<blz::string> *pszFileData) {
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
        static void Callback(void *self, int32 eResult, blz::shared_ptr<blz::string> *pszFileData) {
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

    HOOK_DEFINE_INLINE(PreferencesFile) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto PreferencesFileData = reinterpret_cast<blz::shared_ptr<blz::string> *>(ctx->X[2]);
            if (!PreferencesFileData || !PreferencesFileData->m_pointer)
                return;
            PRINT("Preferences str %s", PreferencesFileData->m_pointer->m_elements)
            /*
                PreferencesVersion "46"
                PlayedCutscene0 "15"
                PlayedCutscene1 "0"
                PlayedCutscene2 "0"
                PlayedCutscene3 "138"
                PlayedCutscene4 "131"
                FirstTimeFlowCompleted "1"
                WhatsNewSeen "2.600000"
                AnniversarySeen "0"
                LeaderboardSeenSeason "29"
                SeasonTutorialsSeen "28"
                EulaAcceptedNX64 "0"
                WhatsNewSeenNX64 "1"
                DisplayModeFlags "10"
                DisplayModeWindowMode "0"
                DisplayModeWinLeft "0"
                DisplayModeWinTop "0"
                DisplayModeWinWidth "1024"
                DisplayModeWinHeight "768"
                DisplayModeUIOptWidth "1024"
                DisplayModeUIOptHeight "768"
                DisplayModeWidth "1024"
                DisplayModeHeight "768"
                DisplayModeRefreshRate "60"
                DisplayModeBitDepth "32"
                DisplayModeMSAALevel "0"
                Gamma "1.100000"
                MipOffset "0"
                ShadowQuality "6"
                ShadowDetail "1"
                PhysicsQuality "1"
                ClutterQuality "3"
                Vsync "1"
                LargeCursor "0"
                Letterbox "0"
                Antialiasing "1"
                DisableScreenShake "0"
                DisableChromaEffects "0"
                SSAO "1"
                LockCursorInFullscreenWindowed "0"
                LimitForegroundFPS "1"
                MaxForegroundFPS "150"
                LimitBackgroundFPS "1"
                MaxBackgroundFPS "8"         
            */
        }
    };

    HOOK_DEFINE_TRAMPOLINE(BlacklistFileRetrieved) {
        static void Callback(void *self, int32 eResult, blz::shared_ptr<blz::string> *pszFileData) {
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

    HOOK_DEFINE_REPLACE(BDPublish) {
        // @ void bdLogSubscriber::publish(const bdLogSubscriber *this, const bdLogMessageType type, const bdNChar8 *const, const bdNChar8 *const file, const bdNChar8 *const, const bdUInt line, const bdNChar8 *const msg)
        static void Callback(const void *, const bdLogMessageType type, const bdNChar8 *const, const bdNChar8 *const file, const bdNChar8 *const, const bdUInt line, const bdNChar8 *const msg) {
            PRINT_EXPR("BDPUBLISH HIT%s", "!")
            PRINT_EXPR("TYPE %d", type)
            PRINT_EXPR("LINE %d", line)
            PRINT_EXPR("FILE %s", file)
            PRINT_EXPR("MSG %s", msg)
        }
    };

    HOOK_DEFINE_INLINE(PubFileDataConstructor) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto PublisherFileData = reinterpret_cast<blz::string *>(ctx->X[21]);  // X8 @ 0x62444
            if (!PublisherFileData || !PublisherFileData->m_elements)
                return;
            PRINT("PublisherFileData blz::string (size: %ld)\n %s", PublisherFileData->m_size, PublisherFileData->m_elements)
        }
    };

    HOOK_DEFINE_INLINE(PubFileDataMem) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT("PublisherFileDataMem \n %s", reinterpret_cast<char *>(ctx->X[0]))
        }
    };

    HOOK_DEFINE_INLINE(BDFileDataMem) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto fdata = reinterpret_cast<bdFileData *>(ctx->X[0]);
            if (!fdata || !fdata->m_fileData)
                return;
            PRINT("BDFileDataMem (size: %d)\n %s", fdata->m_fileSize, (char *)fdata->m_fileData)
            auto         length     = static_cast<u32>(fdata->m_fileSize);
            const auto  *bytes      = reinterpret_cast<const u8 *>(fdata->m_fileData);
            const u32    dump_len   = (length < 100u) ? length : 100u;
            const char   kPrefix[]  = "FILE CONTENT: ";
            const size_t kPrefixLen = sizeof(kPrefix) - 1;
            const size_t out_len    = kPrefixLen + (static_cast<size_t>(dump_len) * 3);
            auto        *out        = static_cast<char *>(SigmaMemoryNew(out_len + 1, 0, nullptr, true));
            if (out) {
                SigmaMemoryMove(out, const_cast<char *>(kPrefix), kPrefixLen);
                size_t pos = kPrefixLen;
                for (u32 i = 0; i < dump_len; ++i) {
                    const u8 value = bytes[i];
                    out[pos++]     = ByteToChar((value >> 4) & 0xF);
                    out[pos++]     = ByteToChar(value & 0xF);
                    out[pos++]     = ' ';
                }
                out[pos] = '\0';
                TraceInternal_Log(SLVL_INFO, 3u, OUTPUTSTREAM_DEFAULT, ": %s\n", out);
                SigmaMemoryFree(out, nullptr);
            }

            if (length == 31) {
                // auto chaldata = *(ChallengeData *)fdata->m_fileData;
                // PRINT_EXPR("%f", chaldata.challenge_nephalem_orb_multiplier_)
                // PRINT_EXPR("%d", chaldata.challenge_number_)
                auto numchal = *FollowPtr<uint32, 0x28>(fdata->m_fileData);
                PRINT_EXPR("%d", numchal)
            }
        }
    };

    HOOK_DEFINE_INLINE(PubFileDataHex) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto PublisherFileData = reinterpret_cast<blz::string *>(ctx->X[23]);
            auto length            = static_cast<bdUInt>(ctx->W[24]);
            if (!PublisherFileData)
                return;
            auto bytes             = PublisherFileData->m_elements;
            auto usize             = static_cast<uint32>(PublisherFileData->m_size);
            auto m_size            = LODWORD(usize);

            PRINT("PublisherFileDataHex blz::basic_string (filesize: %d | stringsize: %d)", length, m_size)
            auto        dump_path     = blz_make_stringf("%s/dumps/dmp_%u.dat", g_szBaseDir.c_str(), length);
            const char *dump_path_str = dump_path.m_elements ? dump_path.m_elements : "";
            PRINT_EXPR("Trying to write to: %s", dump_path_str)
            if (bytes && WriteTestFile(dump_path_str, bytes, length))
                PRINT_EXPR("Wrote to: %s !", dump_path_str)

            if (length == 3098) {
                D3::Leaderboard::WeeklyChallengeData *wchaldata;
                wchaldata = new D3::Leaderboard::WeeklyChallengeData {}, WeeklyChallengeData_ctor(wchaldata);
                ParsePartialFromString(wchaldata, PublisherFileData);  // 3098
                PRINT_EXPR("%d", (~LOBYTE(wchaldata->_has_bits_[0]) & 7))
            }
        }
    };

    HOOK_DEFINE_TRAMPOLINE(ChallengeRiftCallback) {
        static void Callback(void *bind, StorageResult *pRes, ChallengeData *ptChalConf, WeeklyChallengeData *ptChalData) {
            if (!global_config.challenge_rifts.active) {
                Orig(bind, pRes, ptChalConf, ptChalData);
                return;
            }
            if (!ptChalConf || !ptChalData || !pRes) {
                Orig(bind, pRes, ptChalConf, ptChalData);
                return;
            }
            if (PopulateChallengeRiftData(*ptChalConf, *ptChalData))
                *pRes = Console::Online::STORAGE_SUCCESS;
            Orig(bind, pRes, ptChalConf, ptChalData);
        }
    };

    void SetupDebuggingHooks() {
        if (global_config.challenge_rifts.active) {
            ChallengeRiftCallback::
                InstallAtOffset(0x185F70);
        }

        if (!global_config.defaults_only && global_config.debug.active && global_config.debug.enable_pubfile_dump) {
            PubFileDataHex::
                InstallAtOffset(0x6A2A8);  // Use to dump online files
        }

        if (global_config.debug.active && global_config.debug.enable_error_traces) {
            Print_ErrorDisplay::
                InstallAtOffset(0xA2AC60);
            // Print_Error::
            //     InstallAtOffset(0xA2AC70);
            // Print_ErrorString::
            //     InstallAtOffset(0xA2AE14);
            Print_ErrorStringFinal::
                InstallAtOffset(0xA2AE74);
        }
        // BDPublish::
        //     InstallAtFuncPtr(bdLogSubscriber_publish);
        ConfigFileRetrieved::
            InstallAtOffset(0x641F0);
        SeasonsFileRetrieved::
            InstallAtOffset(0x65270);
        BlacklistFileRetrieved::
            InstallAtOffset(0x657F0);

        // CountCosmetics::
        //     InstallAtOffset(0x4B104C);
        // ParseFile::
        //     InstallAtOffset(0xBFD8A0);
        // CurlData::
        //     InstallAtOffset(0xC592D4);
        // FontSnoop::
        //     InstallAtOffset(0x3A73A8);
        // Store_AttribDefs::
        //     InstallAtOffset(0x69B9E0);
        // Print_SavedAttribs::
        //     InstallAtOffset(0x6B5384);
    }

}  // namespace d3
