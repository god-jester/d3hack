#include "exception_handler.hpp"

#include "d3/_util.hpp"
#include "lib/diag/assert.hpp"
#include "nn/os.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace d3 {
    namespace {
        constexpr char   k_user_exception_dump_path[]       = "sd:/config/d3hack-nx/user_exception.txt";
        constexpr size_t k_user_exception_stack_trace_depth = 64;
        constexpr size_t k_dying_message_size               = 0x1000;

        alignas(nn::os::HandlerStackAlignment) u8 g_user_exception_stack[0x8000] = {};
        nn::os::UserExceptionInfo g_user_exception_info                          = {};
        alignas(0x1000) char g_dying_message[k_dying_message_size]               = {};

        struct FileWriter {
            FileReference *ref;
            u32            offset;

            void WriteRaw(const void *data, size_t size) {
                if (ref == nullptr || data == nullptr || size == 0)
                    return;
                const u8 *bytes      = reinterpret_cast<const u8 *>(data);
                const u32 write_size = static_cast<u32>(size);
                FileWrite(ref, bytes, offset, write_size, ERROR_FILE_WRITE);
                offset += write_size;
            }

            void Write(const char *fmt, ...) {
                char    buf[512] = {};
                va_list vl;
                va_start(vl, fmt);
                int n = vsnprintf(buf, sizeof(buf), fmt, vl);
                va_end(vl);
                if (n <= 0)
                    return;
                const u32 size =
                    (n >= static_cast<int>(sizeof(buf))) ? static_cast<u32>(sizeof(buf) - 1) : static_cast<u32>(n);
                WriteRaw(buf, size);
            }
        };

        const char *UserExceptionTypeName(u32 type) {
            switch (type) {
            case nn::os::UserExceptionType_None:
                return "None";
            case nn::os::UserExceptionType_InvalidInstructionAccess:
                return "InvalidInstructionAccess";
            case nn::os::UserExceptionType_InvalidDataAccess:
                return "InvalidDataAccess";
            case nn::os::UserExceptionType_UnalignedInstructionAccess:
                return "UnalignedInstructionAccess";
            case nn::os::UserExceptionType_UnalignedDataAccess:
                return "UnalignedDataAccess";
            case nn::os::UserExceptionType_UndefinedInstruction:
                return "UndefinedInstruction";
            case nn::os::UserExceptionType_ExceptionalInstruction:
                return "ExceptionalInstruction";
            case nn::os::UserExceptionType_MemorySystemError:
                return "MemorySystemError";
            case nn::os::UserExceptionType_FloatingPointException:
                return "FloatingPointException";
            case nn::os::UserExceptionType_InvalidSystemCall:
                return "InvalidSystemCall";
            default:
                return "Unknown";
            }
        }

        void WriteFixedString(FileWriter *writer, const char *label, const char *value, size_t max_len) {
            if (writer == nullptr || label == nullptr || value == nullptr)
                return;
            writer->WriteRaw(label, std::strlen(label));
            const size_t len = strnlen(value, max_len);
            if (len > 0)
                writer->WriteRaw(value, len);
            writer->WriteRaw("\n", 1);
        }

        const char *GetBlzStringPtr(const blz::string &str) {
            if (str.m_elements != nullptr)
                return str.m_elements;
            return str.m_storage;
        }

        void WriteBlzString(FileWriter *writer, const char *label, const blz::string &str) {
            if (writer == nullptr || label == nullptr)
                return;
            writer->WriteRaw(label, std::strlen(label));
            const char  *ptr = GetBlzStringPtr(str);
            const size_t len = str.m_size;
            if (ptr != nullptr && len > 0)
                writer->WriteRaw(ptr, len);
            writer->WriteRaw("\n", 1);
        }

        void WriteAddressWithModule(FileWriter *writer, const char *label, uintptr_t address) {
            if (writer == nullptr || label == nullptr)
                return;
            const auto *module = exl::util::TryGetModule(address);
            if (module == nullptr) {
                writer->Write("%s: 0x%016llx\n", label, static_cast<unsigned long long>(address));
                return;
            }
            const auto name   = module->GetModuleName();
            const auto offset = address - module->m_Total.m_Start;
            writer->Write("%s: 0x%016llx (", label, static_cast<unsigned long long>(address));
            writer->WriteRaw(name.data(), name.size());
            writer->Write("+0x%llx)\n", static_cast<unsigned long long>(offset));
        }

        void WriteRegisterDump(FileWriter *writer, const nn::os::UserExceptionInfoDetail &detail) {
            if (writer == nullptr)
                return;
            writer->Write("Registers:\n");
            for (size_t i = 0; i < 29; ++i) {
                writer->Write("  x%02zu=0x%016llx\n", i, static_cast<unsigned long long>(detail.r[i]));
            }
            writer->Write(
                "  fp=0x%016llx lr=0x%016llx sp=0x%016llx pc=0x%016llx\n",
                static_cast<unsigned long long>(detail.fp),
                static_cast<unsigned long long>(detail.lr),
                static_cast<unsigned long long>(detail.sp),
                static_cast<unsigned long long>(detail.pc)
            );
            writer->Write(
                "  pstate=0x%08x fpcr=0x%08x fpsr=0x%08x\n",
                detail.pstate,
                detail.fpcr,
                detail.fpsr
            );
            writer->Write(
                "  afsr0=0x%08x afsr1=0x%08x esr=0x%08x far=0x%016llx\n",
                detail.afsr0,
                detail.afsr1,
                detail.esr,
                static_cast<unsigned long long>(detail.far)
            );
        }

        void WriteStackTrace(FileWriter *writer, const nn::os::UserExceptionInfoDetail &detail) {
            if (writer == nullptr)
                return;
            writer->Write("Stack trace:\n");
            WriteAddressWithModule(writer, "  PC", static_cast<uintptr_t>(detail.pc));
            WriteAddressWithModule(writer, "  LR", static_cast<uintptr_t>(detail.lr));

            const uintptr_t fp = static_cast<uintptr_t>(detail.fp);
            if (fp == 0) {
                writer->Write("  <no frame pointer>\n");
                return;
            }

            auto   it    = exl::util::stack_trace::Iterator(fp, exl::util::mem_layout::s_Stack);
            size_t depth = 0;
            while (it.Step() && depth < k_user_exception_stack_trace_depth) {
                char label[24];
                snprintf(label, sizeof(label), "  #%zu", depth);
                WriteAddressWithModule(writer, label, it.GetReturnAddress());
                depth++;
            }
        }

        void WriteModules(FileWriter *writer) {
            if (writer == nullptr)
                return;
            writer->Write("Modules:\n");
            for (int i = static_cast<int>(exl::util::ModuleIndex::Start);
                 i < static_cast<int>(exl::util::ModuleIndex::End);
                 ++i) {
                const auto index = static_cast<exl::util::ModuleIndex>(i);
                if (!exl::util::HasModule(index))
                    continue;
                const auto &module = exl::util::GetModuleInfo(index);
                const auto  name   = module.GetModuleName();
                writer->Write(
                    "  %02d: 0x%016llx-0x%016llx ",
                    i,
                    static_cast<unsigned long long>(module.m_Total.m_Start),
                    static_cast<unsigned long long>(module.m_Total.GetEnd())
                );
                writer->WriteRaw(name.data(), name.size());
                writer->Write("\n");
            }
        }

        void WriteMemoryInfo(FileWriter *writer) {
            if (writer == nullptr)
                return;
            nn::os::MemoryInfo mem_info {};
            nn::os::QueryMemoryInfo(&mem_info);
            if (mem_info.totalAvailableMemorySize == 0) {
                writer->Write("mem: QueryMemoryInfo unavailable\n");
                return;
            }
            const u64 total_available      = mem_info.totalAvailableMemorySize;
            const u64 total_used           = static_cast<u64>(mem_info.totalUsedMemorySize);
            const u64 available_after_used = (total_available > total_used) ? (total_available - total_used) : 0ull;
            writer->Write(
                "mem: total=%llu used=%llu avail=%llu heap_total=%llu heap_alloc=%llu program=%llu stack_total=%llu threads=%d\n",
                static_cast<unsigned long long>(total_available),
                static_cast<unsigned long long>(total_used),
                static_cast<unsigned long long>(available_after_used),
                static_cast<unsigned long long>(mem_info.totalMemoryHeapSize),
                static_cast<unsigned long long>(mem_info.allocatedMemoryHeapSize),
                static_cast<unsigned long long>(mem_info.programSize),
                static_cast<unsigned long long>(mem_info.totalThreadStackSize),
                mem_info.threadCount
            );
        }

        void WriteErrorManager(FileWriter *writer) {
            if (writer == nullptr)
                return;
            if (g_tSigmaGlobals.ptEMGlobals == nullptr) {
                writer->Write("ErrorMgr: <null>\n");
                return;
            }
            const auto &em = *g_tSigmaGlobals.ptEMGlobals;
            writer->Write(
                "ErrorMgr: dump=%d crash_mode=%d crash_num=%d terminate_token=%u\n",
                em.eDumpType,
                em.eErrorManagerBlizzardErrorCrashMode,
                em.nCrashNumber,
                em.dwShouldTerminateToken
            );
            writer->Write(
                "ErrorMgr: flags handling=%d in_handler=%d suppress_ui=%d suppress_stack=%d suppress_mods=%d suppress_trace=%d\n",
                em.fSigmaCurrentlyHandlingError,
                em.fSigmaCurrentlyInExceptionHandler,
                em.fSuppressUserInteraction,
                em.fSuppressStackCrawls,
                em.fSuppressModuleEnumeration,
                em.fSuppressTracingToScreen
            );
            writer->Write(
                "ErrorMgr: inspector=%u trace_thresh=%u last_trace=%.3f hwnd=0x%x\n",
                em.dwInspectorProjectID,
                em.dwTraceHesitationThreshhold,
                em.flLastTraceTime,
                em.hwnd
            );
            WriteFixedString(writer, "ErrorMgr: app=", em.szApplicationName, sizeof(em.szApplicationName));
            WriteFixedString(writer, "ErrorMgr: hero=", em.szHeroFile, sizeof(em.szHeroFile));
            WriteBlzString(writer, "ErrorMgr: parent=", em.szParentProcess);

            const auto &sys = em.tSystemInfo;
            writer->Write(
                "OS: type=%d 64bit=%d ver=%u.%u build=%u sp=%u mem=%llu\n",
                sys.eOSType,
                sys.fOSIs64Bit,
                sys.uMajorVersion,
                sys.uMinorVersion,
                sys.uBuild,
                sys.uServicePack,
                static_cast<unsigned long long>(sys.uTotalPhysicalMemory)
            );
            WriteFixedString(writer, "OS: desc=", sys.szOSDescription, sizeof(sys.szOSDescription));
            WriteFixedString(writer, "OS: lang=", sys.szOSLanguage, sizeof(sys.szOSLanguage));

            const auto &ed = em.tErrorManagerBlizzardErrorData;
            writer->Write(
                "EMData: mode=%d issue=%d severity=%d attachments=%u\n",
                ed.eMode,
                ed.eIssueType,
                ed.eSeverity,
                ed.dwNumAttachments
            );
            WriteFixedString(writer, "EMData: app_dir=", ed.szApplicationDirectory, sizeof(ed.szApplicationDirectory));
            WriteFixedString(writer, "EMData: summary=", ed.szSummary, sizeof(ed.szSummary));
            WriteFixedString(writer, "EMData: assertion=", ed.szAssertion, sizeof(ed.szAssertion));
            WriteFixedString(writer, "EMData: modules=", ed.szModules, sizeof(ed.szModules));
            WriteFixedString(writer, "EMData: stack_digest=", ed.szStackDigest, sizeof(ed.szStackDigest));
            WriteFixedString(writer, "EMData: locale=", ed.szLocale, sizeof(ed.szLocale));
            WriteFixedString(writer, "EMData: comments=", ed.szComments, sizeof(ed.szComments));
            for (u32 i = 0; i < 8; ++i) {
                if (i >= ed.dwNumAttachments)
                    break;
                char label[64];
                snprintf(label, sizeof(label), "EMData: attachment[%u]=", i);
                WriteFixedString(writer, label, ed.aszAttachments[i], sizeof(ed.aszAttachments[i]));
            }
            WriteFixedString(writer, "EMData: user_assign=", ed.szUserAssignment, sizeof(ed.szUserAssignment));
            WriteFixedString(writer, "EMData: email=", ed.szEmailAddress, sizeof(ed.szEmailAddress));
            WriteFixedString(writer, "EMData: reopen_cmd=", ed.szReopenCmdLine, sizeof(ed.szReopenCmdLine));
            WriteFixedString(writer, "EMData: repair_cmd=", ed.szRepairCmdLine, sizeof(ed.szRepairCmdLine));
            WriteFixedString(writer, "EMData: report_dir=", ed.szReportDirectory, sizeof(ed.szReportDirectory));
            writer->Write(
                "EMData: metadata ptr=%p size=%zu cap_bits=%llu embedded=%llu\n",
                ed.aMetadata.m_elements,
                static_cast<size_t>(ed.aMetadata.m_size),
                static_cast<unsigned long long>(ed.aMetadata.m_capacity),
                static_cast<unsigned long long>(ed.aMetadata.m_capacity_is_embedded)
            );
            WriteFixedString(writer, "EMData: branch=", ed.szBranch, sizeof(ed.szBranch));
        }

        void UpdateDyingMessage(const nn::os::UserExceptionInfo *info) {
            if (info == nullptr) {
                snprintf(g_dying_message, sizeof(g_dying_message), "d3hack: exception info missing");
                return;
            }
            const auto &detail = info->detail;
            snprintf(
                g_dying_message,
                sizeof(g_dying_message),
                "d3hack: type=0x%x pc=0x%llx lr=0x%llx far=0x%llx",
                info->exceptionType,
                static_cast<unsigned long long>(detail.pc),
                static_cast<unsigned long long>(detail.lr),
                static_cast<unsigned long long>(detail.far)
            );
        }

        void UserExceptionDump(const nn::os::UserExceptionInfo *info) {
            UpdateDyingMessage(info);

            FileReference tFileRef;
            FileReferenceInit(&tFileRef, k_user_exception_dump_path);
            FileCreate(&tFileRef);
            if (FileOpen(&tFileRef, 2u) == 0)
                return;

            FileWriter writer {&tFileRef, 0u};
            writer.Write("\n--- UserException ---\n");
            writer.Write("core=%d\n", nn::os::GetCurrentCoreNumber());

            if (info == nullptr) {
                writer.Write("info=<null>\n");
                FileClose(&tFileRef);
                return;
            }

            const u32 type = info->exceptionType;
            writer.Write("type=0x%x (%s)\n", type, UserExceptionTypeName(type));
            WriteRegisterDump(&writer, info->detail);
            WriteStackTrace(&writer, info->detail);
            WriteModules(&writer);
            WriteMemoryInfo(&writer);
            WriteErrorManager(&writer);
            FileClose(&tFileRef);
        }

        void UserExceptionHandler(nn::os::UserExceptionInfo *info) {
            UserExceptionDump(info);
            EXL_ABORT("UserExceptionHandler invoked");
        }
    }  // namespace

    void InstallExceptionHandler() {
        nn::os::SetUserExceptionHandler(
            &UserExceptionHandler,
            g_user_exception_stack,
            sizeof(g_user_exception_stack),
            &g_user_exception_info
        );
        nn::os::EnableUserExceptionHandlerOnDebugging(true);
        nn::os::SetDyingMessageRegion(reinterpret_cast<uintptr_t>(g_dying_message), sizeof(g_dying_message));
    }
}  // namespace d3
