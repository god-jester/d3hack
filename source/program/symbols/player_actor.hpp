FUNC_PTR(PlayerIsPrimary,                        BOOL  (*)(ACDID idPlayer));
FUNC_PTR(GetPrimaryPlayer,                      ACDID  (*)());
FUNC_PTR(LocalPlayerGet,                      Player*  (*)());
FUNC_PTR(PlayerGetByACD,                      Player*  (*)(const ACDID idACD));
FUNC_PTR(PlayerGetHeroDisplayName,         CRefString  (*)(const Player *tPlayer, BOOL fDisplayPound));  // @ CRefString *PlayerGetHeroDisplayName(CRefString *__return_ptr __struct_ptr retstr, const Player *tPlayer, BOOL fDisplayPound)
FUNC_PTR(PlayerGetFirstAll,                   Player*  (*)());
FUNC_PTR(PlayerGetNextAll,                    Player*  (*)(const Player *ptPlayer));
FUNC_PTR(PlayerGetFirstInGame,                Player*  (*)());
FUNC_PTR(PlayerGetByPlayerIndex,              Player*  (*)(int32 nPlayerIndex));
FUNC_PTR(GetPrimaryPlayerForGameConnection,   Player*  (*)(GameConnectionID idGameConnection));
FUNC_PTR(PlayerList_ctor,                        void  (*)(PlayerList *, TPlayerPointerPool *pPool));  // @ void PlayerList::PlayerList(PlayerList *this, TPlayerPointerPool *pPool)
FUNC_PTR(PlayerListGetElementPool, TPlayerPointerPool* (*)());
FUNC_PTR(PlayerListGetSingle,                    void  (*)(PlayerList *listPlayers, const Player *tPlayer));
FUNC_PTR(PlayerListGetAllInGame,                 void  (*)(PlayerList *listPlayers, const int32 nPlayerIndexIgnore, const BOOL bIgnoreHardcoreDeadPlayers, const GameConnectionID idGameConnectionToMatch));
FUNC_PTR(SPlayerIsLocal,                         BOOL  (*)(const Player *ptPlayer));
FUNC_PTR(ACD_AttributesGetInt,                  int64  (*)(ActorCommonData *tACD, FastAttribKey tKey));
FUNC_PTR(ACD_AttributesGetFloat,                float  (*)(ActorCommonData *tACD, FastAttribKey tKey));
FUNC_PTR(ACD_AttributesSetInt,                   void  (*)(ActorCommonData *tACD, FastAttribKey tKey, int32_t nValue));
FUNC_PTR(ACD_AttributesSetFloat,                 void  (*)(ActorCommonData *tACD, FastAttribKey tKey, float flValue));
FUNC_PTR(ACD_ModifyCurrencyAmount,               BOOL  (*)(ActorCommonData *tACD, const int64 nAmount, const CurrencyType eCurrencyType, const GoldModifiedReason eReason, const BOOL bForceNoSound));
FUNC_PTR(ACD_SetCurrencyAmount,                  void  (*)(ActorCommonData *tACD, const int64 nAmount, const CurrencyType eCurrencyType, const GoldModifiedReason eReason));
FUNC_PTR(ACDTryToGet,                ActorCommonData*  (*)(const ACDID idACD));
FUNC_PTR(FastAttribGetValueInt,                 int64  (*)(const FastAttribGroup *ptAttribGroup, FastAttribKey tKey));
FUNC_PTR(FastAttribGetValueFloat,               float  (*)(FastAttribGroup *ptAttribGroup, FastAttribKey tKey));
FUNC_PTR(KeyGetDataType,                        int64  (*)(FastAttribKey tKey));
FUNC_PTR(KeyGetParamType,                       int64  (*)(FastAttribKey tKey));
FUNC_PTR(MessageSendToClient,                    void  (*)(const PlayerList *listDestinations, int32 eType, void *pMessage, uint32 dwMessageSize));
FUNC_PTR(ActorGet,                             Actor*  (*)(const ActorID idActor));
FUNC_PTR(ActorFlagSet,                           void  (*)(const ActorID idActor, const ActorFlag eFlag, const BOOL bValue));
