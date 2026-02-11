#pragma once

#include "d3/types/containers.hpp"
#include "d3/types/enums.hpp"
#include "d3/types/namespaces.hpp"
#include "d3/types/onlinebd.hpp"
#include "d3/types/online_service.hpp"
#include "lib/diag/assert.hpp"
#include "lib/reloc/reloc.hpp"
#include "lib/util/ptr_path.hpp"
#include "program/offsets.hpp"

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>
#include <sys/types.h>

using DOMVersionFunc           = int32 (*)(const int32, const struct CXMLNode *, void *);
using TranslateStringToIntFunc = int32 (*)(LPCSTR);
using TranslateIntToStringFunc = CRefString (*)(int32);

struct FastAttribGroup : std::array<_BYTE, 0x12E8> {};
struct PlayerSavedData : std::array<_BYTE, 0x12B0> {};
struct PlayerList : std::array<_BYTE, 0x28> {};
struct ServerTrickleList : std::array<_BYTE, 0x28> {};
struct TPlayerPointerPool : std::array<_BYTE, 0x30> {};
struct WorldPlace : std::array<_BYTE, 0x10> {};
struct XVarString : std::array<_BYTE, 0xF8> {};
struct Blizzard_Lock_Mutex : std::array<_BYTE, 0x20> {};

// NOLINTBEGIN

inline uintptr_t GameOffset(uintptr_t offset) {
    return exl::util::GetMainModuleInfo().m_Total.m_Start + offset;
}

inline uintptr_t GameOffsetFromTable(const char *name) {
    const auto *entry = exl::reloc::GetLookupTable().FindByName(name);
    EXL_ABORT_UNLESS(entry != nullptr, "Missing lookup entry: %s", name);
    const auto &module = exl::util::GetModuleInfo(entry->m_ModuleIndex);
    return module.m_Total.m_Start + entry->m_Offset;
}

template<typename EndType, uintptr_t... Offsets>
inline EndType *FollowPtr(uintptr_t ptr) {
    auto offset = exl::util::pointer_path::FollowSafe<Offsets...>(ptr);
    return (EndType *)offset;
}

template<typename EndType, uintptr_t... Offsets>
inline EndType *FollowPtr(void const *ptr) {
    auto offset = exl::util::pointer_path::FollowSafe<Offsets...>(ptr);
    return (EndType *)offset;
}

template<typename... Ts>
std::array<std::byte, sizeof...(Ts)> make_bytes(Ts &&...args) noexcept {
    return {std::byte(std::forward<Ts>(args))...};
}

constexpr int div_ceil(int numerator, int denominator) {
    return (numerator / denominator) + static_cast<int>((numerator % denominator) != 0);
}

struct divide_t {
    int quot;
    int rem;
};

constexpr divide_t divide(int numerator, int denominator) {
    return divide_t {numerator / denominator, numerator % denominator};
}

template<size_t SizeInBits>
class XBitArray {
    char elements_[div_ceil(SizeInBits, 8)];

   public:
    bool GetNthBit(size_t index) const {
        auto d = divide(index, 8);
        return static_cast<bool>(elements_[d.quot] >> d.rem);
    }

    void SetNthBit(size_t index, bool value) {
        auto d = divide(index, 8);
        if (value) {
            elements_[d.quot] |= (1 << d.rem);
        } else {
            elements_[d.quot] &= ~(1 << d.rem);
        }
    }
};

struct ALIGNED(4) GBHandle {
    GameBalanceType eType;
    GBID            gbid;
};

struct GBHandlePool : XListMemoryPool<GBHandle> {};

typedef XPooledList<GBHandle, const GBHandle &> GBHandleList;

struct SymbolicEntry {
    int32       nValue;
    const char *sName;
};

struct ALIGNED(8) XTypeDescriptor {
    int (**_vptr$XTypeDescriptor)(void);
    LPCSTR                     szClassName;
    int32                      nVersion;
    const TypeDescriptorField *arFields;
    uint32                     nFieldCount;
    DOMVersionFunc             pfuncDOMVersion;
    uint32                     dwFlags;
};

struct ALIGNED(8) TypeDescriptorField {
    LPCSTR                   szFieldName;
    const XTypeDescriptor   *ptDataType;
    int32                    nDataOffset;
    const void              *vDefaultFieldValue;
    const void              *vMinFieldValue;
    const void              *vMaxFieldValue;
    uint32                   dwFlags;
    const XTypeDescriptor   *ptAuxDataType;
    int32                    nSerOffset;
    int32                    nCount;
    int32                    nUsedElementsOffset;
    int16                    nBitCount;
    int16                    nArraySizeBitCount;
    uint32                   dwFilter;
    int32                    eTagBase;
    const SymbolicEntry     *pSymbolTable;
    int32                    nBit;
    TranslateStringToIntFunc pfnStringToInt;
    TranslateIntToStringFunc pfnIntToString;
    int32                    nVariantTypeValue;
    CHAR                     szDynamicFieldName[64];
};

struct SerializeData {
    uint32 dwOffset;
    uint32 dwSize;
};

struct RefStringMemPool {
    uintptr_t *ptRefStringDataAllocator;     // TRefStringDataAllocator *
    uintptr_t *ptBigRefStringDataAllocator;  // TBigRefStringDataAllocator *
};

struct CRefStringData {
    int16             m_nReferenceCount;
    int16             m_eBufferLocation;
    size_t            m_nDataLength;
    size_t            m_nAllocatedStringLength;
    RefStringMemPool *m_ptRefStringMemPool;
    clock_t           m_nCreationTime;
};

struct ALIGNED(8) TBigRefStringDataBuffer {
    CRefStringData m_data;
    CHAR           m_szString[4097];
};

struct ALIGNED(8) TRefStringDataBuffer {
    CRefStringData m_data;
    CHAR           m_szString[129];
};

typedef XChainedFixedBlockAllocator<TRefStringDataBuffer>    TRefStringDataAllocator;
typedef XChainedFixedBlockAllocator<TBigRefStringDataBuffer> TBigRefStringDataAllocator;

struct SavedAttribDef {
    int32  nSavedAttribId;
    Attrib eAttrib;
};

struct SavedAttribute {
    uintmax_t Message;           //  :: google::protobuf::Message
    uintmax_t _unknown_fields_;  // google::protobuf::UnknownFieldSet
    uint32    _has_bits_[1];
    int       _cached_size_;
    int32     key_;
    int32     value_;
};

struct ALIGNED(4) FastAttribValue {
    union {
        float flValue;
        int32 nValue;
    } _anon_0;
    explicit FastAttribValue(int32 val) :
        _anon_0 {.nValue = val} {}
    explicit FastAttribValue(float val) :
        _anon_0 {.flValue = val} {}
};

struct AttributeCompressor {
    int (**_vptr$AttributeCompressor)(void);
    AttributeCompressorType m_eType;
    AttributeCompressor    *m_ptNextCompressor;
};

struct BannerRenderData {
    SNO   snoTexBanners[4];
    float flTintU[3];
    int32 nLook;
    SNO   snoActorAttachments[4];
};

struct FastAttribKey {
    s32 nValue;
};

struct SNOName {
    SNOGroup snoGroup;
    SNO      snoHandle;
};

struct Vector3D {
    float x;
    float y;
    float z;
};

struct RareItemName {
    BOOL  fItemNameIsPrefix;
    SNO   snoAffixStringList;
    int32 nAffixStringListIndex;
    int32 nItemStringListIndex;
};

struct CmdLineParams {
    XBitArray<61> tBitArray;
    CHAR          sPlayerName[128];
    CHAR          sDataDirectory[64];
    CHAR          sReplayDirectory[260];
    CHAR          sBuildMopaqName[4096];
    CHAR          sBuildMopaqDirectory[4096];
    CHAR          sBuildName[4096];
    CHAR          sJoinGameName[260];
    int32         nJoinTeamIndex;
    CHAR          sP4ClientName[96];
    CHAR          sP4Port[96];
    CHAR          sP4UserName[96];
    CHAR          sBattleNetGameAddress[96];
    CHAR          sBattleNetAccount[128];
    CHAR          sBattleNetPassword[128];
    CHAR          sServerBindAddress[96];
    CHAR          sServerAdvertAddress[96];
    CHAR          sServerNetAdapter[96];
    AssetMode     eAssetMode;
    CHAR          sLoadWorld[4096];
    CHAR          sLoadScript[4096];
    CHAR          szInstanceName[4096];
    int32         nJobIndex;
    int32         nTotalJobs;
    int32         nDRLGSeed;
    TestType      eTestType;
    CHAR          szTestFilter[4096];
    CHAR          szTestListPath[4096];
    CHAR          szTestEntryPoint[4096];
    uint32        dwMinFPSScreenshot;
    int32         nTestWaitTime;
    int32         nTestSampleTime;
    int32         nTestDumpFreq;
    int32         nTestPlayerNum;
    PlayerClass   eTestClassOverride;
    CHAR          szSeasonsConfigFilename[260];
    CHAR          szBlacklistConfigFilename[260];
    int32         nLockIdentity;
    uint32        uBlizzconGameTimeLimit;
    int32         uFmodEmergencyMemory;
    SigmaPlatform eBuildSigmaPlatform;
    XLocale       eXLocale;
    CHAR          szAgentUid[128];
    CHAR          szAgentType[4096];
    CHAR          szBranchOverride[4096];
    CHAR          sAssetServerAddress[96];
    int32         nAssetServerPort;
};

struct ALIGNED(8) ActorInventory {
    InventoryType eType;
};

struct RGBAColor {
    uint8 r;
    uint8 g;
    uint8 b;
    uint8 a;
};

using UByte4 = RGBAColor;

/* 7007 */
struct GlobalSNOEntry {
    GlobalSNOBucket eBucket;
    SNOName         snoname;
    uint32          dwFlags;
};

struct GameTemplate {
    uint64    uFlags;
    float     flMaxExperienceDist;
    float     flMinimapFeetPerPixel;
    uint32    uMaxPlayerCount;
    GlobalSNO eStartWorld;
};

struct GameClock {
    uint32 dwLastGameClockTime;
    uint32 dwClockTime;
    uint32 dwUsedClockTime;
    uint32 dwUsedGameTime;
    uint32 dwLastTicksPerSec;
    BOOL   bDebugLagEnable;
    uint32 dwDebugLagStartGameClockTime;
    uint32 dwDebugLagEndGameClockTime;
};

struct Stopwatch {
    int64 m_liStartTime;
};

struct FileReference {
    int32                  nRefcount;
    uint32                 dwAccessFlags;
    Blizzard::File::Stream tStream;
    char                   szPath[4096];

    FileReference() :
        nRefcount(0),
        dwAccessFlags(2u),
        tStream(0LL),
        szPath("\0") {}
};

using SNOArray = XGrowableArray<int, int>;

struct ALIGNED(8) GameParams {
    BOOL         bValid;
    SNO          snoInitialWorld;
    SNO          snoInitialQuest;
    int32        nInitialQuestStepUID;
    SNO          snoInitialHero;
    BOOL         bResumeFromSave;
    uint32       dwSeed;
    GameType     eGameType;
    uint32       dwCreationFlags;
    uint32       dwInitialUpdateCount;
    uint32       dwMaxGameTime;
    GameMode     eMode;
    GameParts    nGameParts;
    int32        nMonsterLevel;
    int32        nHandicapLevel;
    Act          eAct;
    GameTemplate tTemplate;
    uint32       uEntranceGUIDOverride;
    CHAR         szServerAddress[260];
    uint32       idGameConnection;
    Act          ePreferredAdventureAct;
    uint64       arFreeKeyTieredRiftParticipatingHeroID[4];
    CHAR         szReplayFileName[260];
    CHAR         szGameName[260];
    uint64       uServerAuthToken;
    SGameID      idSGame;
};

struct CGame {
    GameClock  tRealClock;
    uint32     dwRealTicks;
    uint32     dwRealClockTime;
    uint32     dwRealClockTicksElapsed;
    float      flRealTime;
    float      flRealDeltaTime;
    GameClock  tClientClock;
    uint32     dwClientTicks;
    uint32     dwClientClockTime;
    uint32     dwClientClockTicksElapsed;
    float      flClientTime;
    float      flClientDeltaTime;
    GameParams tGameStartParams;
    GameParams tQueuedGameStartParams;
    BOOL       bUpdateRigidBodyPhysics;
    BOOL       bLastClientUpdate;
    BOOL       bUpdateUIPositions;
};

struct GameDebugGlobals {
    ACDID         atDebugUpdatedACDID[8];
    uint32        uDebugUpdateACDIDIndex;
    SSceneID      atDeactivatedSceneID[8];
    SGameID       atDeactivateSceneIDGames[8];
    uint32        uDebugDeactivatedSceneIndex;
    FileReference tCombatLog;
    BOOL          bLogEnabled;
    ACDID         idACDDebugged;
    CHAR          szACDDebugInfo[1024];
};

struct FrameTimer {
    Blizzard::Time::Microsecond m_tStart;
    Blizzard::Time::Microsecond m_tAccum;
};

struct FrameTimerHistory {
    Blizzard::Time::Microsecond m_atTimes[3];
};

struct TextRenderProperties {
    SNO   snoFont;
    int32 nFontSize;
    float flScale;
    float flAdvanceScalar;
    BOOL  fSnapToPixelCenterX;
    BOOL  fSnapToPixelCenterY;
    float flStereoDepth;
};

struct GBHeader {
    SerializeData serName_Console;
    XPtr64<char>  szName;
    GBID          gbid;
    uint32        dwPad;
};

struct AttributeSpecifier {
    Attrib                eAttribute;
    int32                 nParam;
    XPtr64<unsigned char> pbAttributeFormula;
    SerializeData         serAttributeFormula;
};

typedef int32 AffixType;
typedef int32 AffixGroup;

struct Affix {
    GBID           gbid;
    uint32         dwFlags;
    int32          nFrequencyWeight;
    int32          nClassFrequencyWeight[7];
    int32          nHirelingFrequencyWeight[4];
    int32          nAffixLevelMin;
    int32          nAffixLevelMax;
    GBID           gbidAffixFamily0;
    GBID           gbidAffixFamily1;
    PlayerClass    ePlayerClass;
    GBID           arAllowedTypes[24];
    GBID           arLegendaryAllowedTypes[24];
    GBID           gbidExclusionCategory;
    GBID           arExcludedCategories[6];
    uint32         dwAllowedQualityLevels;
    AffixType      eAffixType;
    GBID           gbidAssociatedAffix;
    ItemEffectType eItemEffectType;
    int32          nItemEffectLevel;
    GBID           gbidConvertsTo;
    AffixGroup     eAffixGroup;
};

struct AffixTableEntry {
    GBHeader           tHeader;
    uint32             dwFlags;
    int32              nFrequencyWeight;
    int32              nClassFrequencyWeight[7];
    int32              nHirelingFrequencyWeight[4];
    int32              nAffixLevelMin;
    int32              nAffixLevelMax;
    int32              nCost;
    int32              nIdentifyCost;
    int32              nNormalRequiredLevel;
    int32              nCrafterRequiredLevel;
    ItemEffectType     eItemEffectType;
    int                nItemEffectLevel;
    GBID               gbidConvertsTo;
    GBID               gbidLegendaryUprankAffix;
    SNO                snoRareNamePrefixStringList;
    SNO                snoRareNameSuffixStringList;
    GBID               gbidAffixFamily0;
    GBID               gbidAffixFamily1;
    PlayerClass        ePlayerClass;
    GBID               gbidExclusionCategory;
    GBID               arExcludedCategories[6];
    GBID               arAllowedTypes[24];
    GBID               arLegendaryAllowedTypes[24];
    uint32             dwAllowedQualityLevels;
    AffixType          eAffixType;
    GBID               gbidAssociatedAffix;
    AttributeSpecifier ptAffixAttributes[4];
    AttributeSpecifier ptRequirements[3];
    AffixGroup         eAffixGroup;
    uint32             dwExplicit64BitPadding;
};

struct ItemType;
struct Item {
    GBID      gbid;
    uint32    dwFlags;
    int32     nItemLevel;
    Act       eMinAct;
    ItemType *ptItemType;
    int32    *pnDropWeights;
    int32    *pnVendorDropWeights;
    uint32   *pdwLabelMask;
    GBID      gbidBaseItem;
    int32     nSeasonRequiredToDrop;
};

typedef blz::vector<ItemType *, blz::allocator<ItemType *>> ItemTypePointerArray;
typedef blz::vector<Item *, blz::allocator<Item *>>         ItemPointerArray;

struct ItemType {
    GBID                 gbid;
    BOOL                 fAlwaysMagic;
    int32                nLootLevelRange;
    int32                nReqCrafterLevelForEnchant;
    uint32               dwFlags;
    uint32              *pdwLabelMask;
    ItemType            *ptParentType;
    ItemPointerArray     arItems;
    ItemTypePointerArray arChildItemTypes;
};

struct ItemAppGlobals {
    ItemType                               tRootItemType;
    int32                                  nItemTypes;
    ItemType                              *ptItemTypes;
    int32                                  nItems;
    Item                                  *ptItems;
    int32                                  nAffixes;
    Affix                                 *ptAffixes;
    AffixTableEntry                      **ptCachedAffixTableEntyPtrs;
    BOOL                                   bCachedAffixTableEntriesValid;
    XGrowableMap<int, int, Item *, Item *> mapItems;
    ChainedFixedBlockAllocator             tLabelMaskAllocator;
    ChainedFixedBlockAllocator             tDropWeightsAllocator;
    _BYTE                                  tGBIDToCurrencyMap[0x20];
    _BYTE                                  tCurrencyToCurrencyInfoMap[0x20];
    _BYTE                                  tCraftedItemGBIDToPowerInfo[0x20];

    // std::unordered_map<int, int, std::hash<int>, std::equal_to<int>, std::pair<const int, int>> tGBIDToCurrencyMap;
    // std::unordered_map<int, int, blz::hash<int>, blz::equal_to<int>, blz::allocator<blz::pair<const int, int>>> tGBIDToCurrencyMap;
    // blz::unordered_map<int, int, blz::hash<int>, blz::equal_to<int>, blz::allocator<blz::pair<const int, int>>> tGBIDToCurrencyMap;
    // blz::unordered_map<int, CurrencyInfo, blz::hash<int>, blz::equal_to<int>, blz::allocator<blz::pair<const int, CurrencyInfo>>>                           tCurrencyToCurrencyInfoMap;
    // blz::unordered_map<int, CraftedLegendaryPowerInfo, blz::hash<int>, blz::equal_to<int>, blz::allocator<blz::pair<const int, CraftedLegendaryPowerInfo>>> tCraftedItemGBIDToPowerInfo;
};

using GBIDPool = XListMemoryPool<int>;
struct GlobalSNOEntry;
using GlobalSNOArray = blz::vector<GlobalSNOEntry, std::allocator<GlobalSNOEntry>>;

struct AppGlobals {
    BOOL                      fInitialized;
    BOOL                      fAppExitEnabled;
    int32                     nGameGfxInitCount;
    uint32                    m_dwTicksPerSec;
    CGame                    *ptCGameActive;
    CGame                    *ptCGamePlaying;
    GameClock                 tSGameClock;
    uint32                    dwDebugFlags;
    uint32                    dwDebugFlags2;
    uint32                    dwDebugFlags3;
    XBitArray<124>            tDrawFlags;
    HardwareClass             eHWClassOverride;
    DebugRenderMode           eDebugRenderMode;
    DebugDisplayMode          eDebugDisplayMode;
    DebugFadeMode             eDebugFadeMode;
    DebugSoundMode            eDebugSoundMode;
    DebugColorBlindMode       eDebugColorBlindMode;
    XBitArray<27>             tHideRenderLayer;
    int32                     nDRLGSeedOverride;
    int32                     nSubSceneOverride;
    float                     flSpeedHack;
    float                     flDebugDrawRadius;
    uint32                    dwDisplayAnimLeaf;
    SNO                       snoPVPWorldOverride;
    uint32                    dwMinFrameMs;
    uint32                    dwFrameCounter;
    float                     flParticleScalar;
    float                     flExplosionMagnitude;
    uint32                    uPerfTraceInterval;
    uint32                    uLastPerfTrace;
    int32                     nTraceCategoryPage;
    _BYTE                     tCollisionDebugData[0x100C];  // typedef tCollisionDebugData
    _BYTE                     tConstraintDebugData[0x44];   // typedef ConstraintDebugData
    FastAttribKey             tKeyDebugAttrib;
    GlobalSNOArray           *ptGlobalSNOs;
    GameClock                 tAppClock;
    uint32                    dwTicksSinceAppStart;
    BOOL                      bInAppIdle;
    BOOL                      bInServerIdle;
    GameParts                 nGameParts;
    uint32                    dwGameCreateParamFlags;
    Stopwatch                 tStartTimeStopwatch;
    StringDebugMode           eStringDebugMode;
    AODebugMode               eAODebugMode;
    BOOL                      fDoScheduledPrecache;
    uint32                    uSleepingReasons;
    BOOL                      bAllowSleeping;
    BOOL                      fFrameOut;
    CHAR                      szFrameOutPath[4096];
    SNO                       snoForceBounty;
    Act                       eForcedDefaultAct;
    uintptr_t                *ptTagMapAppGlobals;     // TagMapAppGlobals
    uintptr_t                *ptInheritedTagGlobals;  // InheritedTagGlobals
    uintptr_t                *ptGBAppGlobals;         // GameBalanceAppGlobals
    uintptr_t                *ptXServerAppGlobals;    // XServerAppGlobals
    uintptr_t                *ptNetLoungeGlobals;     // NetLoungeGlobals
    uintptr_t                *ptNetworkGlobals;       // NetworkGlobals
    uintptr_t                *ptReplayTestGlobals;    // ReplayTestGlobals
    uintptr_t                *ptFastAttribGlobals;    // FastAttribAppGlobals
    Diablo3StorageAppGlobals *ptDiablo3StorageAppGlobals;
    Diablo3NDBAppGlobals     *ptDiablo3NDBAppGlobals;
    uintptr_t                *ptConsole;  // UIConsoleEditGlobals
    UIChatEditGlobals        *ptChatEdit;
    GBIDPool                 *ptGBIDPool;
    uintptr_t                *ptQuestAppGlobals;        // QuestAppGlobals
    uintptr_t                *ptLuaScriptAppGlobals;    // LuaScriptAppGlobals
    uintptr_t                *ptScriptEventAppGlobals;  // ScriptEventAppGlobals
    uintptr_t                *ptUIAppGlobals;           // UIAppGlobals
    uintptr_t                *ptAppearanceGlobals;      // AppearanceGlobals
    ProfUIAppGlobals         *ptProfUIAppGlobals;
    uintptr_t                *ptGameDebugGlobals;       // GameDebugGlobals
    FontGlobals              *ptFontGlobals;
    ItemAppGlobals           *ptItemAppGlobals;
    uintptr_t                *ptPrefetchGlobals;      // DefinitionPrefetchData
    uintptr_t                *ptGlobalDefAppGlobals;  // GlobalDefinitionAppGlobals
    SNOArray                 *ptScriptDefsPurged;
    SNOArray                 *ptConversationDefsPurged;
    SNOArray                 *ptPowerDefsPurged;
    SNOArray                 *ptConductorDefsPurged;
    Blizzard_Lock_Mutex       tScriptDefPurgeMutex;
    Blizzard_Lock_Mutex       tConversationDefPurgeMutex;
    Blizzard_Lock_Mutex       tPowerDefPurgeMutex;
    Blizzard_Lock_Mutex       tConductorDefPurgeMutex;
    Blizzard_Lock_Mutex       tGlobalDisposeServerMutex;
    _BYTE                     tGlobalServerSemaphore[0x28];  // Blizzard::Lock::Semaphore
    RGBAColor                 rgbaGreenscreen;
    float                     flNormalMapFactor;
    uintptr_t                *ptAchievements;           // Console::Achievements
    uintptr_t                *ptAchievementsInterface;  // Console::ClientAchievementsInterface
    BOOL                      bShowSidekickDebug;
    FrameTimer                mainTimer;
    FrameTimer                renderTimer;
    FrameTimerHistory         mainTimerHistory;
    FrameTimerHistory         renderTimerHistory;
    FrameTimerHistory         gpuTimerHistory;
    uint64                    nLastGameClockTime;
    uint64                    nLastGameClockDelta;
};

struct ALIGNED(8) AttribDef {
    Attrib                     eAttrib;
    FastAttribValue            tDefaultValue;
    AttributeParameterType     eParameterType;
    AttributeOperator          eAttributeOperator;
    AttributeDataType          eAttributeDataType;
    LPCSTR                     pszFormulaItem;
    LPCSTR                     pszFormulaCharacter;
    LPCSTR                     pszName;
    const AttributeCompressor *ptCompressor;
    uint8                      uSyncFlags;
};

struct LogoutTimerData {
    uint32 uLogoutTimeTicks;
    BOOL   bAbortable;
    uint32 uGracePeriodTimeTicks;
};

struct RNG {
    uint32 dwSeed;
    uint32 dwCarry;
};

struct ALIGNED(4) PlayerQuestRewardHistoryEntry {
    SNO   snoQuest;
    int32 nStepUID;
    uint8 uDifficultyFlags;
};

struct PlayerQuestHistoryEntry {
    SNO   snoQuest;
    int32 nStepUID;
};

struct PlayerSavedQuest {
    SNO   snoQuest;
    int32 nCurrentStepUID;
    int32 arObjectiveState[20];
    int32 arFailureConditionState[20];
};

struct ALIGNED(4) HeroStateData {
    Act                           eLastPlayedAct;
    Act                           eHighestUnlockedAct;
    uint32                        uPlayerFlags;
    uint32                        unkData;
    PlayerSavedData               tSavedData;
    int32                         nNumQuestRewardEntries;
    PlayerQuestRewardHistoryEntry tQuestRewardHistory[100];
    PlayerQuestHistoryEntry       tQuestHistory[200];
    PlayerSavedQuest              tSavedQuest[64];
    uint32                        uBossKillFlags[6];
};

struct PlayerAccountBanner {
    uint32 uBannerIndex;
    int32  nSigilMain;
    int32  nSigilAccent;
    int32  nPatternColorIndex;
    int32  nBackgroundColorIndex;
    int32  nSigilColorIndex;
    int32  nPlacementIndex;
    int32  nPattern;
    int32  nEpicBanner;
    BOOL   bUseSigilVariant;
};

struct PlayerAccount {
    OnlineService::GameAccountHandle tHandle;
    OnlineService::AccountId         uBnetAccountId;
    CHAR                             usName[128];
    uint32                           uDigestFlags;
    uint32                           uPartitionFlags;
    PlayerAccountBanner              tBanner;
};

struct ALIGNED(8) CurrencyData {
    CurrencyType eType;
    int64        nCount;
    uint32       uFlags;
};

struct ALIGNED(8) CurrencySavedData {
    CurrencyData arCurrency[22];
};

struct ALIGNED(4) CrafterData {
    GBID  gbidRecipes[640];
    int32 nNumRecipes;
    int32 nLevel;
};

struct ALIGNED(4) CrafterTransmogData {
    GBID  gbidUnlockedTransmogs[1600];
    int32 nUnlockedTransmogs;
    GBID  gbidUnlockedAccountTransmogs[64];
    int32 nUnlockedAccountTransmogs;
};

struct ALIGNED(4) ExtractedLegendariesData {
    GBID  gbidExtractedLegendaries[512];
    int32 nExtractedLegendariesCount;
};

struct ALIGNED(4) CrafterSavedData {
    CrafterData              tCrafterData[5];
    CrafterTransmogData      tTransmogData;
    ExtractedLegendariesData tExtractedLegendariesData;
};

struct ALIGNED(8) CosmeticItemData {
    _BYTE tCosmeticItemData[0x1210];
    //   GBID gbidCosmeticItems[512];
    //   int32 nNumCosmeticItems;
    //   GBID gbidPurchasedCosmeticItems[512];
    //   int32 nNumPurchasedCosmeticItems;
    //   LimitedDurationPet tLimitedDurationPets[32];
    //   int32 nNumLimitedDurationPets;
};

struct ALIGNED(8) CosmeticItemSavedData {
    CosmeticItemData tCosmeticItemData;
};

struct ALIGNED(4) ImageTextureFrame {
    SNO   snoTexture;
    int32 nFrame;
};

struct IVector2D {
    int32 x;
    int32 y;
};

struct Vector2D {
    float x;
    float y;
};

struct Rect2D {
    float left;
    float top;
    float right;
    float bottom;
};

struct IRect2D {
    int32 left;
    int32 top;
    int32 right;
    int32 bottom;
};

/* 9401 */
enum CubeMapFace : int32 {
    CUBEMAPFACE_POSX = 0x0,
    CUBEMAPFACE_NEGX = 0x1,
    CUBEMAPFACE_POSY = 0x2,
    CUBEMAPFACE_NEGY = 0x3,
    CUBEMAPFACE_POSZ = 0x4,
    CUBEMAPFACE_NEGZ = 0x5,
    NUM_CUBEMAPFACES = 0x6,
};

struct RequiredMessageHeader {
    uint16          dwSize;
    int8            nControllerIndex;
    int8            nPadding;
    GameMessageType eType;
};

struct PlayerLevelMessage {
    RequiredMessageHeader tHeader;
    int32                 nPlayerIndex;
    int32                 nNewLevel;
};

struct DWordDataMessage {
    RequiredMessageHeader tHeader;
    uint32                dwData;
};

struct IntDataMessage {
    RequiredMessageHeader tHeader;
    int32                 nData;
};

struct PerPlayerGlobals {
    KnownListID        tKnownListCurrent;
    KnownListID        tKnownListPrevious;
    ServerTrickleList *pListPossibleTrickle;
    ServerTrickleList *pListTricklePrev;
    ServerTrickleList *pListTrickle;
    uint32             uExperienceTagId;
    uint32             uSessionFlags;
    _BYTE              tContentLicenses[0x18];  // OnlineService::ContentLicenseArray
    _BYTE              tRawLicenses[0x18];      // OnlineServiceServer::LicenseArray
    uint32             uContentFlags;
    _BYTE              tConsumables[0x18];      // SPlayerConsumables
    uint32             uConsoleLicenses;
    BOOL               bHasFriends;
    float              flDPSDamageTotal;
    _BYTE              listDPSInfo[0x58];            // DPSBucketList
    _BYTE              listRecordedSpeedInfo[0x58];  // SpeedBucketList
    _BYTE              listTickBankBuckets[0x58];    // TickBankBucketList
    _BYTE              tTickRateTracker[0xFC];       // TickRateTracker
    _BYTE              listPings[0x58];              // PingBucketList
    uint32             uPingSentTime;
    float              flHighestPing;
    uint32             tNextAchievementRegisterAttempt;
    uint32             tNextAchievementUnregisterAttempt;
    _BYTE              tRecentAchievementsEarned[0x18];  // AchievementList
    uint32             uLastAchievementEarnedTime;
    int32              nTrickleLoreAchievementIndex;
    int32              nTrickleCosmeticAchievementIndex;
    BOOL               bGracefulDisconnect;
    _BYTE              tDeliveredRewards[0x30];  // DeliveredRewardsSavedData
};

// struct ServerPlayerGlobals {
//     PerPlayerGlobals                                                        tPerPlayer[4];
//     UpdateData                                                              tPlayerUpdateData;
//     XRingBuffer<SessionTrackedData, SessionTrackedData, 8>                  tPreviousSessions;
//     blz::deque<PendingGameSave, blz::allocator<PendingGameSave>>            tPendingSaves;
//     blz::vector<EscrowSaveData, blz::allocator<EscrowSaveData>>             tPendingEscrowSaveData;
//     blz::vector<ConsumableSaveRecord, blz::allocator<ConsumableSaveRecord>> tPendingConsumableSaveRecords;
//     XPooledDataIDList                                                       listVisitedSScenes;
//     XPooledDataIDList                                                       listLoadingACDs;
//     ServerPlayerRegistration                                                tPlayerRegistrationArray[32];
//     SPlayerSaveMap                                                          tPlayerSaveMap;
//     uint32                                                                  dwNextAllPlayersSaveTime;
//     int32                                                                   nPlayerCooldownIndex;
//     PlayerCooldownData                                                      tPlayerCooldownData[16];
//     OnlineService::RewardIdList                                             tRewardHealingList;
//     uint32                                                                  dwNextItemDupeScan;
//     uint32                                                                  uNextPingTime;
//     uint32                                                                  dwNextAvengerSpawnCheck;
//     uint64                                                                  uGameLeaderAccountId;
// };

// NOLINTEND

struct SavePointData {
    SNO   snoWorld;
    int32 nNum;
    BOOL  bCreatesPortal;
    ACDID idACDReturnPortal;
};

using TargetType = int32;

struct Target {
    TargetType eType;
    ACDID      idACDTarget;
    WorldPlace tPlace;
    BOOL       bForcedActorTarget;
};

struct PreplayedAnimData {
    uint32 dwServerTimeAnimStarted;
    uint32 dwSyncedSeed;
    uint32 dwPrePlayedAnimFlags;
};

struct PlayerQueuedPower {
    uint32            dwPowerLastStartTime;
    uint32            dwPowerLastInterruptTime;
    uint32            dwPowerLastEndTime;
    SNO               snoQueuedPower;
    uint32            dwQueuedTime;
    Target            tQueuedTarget;
    int32             nQueuedComboLevel;
    ACDID             idACDQueuedItemBeingUsed;
    PreplayedAnimData tPreplayedAnimData;
    uint32            dwQueuedTargetMessageFlags;
};

struct EnchantAffixChoice {
    GBID   gbidAffix;
    uint32 dwSeed;
};

struct PlayerVendorData {
    ACDID              idACDEnchantAffixPendingItem;
    GBID               gbidEnchantAffixPendingRemoved;
    EnchantAffixChoice tEnchantAffixChoices[6];
    int32              nEnchantAffixChoicesCount;
};

using PetType                = int32;
using PlayerWarpReason       = int32;
using OpenWorldStatus        = int32;
using MonsterRarity          = int32;
using TimerID                = int32;
using UIAction               = int32;
using EInputModes            = int32;
using InputMsg               = int32;
using XPooledDataIDList      = _BYTE[0x28];  // XPooledList<int, int>;
using PlayerTradeData        = _BYTE[0x30];
using CheckpointData         = _BYTE[0x20];
using HearthPortalData       = _BYTE[0x18];
using HirelingStable         = _BYTE[0xE8];
using LoadoutSavedData       = _BYTE[0x240];
using PooledSNONameList      = _BYTE[0x28];
using PlayerStats            = _BYTE[0x268];
using PRTransform            = _BYTE[0x1C];
using ConsoleTargetComponent = _BYTE[0x40];
using PlayerAvengerData      = _BYTE[0x68];
using SkillsUsedStorage      = _BYTE[0x368];
using SnoStoredStates        = _BYTE[0x128];
using TargetLock             = _BYTE[0x2C];
using MoveValidationLog      = _BYTE[0x20];

using SWorldID          = int32;
using DyeType           = int32;
using ItemEffectType    = int32;
using ActorLocationType = int32;

struct VisualItem {
    GBID           gbidItem;
    DyeType        eDyeType;
    ItemEffectType eItemEffectType;
    int32          nEffectLevel;
};

struct VisualCosmeticItem {
    GBID gbidItem;
};

struct VisualEquipment {
    VisualItem         tVisualItem[8];
    VisualCosmeticItem tCosmeticItems[4];
};

struct VisualInventory {
    VisualEquipment tEquipmentCurrent;
    VisualEquipment tEquipmentLast;
};

struct Sphere {
    Vector3D wpCenter;
    float    wdRadius;
};

struct XSphereNode {
    Sphere       m_sphere;
    void        *m_pUserData;
    XSphereNode *m_pParent;
    XSphereNode *m_listChildren[2];
};

struct AxialCylinder {
    Vector3D wpBase;
    float    wdHeight;
    float    wdRadius;
};

struct WorldLocation {
    PRTransform   transform;
    float         flScale;
    Sphere        wsBounds;
    AxialCylinder wcylBounds;
    XSphereNode  *ptSphereNode;
    SWorldID      idSWorld;
    SSceneID      idSScene;
};

struct InventoryLocation {
    ACDID             idACDContainer;
    InventorySlotType eSlot;
    IVector2D         pt;
};

struct ActorLocation {
    ActorLocationType eActorLocation;
    WorldLocation     tWorldLocation;
    InventoryLocation tInvLocation;
};

using GizmoType      = int32;
using ActorType      = int32;
using GBIDPooledList = XPooledList<int, const int &>;

struct ActorCommonData {
    ACDID                 id;
    uint32                tName;
    int32                 nRefCount;
    uint32                dwFlags;
    ACDNetworkName        ann;
    DataID                idOwner;
    SNO                   snoActor;
    SNOName               tSNOName;
    uint32                dwKnownBits;
    uint32                dwPlayerSeenBits;
    uint32                dwPlayerLastSeenTime;
    uint32                dwHitGroundTime;
    uint32                dwLastSeen;
    GBHandle              hGB;
    MonsterRarity         eMonsterRarity;
    ActorLocation         tLocation;
    AttribGroupID         idFastAttrib;
    AttribGroupID         idPassiveAttribGroup;
    AttribGroupID         idEnhancementAttribGroup;
    AttribGroupID         idUpscaledAttribGroup;
    ActorInventory       *ptInventory;
    VisualInventory      *ptVisualInventory;
    OnlineService::HeroId idAssignedHero;
    GameConnectionID      idAssignedGameConnection;
    GBIDPooledList        listIdentifiedAffixGBIDs;
    GBIDPooledList        listUnidentifiedAffixGBIDs;
    RareItemName          tRareItemName;
    GizmoType             eGizmoType;
    ActorType             eActorType;
    float                 flHitPointsCurrent;
    Team                  eTeamOverride;
    Team                  eTeam;
    HirelingClass         eHirelingClass;
    int8                  fNeverTargetable : 1;
    int8                  fIsUntargetable : 1;
    int8                  fIsStealthed : 1;
    int32                 nPetOwner : 3;
    int8                  fIsNPC : 1;
    int8                  fIsDisabled : 1;
    int8                  fIsAlive : 1;
    int8                  fIsCharmed : 1;
    int8                  fIsStunned : 1;
    int8                  fHasAssignedHeroID : 1;
    int8                  fHasSpawner : 1;
    FastAttribGroup      *ptFastAttribGroup;
    GBID                  pRareNameGBIDs[2];
    GBID                  pMonsterAffixGBIDs[8];
    RNG                  *ptRNG;
    uint32                dwModifiedBits;
    //   TKnownMessageList listACDMessages;
    //   ComplexEffectIDListPooled tListComplexEffects;
    //   BuffIDListPooled listBuffs;
    //   AnimPlayer *ptAnimPlayer;
    //   ResolvedPortalDestination *ptResolvedDest;
    //   AvengerData *ptAvengerData;
    //   FriendGiftData *ptFriendGiftData;
    //   uint32 dwTimerCount;
    //   ACDPackID idACDPack;
    //   float flShear;
    //   uint8 nLookLinkIndex;
    //   POS posGroupPrimary;
    //   uint32 uGroupPrimary;
    //   POS posGroupSecondary;
    //   uint32 uGroupSecondary;
    //   uint8 nScriptState;
    //   uint8 uAttributeSyncMask;
    //   ACDCollSettings tCollSettings;
    //   XSphereNode *ptCollSphereNode;
    //   XSphereNode *ptCollBlockerSphereNode;
    //   SWorldID idCollSWorld;
    //   uint32 uHeroTagBitfield;
    //   XGrowableMap<int,int,int,int> tClientEffects;
};

using ACDIDArray = blz::vector<ACDID, std::allocator<ACDID>>;

struct PlayerClientInfo {
    uint32            dwNameDisplayEnd;
    uint32            dwFadeOutTime;
    uint32            dwLastItemMoveProcessed;
    uint32            dwMinChanneledDurationTime;
    uint32            dwTimeLastComboAttack;
    uint32            dwInterruptedAnimCompleteTime;
    uint32            dwTimeLastNonPreplayedAnimatingPower;
    uint32            dwTimeInputWasCached;
    uint32            dwNextTimeCanSayImFull;
    uint32            dwClosingAbortTime;
    uint32            dwDownEventBits;
    uint32            dwGameTickOfLastHeldEvent;
    int32             nComboLevel;
    int32             nKillsSinceMoving;
    uint32            dwGameTickOfLastAction[98];
    uint32            dwGameTickOfLastActionSecondaryPowerCast[98];
    uint32            dwGameTickOfLastHeldAction[98];
    uint32            dwGameTickOfLastActionOtherListener[98];
    SkillsUsedStorage GameTickOfLastHeldPower;
    UIAction          nUIAction;
    EInputModes       nInputMode;
    InputMsg          eCurrentMessage;
    ACDID             idACDCurrentTarget;
    ACDNetworkName    ann;
    SnoStoredStates   tCachedSnoStates;
    SNO               snoCurrentPower;
    SNO               snoCurrentAnimPower;
    SNO               snoCurrentOriginalAnimPower;
    SNO               snoCurrentChannelledPower;
    SNO               snoCurrentClientWalkPower;
    SNO               snoCurrentLoopingPower;
    SNO               snoLastUsedPower;
    SNO               snoLastCursorQueuedPower;
    BOOL              bWantToCancelChanneledPower;
    BOOL              fSuppressVendorCloseNotification;
    BOOL              bClosing;
    BOOL              bQueuedHighlightForceUpdate;
    BOOL              bPlayerIsLocal;
    BOOL              bJoinRequested;
    BOOL              bUpscaled;
    RNG               rngSyncedAnim;
    TargetLock        tCachedTargetLock;
    TargetLock        tTargetLock;
    IVector2D         ptLastMouse;
    IVector2D         ptLastHeldEventMousePos;
    using ItemMoveQueue = _BYTE[0x58];
    ItemMoveQueue listQueuedItemMoves;
};

struct ALIGNED(8) Player {
    int32                 nPlayerIndex;
    ACDID                 idPlayerACD;
    RActorID              idPlayerRActor;
    BannerRenderData      tBannerRenderData;
    Team                  eTeam;
    Team                  eJoinTeam;
    GameConnectionID      idGameConnection;
    LogoutTimerData       auLogoutTimerData[2];
    int32                 nPlayerKills;
    int32                 nDeaths;
    int32                 nAssists;
    int32                 nTeamWins;
    uint32                nValidationErrors[3];
    ActorID               idActorTownPortalInUse;
    uint64                nTeamId;
    float                 teamRating;
    float                 teamVariance;
    float                 individualRating;
    float                 individualVariance;
    uint32                nLastRewardChallengeNumber;
    D3::Items::Mail       tChallengeMail;
    int32                 nChallengeBestTimesMilliseconds[4];
    BOOL                  bCompletedSoloRift;
    RNG                   rngSyncedAnim;
    HeroStateData         tHeroStateData;
    CrafterSavedData      tCrafterSavedData;  // [0x5434];       // CrafterSavedData    // _BYTE[0x38]   tUnknownPadding;         // Offset adjust +0x38 puts tAccount @ 0xB148
    CurrencySavedData     tCurrencySavedData;
    CosmeticItemSavedData tCosmeticItemSavedData;
    PlayerAccount         tAccount;
    OnlineService::HeroId uHeroId;
    CHAR                  usHeroName[49];
    CHAR                  szHeroFilename[32];
    uint32                dwJoinTime;
    uint32                dwNextStatTime;
    uint32                dwNextAttributeUpdateTime;
    uint32                dwLastInputTime;
    ACDID                 idACDActiveVendor;
    CrafterType           eActiveCrafterType;
    SNO                   snoLastResidualFootstep;
    uint32                dwLastResidualFootstepTime;
    WorldPlace            placeCheckedForActivatingScenes;
    XPooledDataIDList     listVisibleACDs;
    XPooledDataIDList     listPrefetchableACDs;
    XPooledDataIDList     listDestinationsToPrefetch;
    BOOL                  fRefreshWaypointDestination;
    PlayerTradeData       tTradeData;
    struct PlayerQuestData {
        SSceneID idSSceneLast;
    } tQuestData;
    struct PlayerTalkData {
        ACDID idACDTalkingTo;
    } tTalkData;
    ACDID            idACDPlayerToInspect;
    CheckpointData   tLastCheckpoint;
    HearthPortalData tHearthPortalInfo;
    uint8            arGameFlags[128];
    struct PlayerPet {
        ACDID   idACD;
        PetType eType;
    } tPets[64];
    int32             nMaxUsedPetIndex;
    BOOL              bCosmeticPetMissing;
    HirelingStable    tHirelingStable;
    ACDID             idRequestedHirelingLocation;
    HirelingClass     eRequestedHirelingClass;
    uint32            dwRequestedHirelingTimeStamp;
    LoadoutSavedData  tLoadouts[5];
    SNO               snoActorPortrait;
    float             flHealthPercent;
    SNO               snoAreaLocation;
    SNO               snoLastWaypointLevelArea;
    PlayerClass       ePlayerClassPortrait;
    int32             nCurrentLevel;
    int32             nCurrentAltLevel;
    int32             nCurrentHighestHeroSoloRiftLevel;
    int32             nPortraitAction;
    int32             nSkillQuickSelectId;
    PooledSNONameList listConversations;
    PlayerStats       tStats;
    XLocale           eLocale;
    PlayerWarpReason  eWarpReason;
    float             flWarpFadeInSecs;
    OpenWorldStatus   eOpenWorldStatus;
    BOOL              bSetDungeonActive;
    ACDIDArray        listNecroCorpses;
    uint32            uServerFlags;
    PRTransform       tAVCameraTransform;
    struct
    {
        Act           eLastPlayedAct;
        SNO           snoLastPlayedQuest;
        int32         nLastPlayedQuestStep;
        uint32        uActivatedWaypoints;
        SavePointData tLastSavePoint;
        int32         nHandicapLevelSnapshot;
    } tPlayerResumeStateUponJoining;
    XPooledDataIDList      tLoresUsed;
    uint32                 dwBossEncounterFlags;
    uint32                 dwPrevPortalHash;
    uint32                 dwBossesKilledFlags;
    uint32                 dwNephalemRiftFlags;
    uint32                 dwSetDungeonFlags;
    ACDID                  idACDActiveChallengePortal;
    BOOL                   bChallengeAccepted;
    BOOL                   bOptedInToOpenWorldModeChange;
    BOOL                   bPlayOpenWorldTutorial;
    float                  flPVPPlayerDamageTaken[4];
    ACDID                  idACDStashLastOperated;
    uint32                 dwTickLastVerification;
    WorldPlace             tPlaceLastVerification;
    float                  flMaxSpeedThisVerificationInterval;
    uint32                 dwTickLastSpamCheck;
    uint32                 nMessageCostLast300;
    uint32                 nMessageCostLast600;
    uint32                 nMessageCostLast3600;
    uint32                 nMessageCostLastUpdateTick;
    uint32                 nMessageCostMax300;
    uint32                 nMessageCostMax600;
    uint32                 nMessageCostMax3600;
    PlayerQueuedPower      tQueuedPower;
    uint64                 nKicksInitiated;
    uint64                 nKicksParticipatedIn;
    uint64                 nGamesWithoutKicks;
    uint64                 nTimesVoteKicked;
    uint32                 uEntitledRewardBits;
    uint32                 uOutstandingRewardBits;
    uint32                 dwTimeStampJoined;
    GBHandle               hGBKiller;
    SNO                    snoKiller;
    MonsterRarity          eKillerRarity;
    GBID                   killerRareNameGBIDs[2];
    SNO                    snoKilledIn;
    TimerID                idTimerForcedResurrect;
    TimerID                idTimerCorpseResurrect;
    TimerID                idTimerCorpseResurrectGrantCharge;
    uint32                 dwTickLastResurrection;
    uint32                 uSeasonCreated;
    PlayerVendorData       tVendorData;
    XPooledDataIDList      listOnGroundCraftingPlans;
    int32                  nBusyMessagesSinceNPCInteract;
    TimerID                idTimerLoadoutEquipApplyPassives;
    uint32                 dwServerSidePrefFlags;
    int32                  nControllerIndex;
    BOOL                   bIsPrimaryPlayerServerOnly;
    BOOL                   bIsGuestPlayerServerOnly;
    BOOL                   bIsLocalToServerServerOnly;
    SNO                    snoControllerBinding;
    ConsoleTargetComponent tTargetComponent;
    PlayerAvengerData      tSoloAvenger;
    PlayerAvengerData      tHardcoreAvenger;
    PlayerAvengerData      tFriendAvenger;
    AvengerSource          eAvengerSource;
    uint32                 uConsoleSyncFlags;
    uint32                 uLastPlayedTimeUpdate;
    struct
    {
        uint32 uMonstersKilled;
        uint32 uElitesKilled;
        uint32 uGoldPickedUp;
        uint32 uLootRunsCompleted;
    } tConsoleCounters;
    BOOL                      bHasPerformedFinalSaveForQuit;
    uintptr_t                *ptNewItemManager;            // UIInventory::Console::NewItemIndicatorManager<ACDID>
    uintptr_t                *ptNewEliteItemManager;       // UIInventory::Console::NewItemIndicatorManager<ACDID>
    uintptr_t                *ptNewRecipeManager;          // UIInventory::Console::NewItemIndicatorManager<int>
    uintptr_t                *ptNewCosmeticManager;        // UIInventory::Console::NewItemIndicatorManager<int>
    _BYTE                     tMapVendorSeeds;             // blz::unordered_map<int, blz::pair<unsigned int, unsigned int>, blz::hash<int>, blz::equal_to<int>, blz::allocator<blz::pair<const int, blz::pair<unsigned int, unsigned int>>>>
    _BYTE                     tMapPartitionWideCubeSeeds;  // blz::unordered_map<int, blz::pair<unsigned int, unsigned int>, blz::hash<int>, blz::equal_to<int>, blz::allocator<blz::pair<const int, blz::pair<unsigned int, unsigned int>>>>
    uint32                    uCubeBaseSeed;
    _BYTE                     tChallengeRiftRewards;       // blz::list<D3::Account::ConsoleChallengeRiftReward, blz::allocator<D3::Account::ConsoleChallengeRiftReward>>
    Blizzard::Time::Timestamp tNFPDefaultScanTime;
    Blizzard::Time::Timestamp tNFPGoblinScanTime;
    PlayerClientInfo          tClientInfo;
    MoveValidationLog         arMoveValidationLog[16];
    uint32                    uMoveLogIdx;
    Blizzard::Util::BitField  tSeenTutorials;
    uint8                     bStashIcons[12];
    uint32                    dwLastKickAttempt;
    uint32                    dwLastSuccessfulKick;
    uint64                    uAcceptedLicenseBitsAccount;
    uint64                    uAcceptedLicenseBitsHero;
    uint64                    uGuildId;
    uint8                     bDidFirstTimeSetup : 1;
    uint8                     bIsBeingDeleted : 1;
    uint8                     bParticipatingInVoteKick : 1;
    uint8                     fOptedIntoActTransition : 1;
    uint8                     bForceKnownListUpdate : 1;
    uint8                     bParticipatedInVoteKick : 1;
    uint8                     bForceHirelingItemSave : 1;
    uint8                     bNewX1Player : 1;
};

const RGBAColor crgbaWhite {.r = 0xFF, .g = 0xFF, .b = 0xFF, .a = 0xFF};
const RGBAColor crgbaGray {.r = 0x88, .g = 0x88, .b = 0x88, .a = 0xFF};
const RGBAColor crgbaRed {.r = 0xFF, .g = 0, .b = 0, .a = 0xFF};
const RGBAColor crgbaGreen {.r = 0, .g = 0xFF, .b = 0, .a = 0xFF};
const RGBAColor crgbaBlue {.r = 0, .g = 0, .b = 0xFF, .a = 0xFF};
const RGBAColor crgbaYellow {.r = 0xFF, .g = 0xFF, .b = 0, .a = 0xFF};
const RGBAColor crgbaOrange {.r = 0xFF, .g = 0x7F, .b = 0, .a = 0xFF};
const RGBAColor crgbaPurple {.r = 0xFF, .g = 0, .b = 0xFF, .a = 0xFF};
const RGBAColor crgbaGold {.r = 0xC7, .g = 0xB3, .b = 0x77, .a = 0xFF};
const RGBAColor crgbaUIGold {.r = 0xCD, .g = 0x9D, .b = 0x51, .a = 0xFF};
const RGBAColor crgbaBlack {.r = 0, .g = 0, .b = 0, .a = 0xFF};
const RGBAColor crgbaTan {.r = 0xC8, .g = 0xB4, .b = 0x7C, .a = 0xFF};
const RGBAColor crgbaUIOrange {.r = 0xFF, .g = 0x69, .b = 0x1E, .a = 0xFF};
const RGBAColor crgbaDisabled {.r = 0x66, .g = 0x66, .b = 0x66, .a = 0x88};
const RGBAColor crgbaWatermark {.r = 0xC0, .g = 0xFF, .b = 0xFF, .a = 0x50};
const RGBAColor crgbaWatermarkShadow {.r = 0x00, .g = 0x00, .b = 0x00, .a = 0x28};

using ResizeToFitValue    = int32;
using UIJustifyHorizontal = int32;
using UIJustifyVertical   = int32;

using WindowCreationParams = _BYTE[0x730];

using WindowMode = int32;
struct DisplayMode {
    uint32     dwFlags;
    WindowMode dwWindowMode;
    int32      nWinLeft;
    int32      nWinTop;
    int32      nWinWidth;
    int32      nWinHeight;
    uint32     dwUIOptWidth;
    uint32     dwUIOptHeight;
    uint32     dwWidth;
    uint32     dwHeight;
    int32      nRefreshRate;
    uint32     dwBitDepth;
    uint32     dwMSAALevel;
    float      flAspectRatio;
};

struct VariableResRWindowData {
    uint32 dwMinUs;
    uint32 dwMaxUs;
    uint32 dwAdjustCooldown;
    float  flMinPercent;
    float  flMaxPercent;
    uint32 dwVariableResFrameIndex;
    float  flPercent;
    float  flPercentIncr;
    uint32 dwAvgHistoryUs;
    uint32 VariableResHistoryUs[8];
};

struct RWindow {
    _BYTE                   FILLER[0x2BC];
    VariableResRWindowData *m_ptVariableResRWindowData;
};

/* 34 */
using SNOGroup = int32;

/* 4410 */
struct SNOVersions {
    uint32 dwVersions[5];
};

/* 12408 */
enum SNODataType : int32 {
    SNODATATYPE_TITLE    = 0x0,
    SNODATATYPE_USERDATA = 0x1,
    NUM_SNODATATYPES     = 0x2,
};

using GroupDefinition = uintptr_t;
/* 12430 */
struct SNOGroupInfo {
    CHAR             szName[32];
    SNOGroup         snoGroup;
    SNOVersions      tVersions;
    uint32           dwFlags;
    SNODataType      eSNODataType;
    GroupDefinition *pdefGroup;
};

using StringLabelHandle = uint32;

struct StringTableEntry {
    XPtr64<char>      szText;
    SerializeData     serText;
    StringLabelHandle hLabel;
    uint32            dwPad;
};

struct RequiredDefinitionHeader {
    SNO            snoHandle;
    volatile int32 nLockCount;
    volatile int32 dwFlags;
};

using StringTableMap = uintptr_t;
struct StringListDefinition {
    RequiredDefinitionHeader tHeader;
    uint32                   dwStringCount;
    XPtr64<StringTableEntry> arStrings;
    SerializeData            serStrings;
    StringTableMap          *ptMapStringTable;
};

struct Signature {
    char  szName[64];
    char  szComment[64];
    uint8 nMonth;
    uint8 nDay;
    uint8 nYear;
};

struct VideoPrefsToConfirm {
    BOOL        bVsync;
    int32       nMipOffset;
    int32       nShadowQuality;
    DisplayMode tDisplayMode;
    BOOL        bLargeCursor;
};

struct DisplayCaps {
    uint32 dwFlags;
    uint32 dwMaxVertexStreams;
    uint32 dwMaxAnisotropy;
    uint32 dwMaxSimultaneousRTs;
};

using RenderState = _BYTE[0x1F28];
using ShaderTex   = int32;

struct TextureBinding {
    int32 nStage;
};

struct PerfBinStatsFrame {
    int32  nDrawCalls;
    uint32 dwPolys;
    uint32 dwEstStaticTextureUsage;
};

struct GfxPerfStatsFrame {
    int32             nVBSwitches;
    int32             nShaderSwitches;
    int32             nTextureSwitches;
    int32             nBatchedTextureSwitches;
    uint32            dwVBMemUsage;
    PerfBinStatsFrame tBinStats[6];
};

struct GfxPerfStats {
    uint32            dwVBMemUsage;
    uint32            dwIBMemUsage;
    GfxPerfStatsFrame tCurrent;
    GfxPerfStatsFrame tLast;
};

using RenderTarget = _BYTE[0x8];

struct RenderWorkerData {
    RenderState     tRenderState;
    RenderState     tRenderStateLast;
    ShaderTex       eShaderTex[24];
    TextureBinding *ptTexBinding;
    GfxPerfStats    tStats;
    RenderTarget    tCurrentRTs[2];
    uint32          dwRTHeight;
    uint32          dwRTWidth;
    float           flRTHeight;
    float           flRTWidth;
    float           flRTInvHeight;
    float           flRTInvWidth;
    SNO             snoCurrentDepthTarget;
    uint32          dwNumBatches;
    float           flClipPlane[24];
    /* ... */
};

struct GfxRenderData {
    _BYTE str0[0x16C];  // 168
};

using VBID = int32;

using TriangleList2D = _BYTE[0x10];

struct GfxInternalData {
    uint32           dwFlags;
    DisplayMode     *ptModes;
    uint32           dwModeCount;
    BOOL             fTextureFormatValid[3][44];
    DisplayMode      tCurrentMode;
    DisplayCaps      tCaps;
    GfxRenderData    tRenderData[2];
    VBID             vbidLineList2D;
    VBID             vbidLineList3D;
    VBID             vbidPointList3D;
    VBID             vbidTriOverlay;
    VBID             vbidTriList2D;
    TriangleList2D  *ptTriList2D;
    RenderWorkerData workerData[1];
    GfxPerfStats     tGlobalStats;
    // GfxOptions                            tGfxOptions;
    // uint32                                dwPSConstantsSize;
    // uint32                                dwVSConstantsSize;
    // TChainedDataArray<IndexBuffer, int>  *ptIBList;
    // TChainedDataArray<VertexBuffer, int> *ptVBList;
    // XGrowableList<int, int>               tDynIBList;
    // XGrowableList<int, int>               tDynVBList;
    // Blizzard::Lock::Mutex                 tIBListMutex;
    // Blizzard::Lock::Mutex                 tVBListMutex;
    // uint32                                dwNumPresents;
    // float                                 flModeHeight;
    // float                                 flModeWidth;
    /* ... */
};

struct ALIGNED(8) SigmaThreadLocalStorage {
    Blizzard::Thread::TLSSlot *m_ptTlsSlot;
    uint32                     m_nTlsIndex;
};

struct ReturnPointData {
    SNO   snoWorld;
    int32 nNum;
    ACDID idACDReturnPortal;
};

struct MatchmakingData {
    uint32 dwGameMadePublicTick;
    uint32 dwLastPlayerLeaveTick;
    BOOL   bNephalemValorBuffGranted;
    BOOL   bMatchmakingServerState;
    CHAR   szGameTag[16];
    CHAR   szServerPool[16];
};

using ExternalGameSignals = std::array<_BYTE, 0xC0>;
using DungeonFinderData   = std::array<_BYTE, 0x50>;
using UpdateData          = std::array<_BYTE, 0x20>;
using TPacketBufferList   = std::array<_BYTE, 0x28>;
using SGamePrivateGlobals = uintptr_t;

struct ALIGNED(4) DungeonFinderOverrides {
    SNO                     snoDungeonFinderEngine;
    int32                   nSeed;
    SNO                     snoMasterTileset;
    std::array<SNO, 10>     arSnoTilesets;
    uint32                  dwMasterPopulationHash;
    std::array<uint32, 10>  arPopulationHashes;
    SNO                     snoDungeonFinderBossOverride;
    int32                   nMaxFloorSize;
    int32                   nWeeklyChallengeRiftLevel;
    int32                   nWeeklyChallengeCompletionMilliseconds;
    int32                   nWeeklyChallengeTargetMilliseconds;
    float                   flOrbValueMultiplier;
    BOOL                    bUsePylonOverrides;
    std::array<_BYTE, 0x50> arPylonOverrides;
};

struct ALIGNED(4) ChallengeRiftData {
    uint32                 nChallengeNumber;
    uint8                  bChallengeOverriden;
    std::array<_BYTE, 0x3> _pad0;
    DungeonFinderOverrides tDungeonFinderOverrides;
    int32                  nCompletionMilliseconds;
    uint32                 nEndTime;
    int32                  nInstigatorIndex;
};

struct ALIGNED(8) SGameGlobals {
    uintptr_t                *ptAchievementsInterface;  // OnlineService::AchievementsInterface
    ExternalGameSignals       atExternalSignals[2];
    ExternalGameSignals      *ptActiveSignals;
    ExternalGameSignals      *ptInactiveSignals;
    uint32                    dwFlags;
    uint32                    dwFlags2;
    uint32                    dwGameHash;
    uint32                    dwGameStartAppTime;
    uint32                    dwLastAppTimeWithPlayer;
    uint32                    dwLastAppTimeInput;
    uint32                    dwUniqueGameKey;
    uint32                    dwGameEventNumber;
    Blizzard::Time::Timestamp tUTCGameStartTime;
    Stopwatch                 tStopwatch;
    RNG                       rng;
    RNG                       rngSyncedAnim;
    int32                     nBoostLevel;
    int32                     nUpdatesLeftOver;
    int32                     nMaxPlayers;
    int32                     nMonstersKilled;
    SNO                       snoInitialWorld;
    SNO                       snoInitialQuest;
    int32                     nInitialQuestStepUID;
    uint32                    uEntranceGUIDOverride;
    BOOL                      bResumeFromSave;
    SavePointData             tCreatorSavePoint;
    ReturnPointData           tReturnPoint;
    ACDID                     idACDReturnPointPortal;
    MatchmakingData           tMatchmakingData;
    DungeonFinderData         tDungeonFinderData;
    uint32                    uNextDynamicEntranceGUID;
    SNO                       snoOpenWorldHubWorld;
    uint32                    uOpenWorldHubEntranceGUID;
    UpdateData                tOpenWorldUpdateData;
    SGamePrivateGlobals      *ptSGamePrivate;
    CHAR                      szGameName[4096];
    BOOL                      bGameCreatorInfoSet;
    OnlineService::HeroId     uHeroIdCreator;
    CHAR                      uszCreatorHeroName[49];
    CHAR                      uszCreatorAccountName[128];
    uint32                    dwActivatedWaypoints;
    std::array<_BYTE, 0xD78>  _unk13A0;
    // SNOPooledList                         listCompletedBossEncounters;
    // uintptr_t                             uDatabaseSerializeToken;
    // uint32                                dwNextBNetGameUpdate;
    // uint32                                dwPlayerCounter;
    // uint32                                dwLastChampRareUniqueKilled;
    // uint32                                uTicksSinceLastMessage;
    // uint32                                uPandemoniumFlags;
    // int32                                 nLargestPVPScoreDeficitTeam0;
    // int32                                 nLargestPVPScoreDeficitTeam1;
    // DPSTrackingInfo                       tDPSTrackingInfo;
    // GamePerformanceTrackingInfo           tPerformanceTrackingInfo;
    // uint32                                uNextServerVariableCheckTick;
    // uint32                                uNextServerPartyGuideCheckTick;
    // PauseMessage                          pauseMessages[10];
    // uint32                                dwPauseMessageDropCount;
    // uint32                                dwLoggingOutMessageDropCount;
    // char                                  szLogHistory[10][80];
    // uint32                                nLogHistoryCurrentIndex;
    // Blizzard::Time::Millisecond           tSimTimes[512];
    // Blizzard::Time::Millisecond           tSimTimesSpikes;
    // Blizzard::Time::Millisecond           tSimTimeSum;
    // uint32                                tSimTimePos;
    // ConversationVarMap                    tConversationVarMap;
    // ConversationRandomLineMap             tConversationRandomLineMap;
    // uint8                                 dwKeyWardenKilledFlags;
    // mapPendingCCMessages                  tMapPendingCCMessages;
    Blizzard::Time::Timestamp tAchievementTime;
    ACDID                     idGreedPortal;
    float                     flBlizzconRiftTier1RewardDropChance;
    float                     flBlizzconRiftTier2RewardDropChance;
    uint32                    _unk212C;
    UpdateData                tCommunityBuffsUpdateData;
    uint32                    dwLastTimeVerboseSlowFrameLog;
    uint32                    dwTimeBetweenVerboseSlowFrameLogs;
    uint32                    dwVerboseSlowFrameLogMaxPowersPerLog;
    uint32                    _unk215C;
    TPacketBufferList         tOrphanedPacketBufferList;
    ChallengeRiftData         tChallengeRift;
    // float                                 flMonsterHPMult;
    // float                                 flMonsterATKMult;
    // float                                 flMonsterDEFMult;
    // float                                 flMonsterDMGMult;
    // uint32                                uDebugDrawNextID;
    // int32                                 nDRLGSeedOverride;
    // int32                                 nItemSeedOverride;
    // int32                                 nDeathOverride;
    // int32                                 nPopulationOverride;
    // CheatType                             eActiveCheat;
    // uint32                                uNextCheatUpdateTime;
    // int32                                 nTry;
    // SNO                                   snoTestScene;
    // SNO                                   snoTestWorld;
    // GBID                                  gbidForcedMonsterAffix;
    // float                                 flDamageNormalizeFactor;
    // float                                 flPvPDamageCritChanceFactor;
    // float                                 flPvPDamageCritDamageFactor;
    // float                                 flPvPDamageElementalDamageFactor;
};

struct GameSyncedData {
    uint32                dwGameSyncedFlags;
    Act                   eAct;
    int32                 nInitialMonsterLevel;
    int32                 nMonsterLevel;
    int32                 nRandomWeatherSeed;
    OpenWorldMode         eOpenWorldMode;
    Act                   eOpenWorldModeAct;
    int32                 nOpenWorldModeParam;
    uint32                dwOpenWorldTransitionTime;
    Act                   eOpenWorldDefaultAct;
    Act                   eOpenWorldBonusAct;
    SNO                   snoDungeonFinderLevelArea;
    BOOL                  bLootRunOpen;
    int32                 nOpenLootRunLevel;
    BOOL                  bLootRunBossDead;
    int32                 nHunterPlayerIdx;
    BOOL                  bLootRunBossActive;
    BOOL                  bTieredLootRunFailed;
    BOOL                  bLootRunChallengeCompleted;
    BOOL                  bSetDungeonActive;
    Team                  arPlayerAllyTeam[4];
    Team                  arMonsterTeam[4];
    Team                  eSceneryTeam;
    Team                  eNeutralTeam;
    BOOL                  bPregame;
    uint32                dwPregameEnd;
    uint32                dwRoundStart;
    uint32                dwRoundEnd;
    BOOL                  bPVPGameOver;
    BOOL                  bPVPAwardsAssigned;
    uint32                nTeamWins[2];
    uint32                nTeamScore[2];
    PVPGameResult         ePVPGameResult[2];
    OnlineService::HeroId uPartyGuideHeroId;
    uint64                arTieredRiftParticipatingHeroID[4];
};

struct AttributeGroupGlobals;
struct AttributeListGlobals;
struct AttributeGlobals;
struct PathfindingGlobals;

using MarkerGlobals         = uintptr_t;
using InactiveMarkerGlobals = uintptr_t;
using TSnoNameListPool      = std::array<_BYTE, 0x30>;
using TimeMapPool           = std::array<_BYTE, 0x30>;

struct PlayerGlobals {
    TimeMapPool      poolTimeMap;
    TSnoNameListPool poolSNOName;
    Player           tPlayerArray[4];
};

struct ALIGNED(8) GameCommonData {
    DifficultyLevel        eDifficultyLevel;
    int32                  nHandicapLevel;
    uint32                 dwDifficultyChangedFlags;
    uint32                 nNumPlayers;
    GameType               eGameType;
    GameTemplate           tTemplate;
    GameSyncedData         tGameSyncedData;
    GameClock              tGameClock;
    uint32                 dwServerTime;
    int                    nGameFlagsSize;
    uint8                 *arGameFlags;
    MarkerGlobals         *ptMarkerGlobals;
    InactiveMarkerGlobals *ptInactiveMarkerGlobals;
    PlayerGlobals         *ptPlayerGlobals;
    // SceneCommonGlobals          *ptSceneCommonGlobals;
    // ItemGlobals                 *ptItemGlobals;
    // NavMeshGlobals              *ptNavMeshGlobals;
    // NavMeshStitchGlobals        *ptNavMeshStitchGlobals;
    // AttributeGroupGlobals       *sgptAttributeGroupGlobals;
    // AttributeListGlobals        *sgptAttributeListGlobals;
    // AttributeGlobals            *sgptAttributeGlobals;
    // FastAttribGameGlobals       *ptFastAttribGameGlobals;
    // TeamGlobals                 *ptTeamGlobals;
    // PathGameGlobals             *ptPathGlobals;
    // ACDGlobals                  *ptACDGlobals;
    // ActorCollisionGlobals       *ptActorCollisionGlobals;
    // ACDCollGlobals              *ptACDCollGlobals;
    // GameBalanceGlobals          *ptGBGlobals;
    // QuestGlobals                *ptQuestGlobals;
    // AnimPlayerGlobals           *ptAnimPlayerGlobals;
    // PathfindingGlobals          *ptPathFindingGlobals;
    // NavCellPathGlobals          *ptNavCellPathGlobals;
    // ActorInventoryGlobals       *ptActorInventoryGlobals;
    // VisualInventoryGlobals      *ptVisualInventoryGlobals;
    // PortalGlobals               *ptPortalGlobals;
    // ItemWeightInfo              *arItemWeights;
    // GameTestingGlobals          *ptGameTestingGlobals;
    // CombatLog::CombatLogGlobals *ptCombatLogGlobals;
    // Tutorial::TutorialGlobals   *ptTutorialGlobals;
    // ActGlobals                  *ptActGlobals;
    // ChallengeRiftGlobals        *ptChallengeRiftGlobals;
    // CHAR                        *szLootTraceBuffer;
    // ReplayGlobals               *ptReplayGlobals;
    // GameMode                     eMode;
};

struct ALIGNED(8) SNOPooledList {
    XPooledList_Sub<int, int, XListMemoryPool<int>> m_list;
    BOOL                                            m_ConstructorCalled;
};

struct ALIGNED(4) InactiveMarkerOwner {
    SNO    snoMarkerSet;
    int16  nMarkerIndex;
    uint16 nAdventureParentIndex;
    uint32 nMarkerSetInstanceIndex;
};

struct ALIGNED(8) InventoryId {
    OnlineService::GameAccountHandle m_tGameAccount;
    OnlineService::HeroId            m_uHeroId;
};

using Angle       = float;
using LevelAreaID = int32;
using ActorBrain  = int32;

struct ALIGNED(8) Actor {
    using ActorAI           = uintptr_t;
    using ActorPowers       = uintptr_t;
    using TimedEffects      = uintptr_t;
    using ActorProjectile   = uintptr_t;
    using SActorMovement    = uintptr_t;
    using StaggerDamageInfo = uintptr_t;

    ActorID               id;
    RNG                   rndOriginal;
    uint32                tName;
    ACDID                 idACD;
    SNO                   snoActor;
    SNO                   snoMonster;
    SNOPooledList         listSNOScripts;
    SNO                   snoConvList;
    Angle                 aYaw;
    Vector3D              wvVelocity;
    SSceneID              idCurrentScene;
    LevelAreaID           idCurrentLevelArea;
    uint32                uCurrNavMeshFlags;
    ActorAI              *ptAI;
    int32                 nAttackerCount;
    int32                 nAttackerSlotsUsed;
    int32                 nAttackerLargeSlotsUsed;
    ActorBrain            eBrain;
    ActorPowers          *ptActorPowers;
    CHAR                  szPowerState[256];
    TimedEffects         *ptTimedEffects;
    ActorProjectile      *ptProjectile;
    uint32                dwActorFlags;
    SNO                   snoCurrentAnim;
    int32                 nCurrentAnimPermutationIndex;
    float                 flCurrentAnimSpeedScale;
    Vector3D              wvCurrentAnimVelocity;
    UpdateData            tAttrUpdateData;
    UpdateData            tGizmoUpdateData;
    UpdateData            tAreaDamageUpdateData;
    uint32                dwTimeCreated;
    uint32                dwLastAttributeUpdateTime;
    int32                 nAttributeUpdateBucketIndex;
    uint32                dwLastAreaDamageUpdateTime;
    int32                 nAreaDamageBucketIndex;
    uint32                dwTrickleBits;
    uint32                dwTimeSinceLastRecoilUpdate;
    TimerID               idDeathTimer;
    int32                 nSourceSceneUID;
    InactiveMarkerOwner   tMarkerOwner;
    SActorMovement       *ptSActorMovement;
    StaggerDamageInfo    *ptStaggerDamageInfo;
    InventoryId           tItemInventoryId;
    OnlineService::ItemId tItemId;
    uint32                dwItemInitSeed;
    uint32                dwItemIdentifySeed;
    uint32                dwSeasonCreated;
};

struct SharedLooteeParams {
    // int32 nLevel;
    int32 nForcedILevel;
    int32 nTieredLootRunLevel;
    int32 nAncientChanceHandicapLevelOverride;
    BOOL  fIsVendor;
    BOOL  fIsTreasureBag;
    BOOL  fIsCrafter;
    BOOL  bSuppressMagicAndGoldFind;
    BOOL  bSuppressUnluckyBonus;
    BOOL  bSuppressTieredRiftBonuses;
    BOOL  bDisableTormentDrops;
    BOOL  bDisableTormentCheck;
    BOOL  bIgnoreLevelRestriction;
    BOOL  bIgnoreClassLegendaryDropWeight;
    BOOL  bIgnoreKnownRecipeCheck;
    BOOL  bForceCrafterAncientLegendaryChance;
    BOOL  bDisableAncientDrops;
    BOOL  bDisablePrimalAncientDrops;
    BOOL  bDisableNewCraftingDrops;
    BOOL  bSuppressGiftGeneration;
};

struct LootSpecifier {
    SNO                snoActorForFlippy;
    GBHandle           hGBItem;
    uint32             nGoldAmount;
    float              flGoldMultiplier;
    ItemQualityLevel   eIQL;
    int32              nAffixLevel;
    int32              nLegendaryItemLevel;
    PlayerClass        eTargetedPlayerClass;
    HirelingClass      eTargetedHirelingClass;
    SharedLooteeParams tLooteeParams;
    BOOL               fAutoIdentify;
    GBID               arAffixGBIDs[6];
    int32              nNumAffixes;
    uint64             uDesiredStackCount;
    int32              nMinLegendaryAffixes;
    BOOL               fAccountBound;
    BOOL               fBindOnEquip;
    GBID               gbidAllowedLegendaryAffixFamily;
    _BYTE              _gap[0x28];
    AncientRank        eAncientRank;
    BOOL               bAddExtraMajorAffix;
    BOOL               bIgnoreGoldRange;
    BOOL               bTryStack;
};

using ItemLabel = int32;

struct ItemSpecifierData {
    GBID  gbidItem;
    int32 nNumAffixes;
    GBID  gbidAffixes[6];
    int32 nAdditionalRandomAffixes;
    int32 nAdditionalRandomAffixesDelta;
    int32 nMinLegendaryAffixes;
    BOOL  bAccountBound;
    int32 nAdditionalMajorAffixes;
    int32 nAdditionalMinorAffixes;
};

struct LootDropModifier {
    LootDropType      eDropType;
    SNO               snoSubTreasureClass;
    float             flChance;
    int32             nWeight;
    GBID              gbidQualityClass;
    ItemLabel         nItemLabel;
    ItemLabel         nItemLabel2;
    SNO               snoCondition;
    ItemSpecifierData tSpecificItem;
    int32             nQuantity;
    int32             nQuantityDelta;
    uint32            dwItemLabelMask[5];
    BOOL              bTryStack;
    BOOL              bBossSpecificDrop;
    float             flGoldMultiplier;
    BOOL              bIgnoreGoldRanges;
};

struct Quaternion {
    Vector3D wvImag;
    float    wdReal;
};

struct ResolvedPortalDestination {
    SNO    snoWorld;
    uint32 uEntranceGUID;
    SNO    snoDestLevelArea;
};

struct dmBodyId {
    int32  index;
    uint16 revision;
    uint16 worldIndex;
};

struct dmWorldId {
    uint16 index;
};

struct UniqueMonsterActorEntry {
    SNO   snoMonsterActor;
    SNO   snoLevelArea;
    int32 nDRLGIndex;
};

using HeroIdListPooled = XPooledList<unsigned long, unsigned long>;
struct ALIGNED(8) SScene {
    using SceneCommonData = _BYTE[0x2B8];
    using SceneScriptList = _BYTE[0x58];
    SSceneID          id;
    SceneCommonData   tSCD;
    LevelAreaID       arLevelAreaIds[4];
    XPooledDataIDList listACDInScene;
    SceneScriptList   listScriptsInScene;
    HeroIdListPooled  listSeenPlayerIDs;
    dmBodyId          bodyId;
    BOOL              bHasWaypoint;
    PRTransform       transWaypoint;
    SNO               snoWaypointTexture;
    float             fWaypointMaxRange;
    float             fWaypointMinRange;
    float             fWaypointDiscoveryRange;
    BOOL              bIsVista;
};

struct TownPortalLocation {
    WorldPlace wpPortal;
    SNO        snoScene;
};

struct ReturnPortalLocation {
    WorldPlace placeReturn;
    SSceneID   idSScene;
};

struct EntranceLocation {
    SSceneID idSScene;
    SNO      snoMarker;
    uint32   tName;
};
using EntranceLocationMap = XGrowableMap<unsigned int, unsigned int, EntranceLocation, EntranceLocation &>;

struct SavePointLocation {
    WorldPlace placeSavePoint;
    SNO        snoMarker;
    uint32     tName;
};
using SavePointLocationMap = XGrowableMap<unsigned int, unsigned int, SavePointLocation, SavePointLocation &>;

struct SpawnGroupLocation {
    int32 nDRLGIndex;
    int32 nNodeCount;
};
struct SpawnGroupExactInstance {
    SpawnGroupLocation tLocation;
    SNO                snoLevelAreaOrig;
    SNO                snoLevelAreaSpawn;
    int32              nPopIndex;
    int32              nGroupIndex;
};
using TPooledSpawnGroupExactInstanceList = XPooledList<SpawnGroupExactInstance, const SpawnGroupExactInstance &>;

using WorldCreatedCallback = void (*)(SWorldID, ACDID, ACDID, uint32, PlayerWarpReason);

struct SWorld {
    using WorldCommonData          = _BYTE[0x70];
    using NoSpawnTree              = _BYTE[0x48];
    using WorldCreatedCallbackList = _BYTE[0x58];
    SWorldID                           id;
    WorldCommonData                    tWCD;
    SNO                                snoWorldScript;
    uint32                             dwFlags;
    EntranceLocationMap                tEntranceMap;
    SavePointLocationMap               tSavePortalMap;
    TPooledSpawnGroupExactInstanceList listSpawnGroupExact;
    TPooledSpawnGroupExactInstanceList listSpawnGroupFailed;
    ActorID                            idActorEntrancePortal;
    uint32                             dwModifiedBits;
    int32                              nSeed;
    NoSpawnTree                        tNoSpawnTree;
    int32                              eCreateStage;
    WorldCreatedCallback               pfCreatedCallback;
    Vector2D                           vExtMin;
    Vector2D                           vExtMax;
    SNO                                snoBossEncounter;
    SNO                                snoChallenge;
    BOOL                               bChallengeMonstersCleared;
    WorldPlace                         tChallengeCheckpointPlace;
    Quaternion                         qChallengeCheckpointOrientation;
    int32                              nMonstersRemaining;
    BOOL                               bCheckMonstersRemaining;
    SNO                                snoWorldMonsterPower;
    SNO                                snoWorldPlayerPower;
    StringLabelHandle                  hWorldQuestDescriptionLabel;
    StringLabelHandle                  hWorldQuestTitleLabel;
    ResolvedPortalDestination          tDynamicReturnLocation;
    dmWorldId                          worldId;
    TownPortalLocation                 tTownPortalLocation;
    ReturnPortalLocation               tReturnPortalLocation;
    uint32                             dwDeleteTime;
    uint32                             dwTimeWithoutBeingVisited;
    uint64                             arAllowedPlayers[4];
    Blizzard::Time::Millisecond        dwPreloadingStart;
    BOOL                               fPreloadingStarted;
    BOOL                               fPreloadingDungeonFinderStarted;
    BOOL                               fWaitForLoadingScreen;
    SWorld::WorldCreatedCallbackList   tExtraWorldCreatedCallbacks;

    // XPooledMap<unsigned int, unsigned int, Minimap::DiscoveryInfo, const Minimap::DiscoveryInfo &, 16> mapUndiscoveredMinimapMarkers;
    _BYTE                                                                 mapUndiscoveredMinimapMarkers[0x28];
    XPooledList<int, int>                                                 listEventSNOs;
    XPooledList<UniqueMonsterActorEntry, const UniqueMonsterActorEntry &> listUniqueMonsterActorEntries;
};

struct PendingJoinMap : XGrowableMapTS<unsigned long, OnlineService::PlayerResolveData> {};
using GameAccountKickMap = XGrowableMapTS<OnlineService::GameAccountHandle, int>;

struct SGame {
    using MapData                   = uintptr_t;
    using ActorGlobals              = uintptr_t;
    using SpawnerGlobals            = uintptr_t;
    using PowerGlobals              = uintptr_t;
    using ActorPowerGlobals         = uintptr_t;
    using TimedEffectsGlobals       = uintptr_t;
    using ActorAIGlobals            = uintptr_t;
    using BrainComponentGlobals     = uintptr_t;
    using BrainStanceGlobals        = uintptr_t;
    using BrainTranslateGlobals     = uintptr_t;
    using PowerEffectGlobals        = uintptr_t;
    using ActorCollisionGlobals     = uintptr_t;
    using ActorProjectileGlobals    = uintptr_t;
    using FlippyGlobals             = uintptr_t;
    using HeroGlobals               = uintptr_t;
    using LevelAreaGlobals          = uintptr_t;
    using PlayerListGlobals         = uintptr_t;
    using STalkGlobals              = uintptr_t;
    using ServerPlayerGlobals       = uintptr_t;
    using KnownEntryGlobals         = uintptr_t;
    using TrickleGlobals            = uintptr_t;
    using ConsoleGlobals            = uintptr_t;
    using ServerGlobals             = uintptr_t;
    using TimersGlobals             = uintptr_t;
    using SSceneGlobals             = uintptr_t;
    using SWorldGlobals             = uintptr_t;
    using WorldState                = uintptr_t;
    using SActGlobals               = uintptr_t;
    using SGameStatGlobals          = uintptr_t;
    using AdventureGlobals          = uintptr_t;
    using SComplexEffectGlobals     = uintptr_t;
    using STimedEventGlobals        = uintptr_t;
    using SActTransitionGlobals     = uintptr_t;
    using ConversationMgrGlobals    = uintptr_t;
    using BossEncounterState        = uintptr_t;
    using NephalemRiftState         = uintptr_t;
    using SetEncounterState         = uintptr_t;
    using VoteKickState             = uintptr_t;
    using AIStateGameGlobals        = uintptr_t;
    using SActorMovementGameGlobals = uintptr_t;
    using MiniMapMarkerGlobals      = uintptr_t;
    using LuaScriptGameGlobals      = uintptr_t;
    using ConductorGlobals          = uintptr_t;
    using ScriptEventGameGlobals    = uintptr_t;
    using ClientValidationGlobals   = uintptr_t;
    using TagMapPools               = uintptr_t;
    SGameID                                   id;
    SGameSimState                             eSimState;
    uint32                                    uPendingGameUpdateCount;
    uint32                                    uScheduledGameUpdateCount;
    GameParams                                tStartParams;
    SGameGlobals                             *ptSGameGlobals;
    GameCommonData                            tGCD;
    MapData                                  *ptMap;
    ActorGlobals                             *ptActorGlobals;
    SpawnerGlobals                           *ptSpawnerGlobals;
    PowerGlobals                             *ptPowerGlobals;
    ActorPowerGlobals                        *ptActorPowerGlobals;
    TimedEffectsGlobals                      *ptTimedEffectsGlobals;
    ActorAIGlobals                           *ptActorAIGlobals;
    BrainComponentGlobals                    *ptBrainComponentGlobals;
    BrainStanceGlobals                       *ptBrainStanceGlobals;
    BrainTranslateGlobals                    *ptBrainTranslateGlobals;
    PowerEffectGlobals                       *ptPowerEffectGlobals;
    ActorCollisionGlobals                    *ptActorCollisionGlobals;
    ActorProjectileGlobals                   *ptActorProjectileGlobals;
    FlippyGlobals                            *ptFlippyGlobals;
    HeroGlobals                              *ptHeroGlobals;
    LevelAreaGlobals                         *ptLevelAreaGlobals;
    PlayerListGlobals                        *ptPlayerListGlobals;
    STalkGlobals                             *ptSTalkGlobals;
    ServerPlayerGlobals                      *ptServerPlayerGlobals;
    KnownEntryGlobals                        *ptKnownEntryGlobals;
    TrickleGlobals                           *ptTrickleGlobals;
    ConsoleGlobals                           *ptConsoleGlobals;
    ServerGlobals                            *ptServerGlobals;
    TimersGlobals                            *ptTimersGlobals;
    TDataArray<SScene, int>                  *ptSScenes;
    SSceneGlobals                            *ptSSceneGlobals;
    TDataArray<SWorld, int>                  *ptSWorlds;
    SWorldGlobals                            *ptSWorldGlobals;
    WorldState                               *ptWorldStateGlobals;
    SActGlobals                              *ptSActGlobals;
    SGameStatGlobals                         *ptSGameStatGlobals;
    AdventureGlobals                         *ptAdventureGlobals;
    SComplexEffectGlobals                    *ptSComplexEffectGlobals;
    STimedEventGlobals                       *ptSTimedEventGlobals;
    SActTransitionGlobals                    *ptSActTransitionGlobals;
    ConversationMgrGlobals                   *ptConversationMgrGlobals;
    BossEncounterState                       *ptBossEncounterState;
    NephalemRiftState                        *ptNephalemRiftState;
    SetEncounterState                        *ptSetEncounterState;
    VoteKickState                            *ptVoteKickState;
    AIStateGameGlobals                       *ptAIStateGlobals;
    SActorMovementGameGlobals                *ptSActorMovementGlobals;
    MiniMapMarkerGlobals                     *ptSMiniMapGlobals;
    LuaScriptGameGlobals                     *ptLuaScriptGlobals;
    ConductorGlobals                         *ptConductorGlobals;
    ScriptEventGameGlobals                   *ptScriptEventGameGlobals;
    ClientValidationGlobals                  *ptClientValidationGlobals;
    RefStringMemPool                         *ptRefStringMemPool;
    TagMapPools                              *ptTagMapPools;
    XListMemoryPool<int>                     *ptDATAIDMemPool;
    XListMemoryPool<const XSphereNode *>     *ptSphereNodeMemPool;
    XListMemoryPool<int>                     *ptGBIDMemPool;
    XListMemoryPool<SpawnGroupExactInstance> *ptSpawnGroupExactPool;
    GameDebugGlobals                         *ptGameDebugGlobals;
    XMemoryAllocatorInterface                *ptMemoryInterface;
    PendingJoinMap                            tPendingJoinMap;
    GameAccountKickMap                        tKickMap;
};

struct ALIGNED(8) ActiveGameCommonDataGlobals {
    GameCommonData *ptActiveGCD;
    BOOL            bClientActive;
};

struct ALIGNED(8) ThreadGlobals {
    using DRLGDungeonGlobals         = uintptr_t;
    using DRLGGlobals                = uintptr_t;
    using LuaScriptThreadGlobals     = uintptr_t;
    using NavMeshStitchThreadGlobals = uintptr_t;
    using ACDCollThreadGlobals       = uintptr_t;
    using SNONameThreadGlobals       = uintptr_t;
    using SGame                      = uintptr_t;
    using XCommandBuffer             = uintptr_t;
    using DataIDMemoryPool           = uintptr_t;
    using z_streamp                  = u64;
    ActiveGameCommonDataGlobals *m_ptActiveGameCommonData;
    DRLGDungeonGlobals          *m_ptDRLGDungeonGlobals;
    DRLGGlobals                 *m_ptDRLGGlobals;
    LuaScriptThreadGlobals      *m_ptLuaScriptThreadGlobals;
    NavMeshStitchThreadGlobals  *m_ptNavMeshStitchThreadGlobals;
    ACDCollThreadGlobals        *m_ptACDCollThreadGlobals;
    SNONameThreadGlobals        *m_ptSNONameThreadGlobals;
    int                          m_nPowerFormulaStackCount;
    SGameID                      m_idUtilityGame;
    WorldPlace                   m_tUtilityPlace;
    SGameID                      m_idSGameCurrent;
    SGame                       *m_ptSGameCurrent;
    BOOL                         m_bSimulatingGame;
    uint8                       *m_pauEncodeBuffer;
    uint8                       *m_pauDecodeBuffer;
    z_streamp                    m_ptDeflateZstream;
    z_streamp                    m_ptInflateZstream;
    uint8                       *m_pauCompressionBuffer;
    XCommandBuffer              *m_pSpriteThreadBuffer;
    DataIDMemoryPool            *m_pDataIDMemoryPool;
    ThreadIndex                  m_nThreadIndex;
};
