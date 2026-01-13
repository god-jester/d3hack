FUNC_PTR(GameActiveGameCommonDataIsServer,        BOOL (*)());
FUNC_PTR(GameServerCodeLeave_Tracked,          SGameID (*)(BOOL bReturnToClient));
FUNC_PTR(_GameServerCodeEnter,                    BOOL (*)());
FUNC_PTR(GameIsStartUp,                           BOOL (*)());
FUNC_PTR(ServerIsLocal,                           BOOL (*)());
FUNC_PTR(GetPrimaryProfileUserIndex,             int32 (*)());
FUNC_PTR(SGameGlobalsGet,                SGameGlobals* (*)());
FUNC_PTR(GameGetParts,                       GameParts (*)());
FUNC_PTR(ServerGetOnlyGameConnection, GameConnectionID (*)());
FUNC_PTR(AppServerGetOnlyGame,                 SGameID (*)());
FUNC_PTR(bdLogSubscriber_publish,                 void (*)(const void *, const bdLogMessageType type, const bdNChar8 *const, const bdNChar8 *const file, const bdNChar8 *const, const bdUInt line, const bdNChar8 *const msg));
FUNC_PTR(CmdLineGetParams,              CmdLineParams* (*)());
FUNC_PTR(AppDrawFlagSet,                          void (*)(int32 nFlag, BOOL bValue));
FUNC_PTR(XVarString_Set,                          bool (*)(XVarString *, const char *pszNewVal, uint32 uDirtyFlagsIfNecessary));
FUNC_PTR(FileReadIntoMemory,                     void* (*)(FileReference *tFileRef, uint32 *dwSize, const ErrorCode eErrorCodeIfFails));
FUNC_PTR(FileReferenceInit,                       void (*)(FileReference *tFileRef, LPCSTR szPath));
FUNC_PTR(FileCreate,                              BOOL (*)(FileReference *tFileRef));
FUNC_PTR(FileExists,                              BOOL (*)(const FileReference *tFileRef));
FUNC_PTR(FileOpen,                                BOOL (*)(FileReference *tFileRef, uint32 dwAccessFlags));
FUNC_PTR(FileClose,                               void (*)(FileReference *tFileRef));
FUNC_PTR(FileWrite,                               BOOL (*)(FileReference *tFileRef, const void *lpBuf, const uint32 dwOffset, const uint32 dwSize, ErrorCode eErrorCodeIfFails));
FUNC_PTR(sReadFile,                               BOOL (*)(const char *szFileName, unsigned char **pData, uint32 *nDataSize));
FUNC_PTR(TraceInternal_Log,                       void (*)(const SeverityLevel eLevel, const uint32 dwFlags, const OutputStreamType eStream, LPCSTR szFormat, ...));
FUNC_PTR(GetLocalStorage,                        void* (*)(const Blizzard::Thread::TLSSlot *slot));  // Blizz::Thread::GetLocalStorage
FUNC_PTR(GetGameMessageTypeAndSize,               BOOL (*)(GameMessageType eMessageType, const XTypeDescriptor **pTypeDescriptor, uint32 *dwSize, const char **pszMessageLabel));
FUNC_PTR(DisplaySeasonEndingMessage,              void (*)(int32 nMin));
FUNC_PTR(DisplayLongMessageForPlayer,             void (*)(const CRefString *sMessage, const Player *ptPlayer, const ImageTextureFrame *tImage, BOOL bUseLargeIcon));
FUNC_PTR(DisplayGameMessageForAllPlayers,         void (*)(LPCSTR szStringDecoratedLabel, int nParam1, int nParam2));
FUNC_PTR(GfxWindowChangeDisplayMode,              void (*)(const DisplayMode *tMode));
FUNC_PTR(ImageTextureFrame_ctor,                  void (*)(ImageTextureFrame *, LPCSTR szDecoratedLabel));
FUNC_PTR(FormatTruncatedNumber,             CRefString (*)(float flValue, BOOL bUseMillions, BOOL bIncludeSpecialChar));
FUNC_PTR(XVarBool_ToString,                blz::string (*)(uintptr_t * /* XVarBool *this */));
FUNC_PTR(XVarUint32_ToString,              blz::string (*)(uintptr_t * /* XVarUint32 *this */));
FUNC_PTR(XVarBool_Set,                            bool (*)(uintptr_t * /* XVarBool *this */, bool bNewVal, uint32 uDirtyFlagsIfNecessary));
FUNC_PTR(XVarUint32_Set,                          bool (*)(uintptr_t * /* XVarUint32 *this */, uint32 uNewVal, uint32 uDirtyFlagsIfNecessary));
FUNC_PTR(XVarFloat_Set,                           bool (*)(uintptr_t * /* XVarFloat *this */, float fNewVal, uint32 uDirtyFlagsIfNecessary));
FUNC_PTR(OnSeasonsFileRetrieved,                  void (*)(Console::Online::LobbyServiceInternal *, int32 eResult, blz::shared_ptr<blz::string> *pszFileData));
FUNC_PTR(OnConfigFileRetrieved,                   void (*)(Console::Online::LobbyServiceInternal *, int32 eResult, blz::shared_ptr<blz::string> *pszFileData));
FUNC_PTR(OnBlacklistFileRetrieved,                void (*)(Console::Online::LobbyServiceInternal *, int32 eResult, blz::shared_ptr<blz::string> *pszFileData));
FUNC_PTR(StorageGetPublisherFile,                 void (*)(int32 nUserIndex, const char *szFilename, Console::Online::StorageGetFileCallback pfnCallback));
// FUNC_PTR(OnChalRiftFileReceived, void (*)(Console::Online::StorageResult, D3::ChallengeRifts::ChallengeData const&, D3::Leaderboard::WeeklyChallengeData const&));  // 0x185F70
// FUNC_PTR(sOnGetChallengeRiftData, void (*)(int, Console::Online::StorageResult, D3::ChallengeRifts::ChallengeData /* const */ &, D3::Leaderboard::WeeklyChallengeData /* const */ &));  // 0x185F70
FUNC_PTR(sOnGetChallengeRiftData,                 void (*)(void *bind, Console::Online::StorageResult *param1, D3::ChallengeRifts::ChallengeData &param2, D3::Leaderboard::WeeklyChallengeData &param3));
// FUNC_PTR(sOnGetChallengeRiftData, void (*)(void *bind, Console::Online::StorageResult *param1, D3::ChallengeRifts::ChallengeData *param2, D3::Leaderboard::WeeklyChallengeData *param3));  // 0x185F70
FUNC_PTR(ParsePartialFromString,                  bool (*)(google::protobuf::MessageLite *, const blz::string *data));
FUNC_PTR(ChallengeData_ctor,                      void (*)(D3::ChallengeRifts::ChallengeData *));
// FUNC_PTR(CData_ctor, void (*)(uintptr_t *_this));  // 0x564C00
FUNC_PTR(WeeklyChallengeData_ctor,                void (*)(D3::Leaderboard::WeeklyChallengeData *));
FUNC_PTR(GameRandRangeInt,                       int32 (*)(const int32 nMin, const int32 nMax));

//
FUNC_PTR(rawfree,                                 void (*)(void *));
FUNC_PTR(SigmaMemoryNew,                         void* (*)(size_t size, size_t align, XMemoryAllocatorInterface *ptMemoryInterface, const BOOL fClear));
FUNC_PTR(SigmaMemoryFree,                         void (*)(void *lp, XMemoryAllocatorInterface *ptMemoryInterface));
FUNC_PTR(SigmaMemoryMove,                         void (*)(void *lpDest, void *lpSrc, size_t size));
FUNC_PTR(op_delete_1,                             void (*)(void *));
FUNC_PTR(op_delete_2,                             void (*)(void *));
FUNC_PTR(blz_basic_string,                        void (*)(blz::string *, char const*, blz::allocator<char> const&));  //(const char *s, const blz::allocator<char> *alloc));
FUNC_PTR(blz_string_ctor,                  blz::string (*)(char const*, blz::allocator<char> const&));         //(const char *s, const blz::allocator<char> *alloc));
FUNC_PTR(blz_make_stringf,                 blz::string (*)(char const* fmt,...));
FUNC_PTR(blz_make_stringfb,                blz::string (*)(void *,...));

// Localization helpers (2.7.6):
// - Game locale is stored in "unicode_text_current_locale" (XLocale).
// - XLocaleToString converts XLocale -> locale string (e.g. "enUS").
FUNC_PTR(XLocaleToString,                         LPCSTR (*)(uint32));
