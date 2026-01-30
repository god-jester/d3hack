#pragma once

#include "d3/types/blz_types.hpp"
#include "d3/types/common.hpp"

#include <cstddef>

struct ErrorManagerBlizzardErrorMetaData;
struct ErrorManagerGlobals;
struct ErrorManagerPlatformGlobals;
struct GamePerfGlobals;
struct MemoryPools;
struct RefStringGlobals;

using ErrorManagerBlizzardErrorMode = int32;

enum ErrorManagerBlizzardErrorIssueType : int32 {
    EMBEIT_EXCEPTION = 0x0,
    EMBEIT_BUG       = 0x1,
    EMBEIT_COUNT     = 0x2,
};

enum ErrorManagerJiraSeverity : int32 {
    EMJS_HIGH    = 0x0,
    EMJS_MAJOR   = 0x1,
    EMJS_MINOR   = 0x2,
    EMJS_TRIVIAL = 0x3,
    EMJS_NONE    = 0x4,
    EMJS_COUNT   = 0x5,
};

enum OS_Type : int32 {
    OSTYPE_UNKNOWN             = 0x0,
    OSTYPE_WINDOWS95           = 0x1,
    OSTYPE_WINDOWS98           = 0x2,
    OSTYPE_WINDOWSME           = 0x3,
    OSTYPE_WINDOWSNT           = 0x4,
    OSTYPE_WINDOWS2000         = 0x5,
    OSTYPE_WINDOWSXP           = 0x6,
    OSTYPE_WINDOWSSERVER2003   = 0x7,
    OSTYPE_WINDOWSVISTA        = 0x8,
    OSTYPE_WINDOWSSERVER2008   = 0x9,
    OSTYPE_WINDOWS7            = 0xA,
    OSTYPE_WINDOWSSERVER2008R2 = 0xB,
    OSTYPE_WINDOWS8            = 0xC,
    OSTYPE_WINDOWSSERVER2012   = 0xD,
    OSTYPE_WINDOWS8_1          = 0xE,
    OSTYPE_WINDOWSSERVER2012R2 = 0xF,
    OSTYPE_WINDOWS10           = 0x10,
    OSTYPE_WINDOWS10SERVER     = 0x11,
    OSTYPE_MACOSX              = 0x12,
    OSTYPE_XBOXONE             = 0x13,
    OSTYPE_LINUX               = 0x14,
    OSTYPE_PS4                 = 0x15,
    OSTYPE_NX64                = 0x16,
};

enum ErrorManagerDumpType : int32 {
    EMDT_TINY   = 0x0,
    EMDT_MEDIUM = 0x1,
    EMDT_FULL   = 0x2,
};

using HWND = uint32;

struct OSInfo {
    BOOL    fInitialized;
    OS_Type eOSType;
    BOOL    fOSIs64Bit;
    uint32  uMajorVersion;
    uint32  uMinorVersion;
    uint32  uBuild;
    uint32  uServicePack;
    char    szOSDescription[256];
    char    szOSLanguage[256];
    uint64  uTotalPhysicalMemory;
};

using PFNERRORPREPCALLBACK             = void (*)(void);
using PFNCRASHREPORTCALLBACK           = void (*)(void);
using PFNENDOFCRASHCALLBACK            = void (*)(void);
using PFNGETGAMEID                     = BOOL (*)(uint32 *);
using PFNTERMINATECALLBACK             = void (*)(ErrorCode, uint32);
using PFNUITRACECALLBACK               = void (*)(LPCSTR, const uint32);
using PFNREMOVEAPPTEXTENCODINGCALLBACK = void (*)(LPCSTR, char *, int32);
using PFNTESTINGERRORCALLBACK          = void (*)(LPCSTR);
using PFNAPPDUMPCALLBACK               = void (*)(void);
using PFNCAPSDEPRESSEDCALLBACK         = BOOL (*)(void);

struct ErrorManagerBlizzardErrorData {
    ErrorManagerBlizzardErrorMode      eMode;
    ErrorManagerBlizzardErrorIssueType eIssueType;
    char                               szApplicationDirectory[4096];
    char                               szSummary[1024];
    char                               szAssertion[8192];
    char                               szModules[16384];
    char                               szStackDigest[8192];
    char                               szLocale[128];
    char                               szComments[1024];
    char                               aszAttachments[8][4096];
    uint32                             dwNumAttachments;
    char                               szUserAssignment[128];
    char                               szEmailAddress[1024];
    char                               szReopenCmdLine[4096];
    char                               szRepairCmdLine[4096];
    ErrorManagerJiraSeverity           eSeverity;
    char                               szReportDirectory[4096];
    blz::vector<ErrorManagerBlizzardErrorMetaData, blz::allocator<ErrorManagerBlizzardErrorMetaData>>
         aMetadata;
    char szBranch[32];
};

struct ALIGNED(8) ErrorManagerGlobals {
    PFNERRORPREPCALLBACK             pfnErrorPrepCallback;
    PFNCRASHREPORTCALLBACK           pfnCrashReportCallback;
    PFNENDOFCRASHCALLBACK            pfnEndOfCrashCallback;
    PFNGETGAMEID                     pfnGameIDCallback;
    PFNTERMINATECALLBACK             pfnTerminateCallback;
    PFNUITRACECALLBACK               pfnUITraceCallback;
    PFNREMOVEAPPTEXTENCODINGCALLBACK pfnRemoveAppTextEncodingCallback;
    PFNTESTINGERRORCALLBACK          pfnTestingErrorCallback;
    PFNAPPDUMPCALLBACK               pfnAppDumpCallback;
    PFNCAPSDEPRESSEDCALLBACK         pfnCapsDepressedCallback;
    CHAR                             aszTemporary[1024];
    OSInfo                           tSystemInfo;
    CHAR                             szApplicationName[4096];
    CHAR                             szHeroFile[4096];
    blz::string                      szParentProcess;
    BOOL                             fSigmaCurrentlyHandlingError;
    BOOL                             fSigmaCurrentlyInExceptionHandler;
    BOOL                             fSuppressUserInteraction;
    BOOL                             fSuppressStackCrawls;
    BOOL                             fSuppressModuleEnumeration;
    BOOL                             fSuppressTracingToScreen;
    uint32                           dwInspectorProjectID;
    uint32                           dwTraceHesitationThreshhold;
    Stopwatch                        tTraceHesitationStopwatch;
    float                            flLastTraceTime;
    HWND                             hwnd;
    int32                            nCrashNumber;
    uint32                           dwShouldTerminateToken;
    ErrorManagerBlizzardErrorMode    eErrorManagerBlizzardErrorCrashMode;
    uint8                            _pad26dc[4];
    ErrorManagerBlizzardErrorData    tErrorManagerBlizzardErrorData;
    ErrorManagerDumpType             eDumpType;
    uint8                            _pad1742c[4];
};

struct SigmaGlobals {
    MemoryPools                 *ptMemoryPools;
    ErrorManagerGlobals         *ptEMGlobals;
    ErrorManagerPlatformGlobals *ptEMPGlobals;
    GamePerfGlobals             *ptGamePerfGlobals;
    RefStringGlobals            *ptRefStringGlobals;
};

// static_assert(sizeof(ErrorManagerBlizzardErrorData) == 0x14D48);
// static_assert(offsetof(ErrorManagerBlizzardErrorData, szApplicationDirectory) == 0x8);
// static_assert(offsetof(ErrorManagerBlizzardErrorData, szStackDigest) == 0x7408);
// static_assert(offsetof(ErrorManagerBlizzardErrorData, aszAttachments) == 0x9888);
// static_assert(offsetof(ErrorManagerBlizzardErrorData, dwNumAttachments) == 0x11888);
// static_assert(offsetof(ErrorManagerBlizzardErrorData, szReportDirectory) == 0x13D10);
// static_assert(offsetof(ErrorManagerBlizzardErrorData, aMetadata) == 0x14D10);
// static_assert(offsetof(ErrorManagerBlizzardErrorData, szBranch) == 0x14D28);

// static_assert(sizeof(OSInfo) == 0x228);
// static_assert(offsetof(OSInfo, szOSDescription) == 0x1C);
// static_assert(offsetof(OSInfo, uTotalPhysicalMemory) == 0x220);

// static_assert(sizeof(ErrorManagerGlobals) == 0x17430);
// static_assert(offsetof(ErrorManagerGlobals, aszTemporary) == 0x50);
// static_assert(offsetof(ErrorManagerGlobals, tSystemInfo) == 0x450);
// static_assert(offsetof(ErrorManagerGlobals, szParentProcess) == 0x2678);
// static_assert(offsetof(ErrorManagerGlobals, tErrorManagerBlizzardErrorData) == 0x26E0);
// static_assert(offsetof(ErrorManagerGlobals, eDumpType) == 0x17428);

// static_assert(sizeof(SigmaGlobals) == 0x28);
// static_assert(offsetof(SigmaGlobals, ptEMGlobals) == 0x8);
