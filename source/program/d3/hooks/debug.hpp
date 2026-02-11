#pragma once

#include "program/config.hpp"
#include "program/tagnx.hpp"
#include "program/d3/_util.hpp"
#include "program/d3/types/attributes.hpp"
#include "program/d3/types/d3_account.hpp"
#include "program/d3/types/common.hpp"
#include "program/d3/types/namespaces.hpp"
#include "lib/hook/inline.hpp"
#include "lib/hook/replace.hpp"
#include "lib/hook/trampoline.hpp"
#include "lib/patch/random_access_patcher.hpp"
#include "lib/util/modules.hpp"
#include "lib/util/sys/mem_layout.hpp"
#include "nvn.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace d3 {
    namespace {
        constexpr char k_error_mgr_dump_path[] = "sd:/config/d3hack-nx/error_manager_dump.txt";

        static auto InRange(exl::util::Range range, uintptr_t addr) -> bool {
            return addr >= range.m_Start && addr < range.GetEnd();
        }

        static auto IsProbablyReadablePtr(uintptr_t addr) -> bool {
            if (addr == 0) {
                return false;
            }
            if (exl::util::TryGetModule(addr) != nullptr) {
                return true;
            }
            return InRange(exl::util::mem_layout::s_Heap, addr) ||
                   InRange(exl::util::mem_layout::s_Alias, addr) ||
                   InRange(exl::util::mem_layout::s_Aslr, addr) ||
                   InRange(exl::util::mem_layout::s_Stack, addr);
        }

        static auto ClampReadableLen(uintptr_t addr, size_t len) -> size_t {
            if (!IsProbablyReadablePtr(addr) || len == 0) {
                return 0;
            }

            if (exl::util::TryGetModule(addr) != nullptr) {
                return len;
            }

            const exl::util::Range *const kRanges[] = {
                &exl::util::mem_layout::s_Heap,
                &exl::util::mem_layout::s_Alias,
                &exl::util::mem_layout::s_Aslr,
                &exl::util::mem_layout::s_Stack,
            };

            for (const auto *range : kRanges) {
                if (!InRange(*range, addr)) {
                    continue;
                }
                const uintptr_t end = range->GetEnd();
                if (end <= addr) {
                    return 0;
                }
                const size_t remaining = static_cast<size_t>(end - addr);
                return std::min(len, remaining);
            }

            return len;
        }

        inline void WriteDumpLine(FileReference *tFileRef, const char *fmt, ...) {
            char    buf[512] = {};
            va_list vl;
            va_start(vl, fmt);
            int n = vsnprintf(buf, sizeof(buf), fmt, vl);
            va_end(vl);
            if (n <= 0)
                return;
            u32 size = (n >= static_cast<int>(sizeof(buf))) ? static_cast<u32>(sizeof(buf) - 1) : static_cast<u32>(n);
            FileWrite(tFileRef, buf, -1, size, ERROR_FILE_WRITE);
        }

        inline const char *GetBlzStringPtr(const blz::string &str) {
            if (str.m_elements)
                return str.m_elements;
            return str.m_storage;
        }

        inline void DumpErrorManagerToFile(const char *reason, const char *message, const char *file, u32 line) {
            FileReference tFileRef;
            FileReferenceInit(&tFileRef, k_error_mgr_dump_path);
            FileCreate(&tFileRef);
            if (FileOpen(&tFileRef, 2u) == 0)
                return;

            WriteDumpLine(&tFileRef, "\n--- ErrorManager dump (%s) ---\n", reason ? reason : "unknown");
            WriteDumpLine(&tFileRef, "message=%s\n", message ? message : "<null>");
            WriteDumpLine(&tFileRef, "file=%s line=%u\n", file ? file : "<null>", line);

            if (g_tSigmaGlobals.ptEMGlobals == nullptr) {
                WriteDumpLine(&tFileRef, "ErrorManagerGlobals: <null>\n");
                FileClose(&tFileRef);
                return;
            }

            const auto &em = *g_tSigmaGlobals.ptEMGlobals;
            WriteDumpLine(
                &tFileRef,
                "EM: dump=%d crash_mode=%d crash_num=%d terminate_token=%u\n",
                em.eDumpType,
                em.eErrorManagerBlizzardErrorCrashMode,
                em.nCrashNumber,
                em.dwShouldTerminateToken
            );
            WriteDumpLine(
                &tFileRef,
                "EM: flags handling=%d in_handler=%d suppress_ui=%d suppress_stack=%d suppress_mods=%d suppress_trace=%d\n",
                em.fSigmaCurrentlyHandlingError,
                em.fSigmaCurrentlyInExceptionHandler,
                em.fSuppressUserInteraction,
                em.fSuppressStackCrawls,
                em.fSuppressModuleEnumeration,
                em.fSuppressTracingToScreen
            );
            WriteDumpLine(
                &tFileRef,
                "EM: inspector=%u trace_thresh=%u last_trace=%.3f hwnd=0x%x\n",
                em.dwInspectorProjectID,
                em.dwTraceHesitationThreshhold,
                em.flLastTraceTime,
                em.hwnd
            );
            const size_t    parent_len  = static_cast<size_t>(em.szParentProcess.m_size);
            const char     *parent_ptr  = GetBlzStringPtr(em.szParentProcess);
            const uintptr_t parent_addr = reinterpret_cast<uintptr_t>(parent_ptr);
            WriteDumpLine(
                &tFileRef,
                "EM: app=%s hero=%s parent_len=%zu parent_ptr=0x%llx\n",
                em.szApplicationName,
                em.szHeroFile,
                parent_len,
                static_cast<unsigned long long>(parent_addr)
            );
            if (parent_len != 0 && parent_ptr != nullptr) {
                constexpr size_t kMaxParentDump = 512;
                const size_t     want           = std::min(parent_len, kMaxParentDump);
                const size_t     safe_len       = ClampReadableLen(parent_addr, want);
                if (safe_len != 0) {
                    WriteDumpLine(&tFileRef, "EM: parent=");
                    (void)FileWrite(&tFileRef, parent_ptr, static_cast<u32>(-1), static_cast<u32>(safe_len), ERROR_FILE_WRITE);
                    WriteDumpLine(&tFileRef, "\n");
                } else {
                    WriteDumpLine(&tFileRef, "EM: parent=<unreadable>\n");
                }
            }

            const auto &sys = em.tSystemInfo;
            WriteDumpLine(
                &tFileRef,
                "OS: type=%d 64bit=%d ver=%u.%u build=%u sp=%u mem=%llu\n",
                sys.eOSType,
                sys.fOSIs64Bit,
                sys.uMajorVersion,
                sys.uMinorVersion,
                sys.uBuild,
                sys.uServicePack,
                static_cast<unsigned long long>(sys.uTotalPhysicalMemory)
            );
            WriteDumpLine(&tFileRef, "OS: desc=%s\n", sys.szOSDescription);
            WriteDumpLine(&tFileRef, "OS: lang=%s\n", sys.szOSLanguage);

            const auto &ed = em.tErrorManagerBlizzardErrorData;
            WriteDumpLine(
                &tFileRef,
                "EMData: mode=%d issue=%d severity=%d attachments=%u\n",
                ed.eMode,
                ed.eIssueType,
                ed.eSeverity,
                ed.dwNumAttachments
            );
            WriteDumpLine(&tFileRef, "EMData: app_dir=%s\n", ed.szApplicationDirectory);
            WriteDumpLine(&tFileRef, "EMData: summary=%s\n", ed.szSummary);
            WriteDumpLine(&tFileRef, "EMData: assertion=%s\n", ed.szAssertion);
            WriteDumpLine(&tFileRef, "EMData: modules=%s\n", ed.szModules);
            WriteDumpLine(&tFileRef, "EMData: stack_digest=%s\n", ed.szStackDigest);
            WriteDumpLine(&tFileRef, "EMData: locale=%s\n", ed.szLocale);
            WriteDumpLine(&tFileRef, "EMData: comments=%s\n", ed.szComments);
            for (u32 i = 0; i < 8; ++i) {
                if (i >= ed.dwNumAttachments)
                    break;
                WriteDumpLine(&tFileRef, "EMData: attachment[%u]=%s\n", i, ed.aszAttachments[i]);
            }
            WriteDumpLine(&tFileRef, "EMData: user_assign=%s\n", ed.szUserAssignment);
            WriteDumpLine(&tFileRef, "EMData: email=%s\n", ed.szEmailAddress);
            WriteDumpLine(&tFileRef, "EMData: reopen_cmd=%s\n", ed.szReopenCmdLine);
            WriteDumpLine(&tFileRef, "EMData: repair_cmd=%s\n", ed.szRepairCmdLine);
            WriteDumpLine(&tFileRef, "EMData: report_dir=%s\n", ed.szReportDirectory);
            WriteDumpLine(
                &tFileRef,
                "EMData: metadata ptr=%p size=%zu cap_bits=%llu embedded=%llu\n",
                ed.aMetadata.m_elements,
                static_cast<size_t>(ed.aMetadata.m_size),
                static_cast<unsigned long long>(ed.aMetadata.m_capacity),
                static_cast<unsigned long long>(ed.aMetadata.m_capacity_is_embedded)
            );
            WriteDumpLine(&tFileRef, "EMData: branch=%s\n", ed.szBranch);
            FileClose(&tFileRef);
        }
    }  // namespace

    HOOK_DEFINE_INLINE(Store_AttribDefs) {
        static void Callback(exl::hook::InlineCtx * /*ctx*/) {
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
            // PRINT_EXPR("%i", g_arAttribDefs[1].eAttrib)
            auto                attrDef = g_arAttribDefs[CURRENCIES_DISCOVERED];
            FastAttribKey const tKey {.nValue = CURRENCIES_DISCOVERED};
            AttribStringInfo(tKey, attrDef.tDefaultValue);
            for (auto i : AllAttributes) {
                attrDef = g_arAttribDefs[i];
                FastAttribKey tKey {.nValue = i};
                if (attrDef.tDefaultValue._anon_0.nValue > 0)
                    AttribStringInfo(tKey, attrDef.tDefaultValue);
            }
        }
    };

    HOOK_DEFINE_INLINE(Print_SavedAttribs) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            return;
            /* FastAttribSetValue(AttributeGroup, tKey, &tValue); */
            auto  nSavedAttribId      = static_cast<Attrib>(ctx->W[10]);                    // paAttribsToSave[v13].nSavedAttribId
            auto  ptCurAttrib         = *(&ctx->X[19]);                                     // tSavedAttributes->attributes_.elements_[v18];
            auto  tOrigKey            = *FollowPtr<s32, 0x18>(ptCurAttrib);
            auto  uAttribsToSaveCount = ctx->W[22];                                         // uint32
            auto *paAttribsToSave     = *reinterpret_cast<SavedAttribDef **>(&ctx->X[23]);  // const SavedAttribDef *
            // auto idFastAttrib        = ctx->W[0];                                         // AttribGroupID
            auto tKey   = *reinterpret_cast<FastAttribKey *>(ctx->X[1]);    // tKey
            auto tValue = *reinterpret_cast<FastAttribValue *>(ctx->X[2]);  // tValue *

            auto        index       = KeyGetAttrib(tKey);
            auto        makeorigkey = MakeAttribKey(nSavedAttribId, (tKey.nValue >> 12)).nValue;
            const auto *attribstr   = AttribToStr(tKey);
            const auto *paramstr    = ParamToStr(index, (tKey.nValue >> 12));
            auto        fullattr    = KeyGetFullAttrib(tKey);
            auto        tTemp       = FastAttribKey {.nValue = fullattr};
            const auto *fullstr     = AttribToStr(tTemp);
            auto        attrDef     = g_arAttribDefs[index];

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
            const auto *fullparamstr = ParamToStr(index, (tTemp.nValue >> 12));
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
            std::vector<GBID> eBonuses;
            AllGBIDsOfType(GB_PARAGON_BONUSES, eBonuses);
            for (GBID bonusGbid : eBonuses) {
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
            const uintptr_t pszMessage = ctx->X[0];
            const uintptr_t szFile     = ctx->X[1];
            const u32       nLine      = static_cast<u32>(ctx->X[2]);

            const auto *pszMessageStr = (pszMessage >= 0x8000000u) ? reinterpret_cast<const char *>(pszMessage) : nullptr;
            const auto *szFileStr     = (szFile >= 0x8000000u) ? reinterpret_cast<const char *>(szFile) : nullptr;

            const char *pszMessageOut = (pszMessageStr != nullptr) ? pszMessageStr : "<null>";
            const char *szFileOut     = (szFileStr != nullptr) ? szFileStr : "<invalid>";
            PRINT_EXPR(
                "\t!!!INTERNAL ERROR!!!\n\tpszMessage: %s\n\tszFile: %s\n\tnLine: %u",
                pszMessageOut,
                szFileOut,
                nLine
            )
            PRINT_EXPR(
                "\tpszMessage=0x%lx szFile=0x%lx",
                static_cast<unsigned long>(pszMessage),
                static_cast<unsigned long>(szFile)
            )
            PRINT_EXPR("eErrorCode: 0x%x | FP: 0x%lx LR: 0x%lx", (u32)ctx->X[3], ctx->X[29], ctx->X[30])
            DumpStackTrace("DisplayInternalError", ctx->X[29]);
            // Avoid calling file IO helpers from inside the internal error display path.
            // In some failure modes, ErrorManager state is partially corrupted and dumping
            // can trigger further faults (especially under emulators).
        }
    };

    HOOK_DEFINE_INLINE(Print_Error) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT_EXPR("DisplayErrorMessage X1: %s", (LPCSTR)ctx->X[1])
        }
    };

    HOOK_DEFINE_INLINE(Print_ChallengeRiftFailed) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            PRINT_EXPR("ChallengeRiftFailed X0: %s", (LPCSTR)ctx->X[0])
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
            DumpStackTrace("DisplayErrorMessage", ctx->X[29]);
            // Avoid calling file IO helpers from inside the error display path.
        }
    };

    static auto ReadVarint(const u8 *data, size_t len, size_t &idx, u64 &out) -> bool {
        out       = 0;
        u32 shift = 0;
        while (idx < len && shift < 64) {
            const u8 byte = data[idx++];
            out |= (static_cast<u64>(byte & 0x7Fu) << shift);
            if ((byte & 0x80u) == 0)
                return true;
            shift += 7;
        }
        return false;
    }

    static auto ParseWeeklyChallengeTopLevel(const blz::string *data, u32 &f1, u32 &f2, u32 &f3, u64 &f4) -> bool {
        if (!data || !data->m_elements || data->m_size == 0)
            return false;
        const auto *bytes = reinterpret_cast<const u8 *>(data->m_elements);
        const auto  len   = static_cast<size_t>(data->m_size);
        size_t      idx   = 0;
        bool        has1  = false;
        bool        has2  = false;
        bool        has3  = false;
        bool        has4  = false;
        f1 = f2 = f3 = 0;
        f4           = 0;
        while (idx < len) {
            u64 tag = 0;
            if (!ReadVarint(bytes, len, idx, tag))
                break;
            const u32 field = static_cast<u32>(tag >> 3);
            const u32 wire  = static_cast<u32>(tag & 0x7);
            if (wire == 0) {
                u64 value = 0;
                if (!ReadVarint(bytes, len, idx, value))
                    break;
                if (field == 4) {
                    f4   = value;
                    has4 = true;
                }
            } else if (wire == 2) {
                u64 length = 0;
                if (!ReadVarint(bytes, len, idx, length))
                    break;
                if (idx + length > len)
                    break;
                if (field == 1) {
                    f1   = static_cast<u32>(length);
                    has1 = true;
                } else if (field == 2) {
                    f2   = static_cast<u32>(length);
                    has2 = true;
                } else if (field == 3) {
                    f3   = static_cast<u32>(length);
                    has3 = true;
                }
                idx += static_cast<size_t>(length);
            } else {
                break;
            }
        }
        return has1 && has2 && has3 && has4;
    }

    inline const void *GetMessageVptr(const google::protobuf::MessageLite *msg) {
        if (msg == nullptr) {
            return nullptr;
        }
        return *reinterpret_cast<void *const *>(msg);
    }

    inline const void *GetChallengeDataVptr() {
        static ChallengeData s_chal;
        return *reinterpret_cast<void *const *>(&s_chal);
    }

    inline const void *GetWeeklyChallengeDataVptr() {
        static WeeklyChallengeData s_weekly;
        return *reinterpret_cast<void *const *>(&s_weekly);
    }

    inline bool CacheChallengeRiftRaw(const char *filename, const blz::string *data, const char *log_line) {
        if (data == nullptr || global_config.debug.enable_pubfile_dump == false) {
            return false;
        }
        const char  *bytes = data->m_elements ? data->m_elements : data->m_storage;
        const size_t size  = static_cast<size_t>(data->m_size);
        if (bytes == nullptr || size == 0) {
            return false;
        }
        char cache_path[512] {};
        if (!BuildPubfileCachePath(cache_path, sizeof(cache_path), filename)) {
            return false;
        }
        EnsurePubfileCacheDir();
        if (WriteTestFile(cache_path, const_cast<char *>(bytes), size)) {
            PRINT_LINE(log_line);
            return true;
        }
        return false;
    }

    namespace cr_debug {
        constexpr uintptr_t kChallengeSaveGatePrimaryOff  = 0x4EE18;
        constexpr uintptr_t kChallengeSaveGateFallbackOff = 0x4EF28;
        constexpr uint32    kPrimaryRestoreInsn           = 0x54000EE9u;  // B.ls loc_4EFF4
        constexpr uint32    kFallbackRestoreInsn          = 0x54FFF7A8u;  // B.hi loc_4EE1C
        constexpr uint32    kPrimaryBypassInsn            = 0x14000001u;  // B loc_4EE1C
        constexpr uint32    kFallbackBypassInsn           = 0x17FFFFBDu;  // B loc_4EE1C
        inline bool         g_reward_save_gate_patched    = false;
        inline uint32       g_last_challenge_number       = 0u;

        inline void SetLastChallengeNumber(uint32 challenge_num) {
            if (challenge_num != 0u) {
                g_last_challenge_number = challenge_num;
            }
        }

        inline auto GetCurrentChallengeNumber() -> uint32 {
            if (g_last_challenge_number != 0u) {
                return g_last_challenge_number;
            }
            SGameGlobals *ptGlobals = SGameGlobalsGet();
            if (ptGlobals == nullptr) {
                return 1u;
            }
            constexpr uintptr_t kChallengeNumberOff = 0x2198;  // 2.7.6 OpenChallengeRift LDR W9, [X0,#0x2198]
            const uintptr_t     challenge_addr      = reinterpret_cast<uintptr_t>(ptGlobals) + kChallengeNumberOff;
            const uint32        challenge_num =
                IsProbablyReadablePtr(challenge_addr) ? *reinterpret_cast<uint32 const *>(challenge_addr) : 0u;
            return challenge_num != 0u ? challenge_num : 1u;
        }

        inline void SetRewardSaveGateBypass(bool enabled) {
            if (g_reward_save_gate_patched == enabled) {
                return;
            }

            auto jest = exl::patch::RandomAccessPatcher();
            if (enabled) {
                jest.Patch<uint32>(kChallengeSaveGatePrimaryOff, kPrimaryBypassInsn);
                jest.Patch<uint32>(kChallengeSaveGateFallbackOff, kFallbackBypassInsn);
            } else {
                jest.Patch<uint32>(kChallengeSaveGatePrimaryOff, kPrimaryRestoreInsn);
                jest.Patch<uint32>(kChallengeSaveGateFallbackOff, kFallbackRestoreInsn);
            }

            g_reward_save_gate_patched = enabled;
            PRINT("[cr_reward_save_gate] patched=%d", enabled ? 1 : 0)
        }
    }  // namespace cr_debug

    HOOK_DEFINE_TRAMPOLINE(ParsePartialFromStringHook) {
        static auto Callback(google::protobuf::MessageLite *msg, const blz::string *data) -> bool {
            const u32           size                 = data ? static_cast<u32>(data->m_size) : 0u;
            const bool          ok                   = Orig(msg, data);
            static unsigned int s_last_challenge_num = 0;
            const void         *msg_vptr             = GetMessageVptr(msg);
            if (ok && data && msg_vptr == GetChallengeDataVptr()) {
                const auto *chal     = reinterpret_cast<const ChallengeData *>(msg);
                s_last_challenge_num = static_cast<unsigned int>(chal->challenge_number_);
                cr_debug::SetLastChallengeNumber(static_cast<uint32>(s_last_challenge_num));
                CacheChallengeRiftRaw("challengerift_config.dat", data, "[pubfiles] cached challenge rift config");
            } else if (ok && data && msg_vptr == GetWeeklyChallengeDataVptr()) {
                char data_name[64] {};
                snprintf(data_name, sizeof(data_name), "challengerift_%02u.dat", s_last_challenge_num);
                CacheChallengeRiftRaw(data_name, data, "[pubfiles] cached challenge rift data");
            }
            if (data) {
                if (size >= 3000 && size <= 9000) {
                    u32        f1     = 0;
                    u32        f2     = 0;
                    u32        f3     = 0;
                    u64        f4     = 0;
                    const bool parsed = ParseWeeklyChallengeTopLevel(data, f1, f2, f3, f4);
                    PRINT_EXPR("ParsePartialFromString size=%u ok=%d parsed=%d f1=%u f2=%u f3=%u f4=%llu", size, ok, parsed, f1, f2, f3, static_cast<unsigned long long>(f4))
                } else if (size == 0 && data->m_elements) {
                    PRINT_EXPR("ParsePartialFromString size=0 ok=%d ptr=%p", ok, data->m_elements)
                }
            }
            return ok;
        }
    };

    static int g_nCosCount = 0;
    HOOK_DEFINE_INLINE(CountCosmetics) {
        static void Callback(exl::hook::InlineCtx * /*ctx*/) {
            ++g_nCosCount;
            if ((g_nCosCount % 1000) == 0)
                PRINT_EXPR("Cosmetics so far: %d", g_nCosCount)
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

    HOOK_DEFINE_REPLACE(RejectInvalidModes) {
        // @ 0x298A20: uint32 __fastcall sRejectInvalidModes(DisplayMode *ptDisplayModes, uint32 dwDisplayModeCount)
        static auto Callback([[maybe_unused]] DisplayMode *ptDisplayModes, uint32 dwDisplayModeCount) -> uint32 {
            PRINT("RejectInvalidModes called with dwDisplayModeCount=%u", dwDisplayModeCount)
            return dwDisplayModeCount;
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
            auto *tParams = reinterpret_cast<TextCreationParams *>(ctx->X[1]);
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

    HOOK_DEFINE_INLINE(PreferencesFile) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto PreferencesFileData = reinterpret_cast<blz::shared_ptr<blz::string> *>(ctx->X[2]);
            if ((PreferencesFileData == 0u) || !PreferencesFileData->m_pointer)
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
            if ((PublisherFileData == 0u) || !PublisherFileData->m_elements)
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
            auto *fdata = reinterpret_cast<bdFileData *>(ctx->X[0]);
            if ((fdata == nullptr) || (fdata->m_fileData == nullptr))
                return;
            PRINT("BDFileDataMem (size: %d)\n %s", fdata->m_fileSize, (char *)fdata->m_fileData)
            auto         length     = static_cast<u32>(fdata->m_fileSize);
            const auto  *bytes      = reinterpret_cast<const u8 *>(fdata->m_fileData);
            const u32    dumpLen    = (length < 100u) ? length : 100u;
            const char   kPrefix[]  = "FILE CONTENT: ";
            const size_t kPrefixLen = sizeof(kPrefix) - 1;
            const size_t outLen     = kPrefixLen + (static_cast<size_t>(dumpLen) * 3);
            auto        *out        = static_cast<char *>(SigmaMemoryNew(outLen + 1, 0, nullptr, 1));
            if (out != nullptr) {
                SigmaMemoryMove(out, const_cast<char *>(kPrefix), kPrefixLen);
                size_t pos = kPrefixLen;
                for (u32 i = 0; i < dumpLen; ++i) {
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
            if (PublisherFileData == 0u)
                return;
            auto bytes  = PublisherFileData->m_elements;
            auto usize  = static_cast<uint32>(PublisherFileData->m_size);
            auto m_size = LODWORD(usize);

            PRINT("PublisherFileDataHex blz::basic_string (filesize: %d | stringsize: %d)", length, m_size)
            auto        dump_path     = blz_make_stringf("%s/dumps/dmp_%u.dat", g_szBaseDir, length);
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

    // static int g_nRiftNum = 0;

    // HOOK_DEFINE_TRAMPOLINE(StorageGetPubFile) {
    //     static void Callback(int32 nUserIndex, const char *szFilename, Console::Online::StorageGetFileCallback pfnCallback) {
    //         if (global_config.challenge_rifts.active && szFilename && strstr(szFilename, "challengerift_config.dat")) {  // Comment ENTIRE `if` contents to dump online files, uncomment to force challenges offline
    //             int32               *bind       = new int32 {nUserIndex};
    //             StorageResult        pRes       = Console::Online::STORAGE_SUCCESS;
    //             ChallengeData       *ptChalConf = new ChallengeData {};
    //             WeeklyChallengeData *ptChalData = new WeeklyChallengeData {};
    //             InterceptChallengeRift(bind, &pRes, *ptChalConf, *ptChalData);
    //             return;
    //         } else if (global_config.challenge_rifts.active && szFilename && strstr(szFilename, "challengerift_")) {
    //             auto  szFormat = "challengerift_%d.dat";
    //             auto  dwSize   = BITSIZEOF(szFormat);
    //             auto *lpBuf    = static_cast<char *>(alloca(dwSize));
    //             if (g_nRiftNum > 9)
    //                 g_nRiftNum = 0;
    //             ++g_nRiftNum;
    //             snprintf(lpBuf, dwSize + 1, szFormat, g_nRiftNum);
    //             szFilename = lpBuf;
    //             PRINT_EXPR("Trying for: %s", szFilename)
    //         }
    //         Orig(nUserIndex, szFilename, pfnCallback);
    //     }
    // };

    HOOK_DEFINE_TRAMPOLINE(ChallengeRiftCallback) {
        static void Callback(void *bind, StorageResult *pRes, ChallengeData *ptChalConf, WeeklyChallengeData *ptChalData) {
            const bool can_populate = global_config.challenge_rifts.active && ptChalConf && ptChalData && pRes;
            if (can_populate) {
                if (PopulateChallengeRiftData(*ptChalConf, *ptChalData))
                    *pRes = Console::Online::STORAGE_SUCCESS;
            }
            Orig(bind, pRes, ptChalConf, ptChalData);
        }
    };

    // Challenge Rift reward chest not spawning (seasonal, offline):
    //
    // When a weekly challenge reward is earned, the game persists a ConsoleChallengeRiftReward entry to the
    // profile. In stock, it only tags the reward with `season_earned` when the lobby is connected.
    //
    // For offline seasonal play, that connected gate is false, so rewards can be saved as season_earned == 0.
    // Later seasonal logic filters rewards by current season, so the reward chest never appears in town.
    //
    // Fix: if we have a nonzero season number, tag the reward before running the stock save path.
    HOOK_DEFINE_TRAMPOLINE(ChallengeRiftRewardEarned_SaveToProfileHook) {
        static void Callback(int32 nUserIndex, void *tReward) {
            PRINT(
                "[cr_reward_save] ENTER user=%d reward=0x%lx active=%d",
                nUserIndex,
                reinterpret_cast<uintptr_t>(tReward),
                global_config.challenge_rifts.active ? 1 : 0
            )

            if (!global_config.challenge_rifts.active || tReward == nullptr) {
                Orig(nUserIndex, tReward);
                return;
            }

            const uintptr_t reward_addr   = reinterpret_cast<uintptr_t>(tReward);
            const bool      reward_ptr_ok = IsProbablyReadablePtr(reward_addr);
            PRINT(
                "[cr_reward_save] step ptr_check ok=%d season_num_fn=0x%lx season_state_fn=0x%lx seasonal_fn=0x%lx",
                reward_ptr_ok ? 1 : 0,
                reinterpret_cast<uintptr_t>(OnlineServiceGetSeasonNum),
                reinterpret_cast<uintptr_t>(OnlineServiceGetSeasonState),
                reinterpret_cast<uintptr_t>(GameIsSeasonal)
            )

            uint32 cur_season       = 0u;
            int32  season_state     = -1;
            bool   is_game_seasonal = false;

            PRINT_LINE("[cr_reward_save] step season_num_call");
            if (OnlineServiceGetSeasonNum) {
                cur_season = OnlineServiceGetSeasonNum();
            }
            PRINT("[cr_reward_save] step season_num_done=%u", cur_season)

            PRINT_LINE("[cr_reward_save] step season_state_call");
            if (OnlineServiceGetSeasonState) {
                season_state = OnlineServiceGetSeasonState();
            }
            PRINT("[cr_reward_save] step season_state_done=%d", season_state)

            PRINT_LINE("[cr_reward_save] step game_seasonal_call");
            if (GameIsSeasonal) {
                is_game_seasonal = GameIsSeasonal();
            }
            PRINT("[cr_reward_save] step game_seasonal_done=%d", is_game_seasonal ? 1 : 0)

            auto *ptReward = reinterpret_cast<D3::Account::ConsoleChallengeRiftReward *>(tReward);
            PRINT_LINE("[cr_reward_save] step reward_read_begin");
            if (reward_ptr_ok) {
                PRINT(
                    "[cr_reward_save] pre user=%d challenge=%u season_earned=%u cur_season=%u seasonal=%d season_state=%d has_bits=0x%x",
                    nUserIndex,
                    ptReward->challenge_rift_,
                    ptReward->season_earned_,
                    cur_season,
                    is_game_seasonal ? 1 : 0,
                    season_state,
                    ptReward->_has_bits_[0]
                )
            } else {
                PRINT("[cr_reward_save] pre user=%d reward_ptr_not_readable", nUserIndex)
            }

            if (reward_ptr_ok && cur_season != 0u) {
                PRINT_LINE("[cr_reward_save] step season_tamper_begin");
                ptReward->_has_bits_[0] |= 8u;
                ptReward->season_earned_ = cur_season;
                PRINT(
                    "[cr_reward_save] step season_tamper_done challenge=%u season_earned=%u has_bits=0x%x",
                    ptReward->challenge_rift_,
                    ptReward->season_earned_,
                    ptReward->_has_bits_[0]
                )
            }

            PRINT_LINE("[cr_reward_save] step orig_call_begin");
            Orig(nUserIndex, tReward);
            PRINT_LINE("[cr_reward_save] step orig_call_done");

            if (reward_ptr_ok) {
                PRINT(
                    "[cr_reward_save] post user=%d challenge=%u season_earned=%u has_bits=0x%x",
                    nUserIndex,
                    ptReward->challenge_rift_,
                    ptReward->season_earned_,
                    ptReward->_has_bits_[0]
                )
            }
        }
    };

    // Challenge Rift reward claim bug (seasonal):
    //
    // The game stores challenge rift rewards with a `season_earned` tag and, when the game is seasonal,
    // it only grants rewards whose season matches OnlineService::GetSeasonNum().
    //
    // In some local/offline challenge rift flows the reward entry can end up with season_earned == 0,
    // which makes non-seasonal heroes able to claim (season check skipped) but seasonal heroes unable
    // to claim.
    //
    // Fix: if no reward matches the current season but there is an "untagged" entry (season 0),
    // rewrite its season_earned to the current season right before running the stock grant logic.
    HOOK_DEFINE_TRAMPOLINE(ChallengeRiftGrantRewardToPlayerIfEligibleHook) {
        static void Callback(Player *ptPlayer) {
            if (global_config.challenge_rifts.active && ptPlayer != nullptr) {
                constexpr uintptr_t kRewardsListSentinelOff = 0xDD88;
                constexpr uintptr_t kRewardsListHeadOff     = 0xDD90;
                constexpr uintptr_t kNodeNextOff            = 0x8;
                constexpr uintptr_t kNodeSeasonEarnedOff    = 0x48;

                const uint32 cur_season       = OnlineServiceGetSeasonNum ? OnlineServiceGetSeasonNum() : 0u;
                const int32  season_state     = OnlineServiceGetSeasonState ? OnlineServiceGetSeasonState() : -1;
                const bool   is_game_seasonal = GameIsSeasonal ? GameIsSeasonal() : false;
                PRINT(
                    "[cr_reward_claim] pre player=0x%lx user=%d last_reward_challenge=%u cur_season=%u seasonal=%d season_state=%d",
                    reinterpret_cast<uintptr_t>(ptPlayer),
                    ptPlayer->nControllerIndex,
                    ptPlayer->nLastRewardChallengeNumber,
                    cur_season,
                    is_game_seasonal ? 1 : 0,
                    season_state
                )
                if (cur_season != 0u) {
                    const uintptr_t player_addr = reinterpret_cast<uintptr_t>(ptPlayer);
                    const uintptr_t sentinel    = player_addr + kRewardsListSentinelOff;
                    uintptr_t       node        = *reinterpret_cast<uintptr_t const *>(player_addr + kRewardsListHeadOff);

                    bool      has_current       = false;
                    uintptr_t first_zero        = 0;
                    uint32    first_zero_before = 0;
                    size_t    visited           = 0;

                    while (node != 0 && node != sentinel) {
                        ++visited;
                        const uint32 season_earned = *reinterpret_cast<uint32 const *>(node + kNodeSeasonEarnedOff);
                        PRINT(
                            "[cr_reward_claim] node=0x%lx season_earned=%u idx=%zu",
                            node,
                            season_earned,
                            visited
                        )
                        if (season_earned == cur_season) {
                            has_current = true;
                            break;
                        }
                        if (season_earned == 0u && first_zero == 0) {
                            first_zero        = node;
                            first_zero_before = season_earned;
                        }
                        node = *reinterpret_cast<uintptr_t const *>(node + kNodeNextOff);
                    }
                    PRINT(
                        "[cr_reward_claim] scan_done visited=%zu has_current=%d first_zero=0x%lx",
                        visited,
                        has_current ? 1 : 0,
                        first_zero
                    )

                    if (!has_current && first_zero != 0) {
                        *reinterpret_cast<uint32 *>(first_zero + kNodeSeasonEarnedOff) = cur_season;
                        const uint32 after                                             = *reinterpret_cast<uint32 const *>(first_zero + kNodeSeasonEarnedOff);
                        PRINT(
                            "[cr_reward_claim] tamper node=0x%lx season_earned %u -> %u",
                            first_zero,
                            first_zero_before,
                            after
                        )
                    }
                }
            }

            Orig(ptPlayer);
        }
    };

    // Challenge Rift reward eligibility bypass:
    //
    // Stock behavior gates rewards to 1x per weekly challenge number by tracking the last reward's
    // challenge number on the player. This is good for real gameplay, but it blocks rapid testing
    // (and any "farmable" reward behavior).
    //
    // For repeat testing, run the stock handler with a rewound remembered challenge number.
    // This preserves stock branch decisions/UI status for the current completion while re-arming
    // the same challenge for the next run.
    HOOK_DEFINE_TRAMPOLINE(ChallengeRiftHandleChallengeRewardsHook) {
        static void Callback(Player *ptPlayer, int32 arg1, void *reward_screen_info) {
            if (global_config.challenge_rifts.active && ptPlayer != nullptr) {
                const uint32 cur_season              = OnlineServiceGetSeasonNum ? OnlineServiceGetSeasonNum() : 0u;
                const int32  season_state            = OnlineServiceGetSeasonState ? OnlineServiceGetSeasonState() : -1;
                const bool   is_game_seasonal        = GameIsSeasonal ? GameIsSeasonal() : false;
                const uint32 challenge_num           = cr_debug::GetCurrentChallengeNumber();
                const uint32 pre_clear               = ptPlayer->nLastRewardChallengeNumber;
                const uint32 rearm_value             = (challenge_num > 1u) ? (challenge_num - 1u) : 1u;
                ptPlayer->nLastRewardChallengeNumber = rearm_value;
                PRINT(
                    "[cr_reward_handle] pre player=0x%lx challenge=%u total_time_ms=%d last_reward_challenge=%u -> %u cur_season=%u seasonal=%d season_state=%d",
                    reinterpret_cast<uintptr_t>(ptPlayer),
                    challenge_num,
                    arg1,
                    pre_clear,
                    ptPlayer->nLastRewardChallengeNumber,
                    cur_season,
                    is_game_seasonal ? 1 : 0,
                    season_state
                )
            }

            Orig(ptPlayer, arg1, reward_screen_info);

            if (global_config.challenge_rifts.active && ptPlayer != nullptr) {
                const uint32 before_clear = ptPlayer->nLastRewardChallengeNumber;
                // const uint32 challenge_num = cr_debug::GetCurrentChallengeNumber();
                // const uint32 rearm_value = (challenge_num > 1u) ? (challenge_num - 1u) : 1u;
                ptPlayer->nLastRewardChallengeNumber = 1u;
                const uint32 after_clear             = ptPlayer->nLastRewardChallengeNumber;
                if (reward_screen_info != nullptr) {
                    const auto *screen = reinterpret_cast<uint32 const *>(reward_screen_info);
                    PRINT(
                        "[cr_reward_handle] post reward_screen challenge=%u reward_gbid=%u pylon=%u status=%u",
                        screen[0],
                        screen[1],
                        screen[2],
                        screen[3]
                    )
                }
                PRINT(
                    "[cr_reward_handle] post last_reward_challenge %u -> %u",
                    before_clear,
                    after_clear
                )
            }
        }
    };

    // Weekly reward eligibility is precomputed when opening a challenge rift from
    // Player::nLastRewardChallengeNumber. Force-rearm that field before stock open logic so
    // test runs always begin in an eligible state.
    HOOK_DEFINE_TRAMPOLINE(OpenChallengeRiftHook) {
        static void Callback() {
            if (global_config.challenge_rifts.active) {
                const uint32 weekly_num = cr_debug::GetCurrentChallengeNumber();
                int          patched    = 0;
                for (Player *pt = PlayerGetFirstAll(); pt != nullptr; pt = PlayerGetNextAll(pt)) {
                    const uint32 before      = pt->nLastRewardChallengeNumber;
                    const uint32 rearm_value = (weekly_num > 1u) ? (weekly_num - 1u) : 1u;
                    if (before != rearm_value) {
                        pt->nLastRewardChallengeNumber = rearm_value;
                        ++patched;
                        PRINT(
                            "[cr_reward_open] prerearm player=0x%lx weekly=%u last_reward_challenge=%u -> %u",
                            reinterpret_cast<uintptr_t>(pt),
                            weekly_num,
                            before,
                            pt->nLastRewardChallengeNumber
                        )
                    }
                }
                PRINT("[cr_reward_open] prerearm_done weekly=%u patched=%d", weekly_num, patched)
            }
            Orig();
        }
    };

    inline void SetupDebuggingHooks() {
        if (global_config.challenge_rifts.active) {
            cr_debug::SetRewardSaveGateBypass(true);
            // StorageGetPubFile::
            //     InstallAtFuncPtr(StorageGetPublisherFile);
            Print_ChallengeRiftFailed::
                InstallAtSymbol("sym_print_challenge_rift_failed");
            ChallengeRiftCallback::
                InstallAtFuncPtr(challenge_rift_callback);
            OpenChallengeRiftHook::
                InstallAtFuncPtr(OpenChallengeRift);
            ChallengeRiftRewardEarned_SaveToProfileHook::
                InstallAtFuncPtr(ChallengeRiftRewardEarned_SaveToProfile);
            ChallengeRiftHandleChallengeRewardsHook::
                InstallAtFuncPtr(ChallengeRiftHandleChallengeRewards);
            ChallengeRiftGrantRewardToPlayerIfEligibleHook::
                InstallAtFuncPtr(ChallengeRiftGrantRewardToPlayerIfEligible);
            ParsePartialFromStringHook::
                InstallAtFuncPtr(ParsePartialFromString);
        } else {
            cr_debug::SetRewardSaveGateBypass(false);
        }

        if (!global_config.defaults_only && global_config.debug.active && global_config.debug.enable_pubfile_dump) {
            PubFileDataHex::
                InstallAtSymbol("sym_pubfile_data_hex");  // Use to dump online files
        }

        if (global_config.debug.active && global_config.debug.enable_error_traces) {
            Print_ErrorDisplay::
                InstallAtSymbol("sym_print_error_display");
            // Print_Error::
            //     InstallAtSymbol("sym_print_error");
            // Print_ErrorString::
            //     InstallAtSymbol("sym_print_error_string");
            Print_ErrorStringFinal::
                InstallAtSymbol("sym_print_error_string_final");
        }

        if (global_config.debug.active && global_config.debug.tagnx) {
            tagnx::InstallHooks();
        }

        // BDPublish::
        //     InstallAtFuncPtr(bdLogSubscriber_publish);
        // CountCosmetics::
        //     InstallAtSymbol("sym_count_cosmetics");
        // ParseFile::
        //     InstallAtSymbol("sym_parse_file");
        // FontSnoop::
        //     InstallAtSymbol("sym_font_snoop");
        // Store_AttribDefs::
        //     InstallAtSymbol("sym_store_attrib_defs");
        // Print_SavedAttribs::
        //     InstallAtSymbol("sym_print_saved_attribs");
    }

}  // namespace d3
