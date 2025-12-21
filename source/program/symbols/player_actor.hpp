FUNC_PTR(0x933ED0, PlayerIsPrimary,                        BOOL  (*)(ACDID idPlayer));
FUNC_PTR(0x93DAF0, GetPrimaryPlayer,                      ACDID  (*)());
FUNC_PTR(0x0B8740, LocalPlayerGet,                      Player*  (*)());
FUNC_PTR(0x51FC40, PlayerGetByACD,                      Player*  (*)(const ACDID idACD));
FUNC_PTR(0x51CFF0, PlayerGetHeroDisplayName,         CRefString  (*)(const Player *tPlayer, BOOL fDisplayPound));  // @ CRefString *PlayerGetHeroDisplayName(CRefString *__return_ptr __struct_ptr retstr, const Player *tPlayer, BOOL fDisplayPound)
FUNC_PTR(0x51F7C0, PlayerGetFirstAll,                   Player*  (*)());
FUNC_PTR(0x51F870, PlayerGetNextAll,                    Player*  (*)(const Player *ptPlayer));
FUNC_PTR(0x5202C0, PlayerGetFirstInGame,                Player*  (*)());
FUNC_PTR(0x5211F0, PlayerGetByPlayerIndex,              Player*  (*)(int32 nPlayerIndex));
FUNC_PTR(0x5279E0, GetPrimaryPlayerForGameConnection,   Player*  (*)(GameConnectionID idGameConnection));
FUNC_PTR(0x7A6330, PlayerList_ctor,                        void  (*)(PlayerList *, TPlayerPointerPool *pPool));  // @ void PlayerList::PlayerList(PlayerList *this, TPlayerPointerPool *pPool)
FUNC_PTR(0x7A6520, PlayerListGetElementPool, TPlayerPointerPool* (*)());
FUNC_PTR(0x7A6BF0, PlayerListGetSingle,                    void  (*)(PlayerList *listPlayers, const Player *tPlayer));
FUNC_PTR(0x7A6850, PlayerListGetAllInGame,                 void  (*)(PlayerList *listPlayers, const int32 nPlayerIndexIgnore, const BOOL bIgnoreHardcoreDeadPlayers, const GameConnectionID idGameConnectionToMatch));
FUNC_PTR(0x7FD3F0, SPlayerIsLocal,                         BOOL  (*)(const Player *ptPlayer));
FUNC_PTR(0x46FAB0, ACD_AttributesGetInt,                  int64  (*)(ActorCommonData *tACD, FastAttribKey tKey));
FUNC_PTR(0x46FAC0, ACD_AttributesGetFloat,                float  (*)(ActorCommonData *tACD, FastAttribKey tKey));
FUNC_PTR(0x46FB90, ACD_AttributesSetInt,                   void  (*)(ActorCommonData *tACD, FastAttribKey tKey, int32_t nValue));
FUNC_PTR(0x46FB10, ACD_AttributesSetFloat,                 void  (*)(ActorCommonData *tACD, FastAttribKey tKey, float flValue));
FUNC_PTR(0x47FF40, ACD_ModifyCurrencyAmount,               BOOL  (*)(ActorCommonData *tACD, const int64 nAmount, const CurrencyType eCurrencyType, const GoldModifiedReason eReason, const BOOL bForceNoSound));
FUNC_PTR(0x4801C0, ACD_SetCurrencyAmount,                  void  (*)(ActorCommonData *tACD, const int64 nAmount, const CurrencyType eCurrencyType, const GoldModifiedReason eReason));
FUNC_PTR(0x669BB0, ACDTryToGet,                ActorCommonData*  (*)(const ACDID idACD));
FUNC_PTR(0x69F4E0, FastAttribGetValueInt,                 int64  (*)(const FastAttribGroup *ptAttribGroup, FastAttribKey tKey));
FUNC_PTR(0x69F580, FastAttribGetValueFloat,               float  (*)(FastAttribGroup *ptAttribGroup, FastAttribKey tKey));
FUNC_PTR(0x69AF10, KeyGetDataType,                        int64  (*)(FastAttribKey tKey));
FUNC_PTR(0x6A0150, KeyGetParamType,                       int64  (*)(FastAttribKey tKey));
FUNC_PTR(0x71DA90, MessageSendToClient,                    void  (*)(const PlayerList *listDestinations, int32 eType, void *pMessage, uint32 dwMessageSize));
FUNC_PTR(0x86DF70, ActorGet,                             Actor*  (*)(const ActorID idActor));
FUNC_PTR(0x86F840, ActorFlagSet,                           void  (*)(const ActorID idActor, const ActorFlag eFlag, const BOOL bValue));
