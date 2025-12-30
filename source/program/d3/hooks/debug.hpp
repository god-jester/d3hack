#include "lib/hook/inline.hpp"
#include "lib/hook/replace.hpp"
#include "lib/hook/trampoline.hpp"
#include "../../config.hpp"

namespace d3 {
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
                char *pch = strstr(paramstr, gbString);
                if (pch != NULL) {
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
            if (strstr(lowercheck, "Jester's") != NULL) {
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
            auto ConfigFileData = *reinterpret_cast<std::shared_ptr<blz::basic_string<char, blz::char_traits<char>, blz::allocator<char>>> *>(ctx->X[2]);
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
            PRINT("CONFIG size %lx vs stored %lx", ConfigFileData.get()->m_size, sizeof(c_szConfigSwap))
            PRINT("CONFIG str %s", ConfigFileData.get()->m_elements)
            auto &what = ConfigFileData->m_elements;
            what       = const_cast<char *>(c_szConfigSwap);
            // *(ConfigFileData->m_elements) = const_cast<char *>(c_szConfigSwap);
            PRINT("CONFIG POST str %s", ConfigFileData.get()->m_elements)
        }
    };

    HOOK_DEFINE_INLINE(SeasonInline) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT("szLine: %s", (char *)(ctx->X[0]))
            PRINT("format: %s", (char *)(ctx->X[1]))
        }
    };

    HOOK_DEFINE_INLINE(SeasonFile) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            // PRINT("SEASONFILE %s", "ENTER")
            // auto SeasonsFileData = *reinterpret_cast<std::shared_ptr<blz::basic_string<char, blz::char_traits<char>, blz::allocator<char>>> *>(ctx->X[2]);
            auto SeasonsFileData = *reinterpret_cast<std::shared_ptr<std::string> *>(ctx->X[2]);
            PRINT("SEASONFILE size %lx vs stored %lx", SeasonsFileData.get()->size(), sizeof(c_szSeasonSwap))
            if (SeasonsFileData.get()->size() > 0xF1) {
                PRINT_EXPR("%s", "SKIPPING SEASON")
                *SeasonsFileData = "";
            }
            // PRINT("SEASONFILE size %lx vs stored %lx", SeasonsFileData.get()->m_size, sizeof(c_szSeasonSwap))
            // PRINT("SEASONFILE str %s", SeasonsFileData.get()->m_elements)
            /* 
                # Format for dates MUST be: " ? ? ? , DD MMM YYYY hh : mm:ss UTC"
                # The Day of Month(DD) MUST be 2 - digit; use either preceding zero or trailing space
                [Season 29]
                Start "Sat, 16 Sep 2023 00:00:00 GMT"
                End "Mon, 08 Jan 2024 01:00:00 GMT"
            */
            // auto &what = SeasonsFileData->m_elements;
            // what       = const_cast<char *>(c_szSeasonSwap);

            std::string testokay(c_szSeasonSwap);
            *SeasonsFileData = testokay;
            // PRINT("SEASONFILE POST str %s", SeasonsFileData.get()->m_elements)
        }
    };

    HOOK_DEFINE_INLINE(PreferencesFile) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto PreferencesFileData = *reinterpret_cast<std::shared_ptr<blz::basic_string<char, blz::char_traits<char>, blz::allocator<char>>> *>(ctx->X[2]);
            PRINT("Preferences str %s", PreferencesFileData.get()->m_elements)
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

    HOOK_DEFINE_INLINE(BlacklistFile) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto BlacklistFileData = *reinterpret_cast<std::shared_ptr<blz::basic_string<char, blz::char_traits<char>, blz::allocator<char>>> *>(ctx->X[2]);
            PRINT("BLACKLIST str %s", BlacklistFileData.get()->m_elements)
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
            auto PublisherFileData = *reinterpret_cast<blz::basic_string<char, blz::char_traits<char>, blz::allocator<char>> *>(ctx->X[21]);  // X8 @ 0x62444
            PRINT("PublisherFileData blz::basic_string\n %s", PublisherFileData.m_elements)
            auto PublisherFileDataStr = *reinterpret_cast<std::string *>(ctx->X[21]);
            PRINT("PublisherFileData std::string (size: %ld)\n %s", PublisherFileDataStr.size(), PublisherFileDataStr.c_str())
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
            PRINT("BDFileDataMem (size: %d)\n %s", fdata->m_fileSize, (char *)fdata->m_fileData)
            auto        length = fdata->m_fileSize;
            auto        bytes  = (char *)fdata->m_fileData;
            std::string final_buf("FILE CONTENT: ");
            for (auto i = 0; i < 100; i++) {
                uchar low  = (bytes[i] >> 0) & 0xF;
                uchar high = (bytes[i] >> 4) & 0xF;

                char buf[3];
                snprintf(buf, sizeof(buf), "%c%c", ByteToChar(high), ByteToChar(low));
                /* Print buf. */
                final_buf += buf;
                final_buf += " ";
            };
            TraceInternal_Log(SLVL_INFO, 3u, OUTPUTSTREAM_DEFAULT, ": %s\n", final_buf.c_str());

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
            auto bytes             = PublisherFileData->m_elements;
            auto usize             = static_cast<uint32>(PublisherFileData->m_size);
            auto m_size            = LODWORD(usize);

            PRINT("PublisherFileDataHex blz::basic_string (filesize: %d | stringsize: %d)", length, m_size)
            std::string final_buf("PUBFILE CONTENT: 0x");
            std::string final_plainbuf("PUBFILE CONTENT: ");
            for (auto i = 0; i < 200; i++) {
                uchar low  = (bytes[i] >> 0) & 0xF;
                uchar high = (bytes[i] >> 4) & 0xF;
                // char  buf[3];
                // snprintf(buf, sizeof(buf), "%c%c", ByteToChar(high), ByteToChar(low));
                char *pBuf = static_cast<char *>(alloca(3));
                SNPRINT(pBuf, "%c%c", ByteToChar(high), ByteToChar(low))
                /* Print buf. */
                final_buf += pBuf;
                final_buf += ", 0x";
                final_plainbuf += pBuf;
                final_plainbuf += " ";
            }

            std::string szDumpPath = g_szBaseDir + "/dumps/dmp_" + std::to_string(length) + ".dat";  //std::format("{}\\_{}.dat", g_szBaseDir, length);

            PRINT_EXPR("Trying to write to: %s", szDumpPath.c_str())
            if (WriteTestFile(szDumpPath, bytes, length))
                PRINT_EXPR("Wrote to: %s !", szDumpPath.c_str())

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
        // ConfigFile::
        //     InstallAtOffset(0x641F0);
        // SeasonFile::
        //     InstallAtOffset(0x65270);

        // CountCosmetics::InstallAtOffset(0x4B104C);
        // BlacklistFile::
        //     InstallAtOffset(0x657F0);
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
