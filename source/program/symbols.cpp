#include "symbols/base.hpp"

// hooks.hpp - Hook sites
#include "symbols/hooks.inc"
SETUP_FUNC_PTR(main_init);
SETUP_FUNC_PTR(gfx_init);
SETUP_FUNC_PTR(scheck_gfx_ready);
SETUP_FUNC_PTR(game_common_data_init);
SETUP_FUNC_PTR(shell_initialize);
SETUP_FUNC_PTR(sgame_initialize);
SETUP_FUNC_PTR(sinitialize_world);
SETUP_FUNC_PTR(cmd_line_parse);
SETUP_FUNC_PTR(var_res_label);
SETUP_FUNC_PTR(move_speed);
SETUP_FUNC_PTR(attack_speed);
SETUP_FUNC_PTR(floating_dmg);
SETUP_FUNC_PTR(font_string_get_rendered_size);
SETUP_FUNC_PTR(font_string_draw_03ff50);
SETUP_FUNC_PTR(font_string_draw_03e5b0);
SETUP_FUNC_PTR(font_string_draw_0010c8);
// Season/event hooks are defined via game.hpp below (OnConfigFileRetrieved, OnSeasonsFileRetrieved, OnBlacklistFileRetrieved)
SETUP_FUNC_PTR(lobby_service_create);
SETUP_FUNC_PTR(lobby_service_idle_internal);
SETUP_FUNC_PTR(specifiers_from_modifier);
SETUP_FUNC_PTR(eval_mod);
SETUP_FUNC_PTR(dupe_dropped_item);
SETUP_FUNC_PTR(request_drop_item);
SETUP_FUNC_PTR(print_challenge_rift_failed);
SETUP_FUNC_PTR(challenge_rift_callback);
SETUP_FUNC_PTR(pubfile_data_hex);
SETUP_FUNC_PTR(print_error_display);
SETUP_FUNC_PTR(print_error_string_final);

// game.hpp
#include "symbols/game.inc"
SETUP_FUNC_PTR(GameActiveGameCommonDataIsServer);
SETUP_FUNC_PTR(GameServerCodeLeave_Tracked);
SETUP_FUNC_PTR(_GameServerCodeEnter);
SETUP_FUNC_PTR(GameIsStartUp);
SETUP_FUNC_PTR(ServerIsLocal);
SETUP_FUNC_PTR(GetPrimaryProfileUserIndex);
SETUP_FUNC_PTR(SGameGlobalsGet);
SETUP_FUNC_PTR(GameGetParts);
SETUP_FUNC_PTR(ServerGetOnlyGameConnection);
SETUP_FUNC_PTR(AppServerGetOnlyGame);
SETUP_FUNC_PTR(bdLogSubscriber_publish);
SETUP_FUNC_PTR(CmdLineGetParams);
SETUP_FUNC_PTR(AppDrawFlagSet);  // void AppDrawFlagSet(int32 nFlag, BOOL bValue)
SETUP_FUNC_PTR(XVarString_Set);
SETUP_FUNC_PTR(FileReadIntoMemory);
SETUP_FUNC_PTR(FileReferenceInit);
SETUP_FUNC_PTR(FileCreate);
SETUP_FUNC_PTR(FileExists);
SETUP_FUNC_PTR(FileOpen);
SETUP_FUNC_PTR(FileClose);
SETUP_FUNC_PTR(FileWrite);
SETUP_FUNC_PTR(sReadFile);
SETUP_FUNC_PTR(TraceInternal_Log);
SETUP_FUNC_PTR(ErrorManagerEnableLocalLogging);
SETUP_FUNC_PTR(ErrorManagerSetLogStreamDirectory);
SETUP_FUNC_PTR(ErrorManagerSetLogStreamFilename);
SETUP_FUNC_PTR(ErrorManagerLogToStream);
SETUP_FUNC_PTR(GetLocalStorage);
SETUP_FUNC_PTR(GetGameMessageTypeAndSize);
SETUP_FUNC_PTR(DisplaySeasonEndingMessage);
SETUP_FUNC_PTR(DisplayLongMessageForPlayer);
SETUP_FUNC_PTR(DisplayGameMessageForAllPlayers);
SETUP_FUNC_PTR(GfxGetDesiredDisplayMode);
SETUP_FUNC_PTR(GfxDeviceReset);
SETUP_FUNC_PTR(GfxForceDeviceReset);
SETUP_FUNC_PTR(GfxWindowChangeDisplayMode);
SETUP_FUNC_PTR(CGameAsyncRenderFlush);
SETUP_FUNC_PTR(CGameAsyncRenderGPUFlush);
SETUP_FUNC_PTR(SetUICConversionConstansts);
SETUP_FUNC_PTR(UI_GfxInit);
SETUP_FUNC_PTR(ImageTextureFrame_ctor);
SETUP_FUNC_PTR(FormatTruncatedNumber);
SETUP_FUNC_PTR(XVarBool_ToString);
SETUP_FUNC_PTR(XVarUint32_ToString);
SETUP_FUNC_PTR(XVarBool_Set);
SETUP_FUNC_PTR(XVarUint32_Set);
SETUP_FUNC_PTR(XVarFloat_Set);
SETUP_FUNC_PTR(OnSeasonsFileRetrieved);
SETUP_FUNC_PTR(OnConfigFileRetrieved);
SETUP_FUNC_PTR(OnBlacklistFileRetrieved);
SETUP_FUNC_PTR(StorageGetPublisherFile);
SETUP_FUNC_PTR(sOnGetChallengeRiftData);
SETUP_FUNC_PTR(ParsePartialFromString);
SETUP_FUNC_PTR(ChallengeData_ctor);
SETUP_FUNC_PTR(WeeklyChallengeData_ctor);
SETUP_FUNC_PTR(GameRandRangeInt);
SETUP_FUNC_PTR(sigma_memory_init);
SETUP_FUNC_PTR(nx64_nvn_heap_early_init);
SETUP_FUNC_PTR(nx64_nvn_heap_add_pool);
SETUP_FUNC_PTR(bdMemory_set_allocate);
SETUP_FUNC_PTR(bdMemory_set_deallocate);
SETUP_FUNC_PTR(bdMemory_set_reallocate);
SETUP_FUNC_PTR(bdMemory_set_aligned_allocate);
SETUP_FUNC_PTR(bdMemory_set_aligned_deallocate);
SETUP_FUNC_PTR(bdMemory_set_aligned_reallocate);
SETUP_FUNC_PTR(bdMemory_alloc);
SETUP_FUNC_PTR(bdMemory_free);
SETUP_FUNC_PTR(bdMemory_realloc);
SETUP_FUNC_PTR(bdMemory_calloc);
SETUP_FUNC_PTR(bdMemory_aligned_alloc);
SETUP_FUNC_PTR(bdMemory_aligned_free);
SETUP_FUNC_PTR(bdMemory_aligned_realloc);
SETUP_FUNC_PTR(rawfree);
SETUP_FUNC_PTR(SigmaMemoryNew);
SETUP_FUNC_PTR(SigmaMemoryFree);
SETUP_FUNC_PTR(SigmaMemoryMove);
SETUP_FUNC_PTR(op_delete_1);
SETUP_FUNC_PTR(op_delete_2);
SETUP_FUNC_PTR(blz_basic_string);
SETUP_FUNC_PTR(blz_string_ctor);
SETUP_FUNC_PTR(blz_make_stringfb);
SETUP_FUNC_PTR(blz_make_stringf);
SETUP_FUNC_PTR(XLocaleToString);
// items.hpp
#include "symbols/items.inc"
SETUP_FUNC_PTR(sGetItemTypeString);                     // void  sGetItemTypeString(GBID a1@<W0>, CRefString *a2@<X8>)
SETUP_FUNC_PTR(GBIDToStringAll);                        // void GBIDToString(unsigned int *a1@<X0>, CRefString *a2@<X8>)
SETUP_FUNC_PTR(GBIDToString);                           // CRefString *GBIDToString(CRefString *retstr, GBID gbid, GameBalanceType eGBType, const BOOL fUsedForXMLFields)
SETUP_FUNC_PTR(AttribParamToString);                    // AttribParamToString(unsigned int attrib@<W0>, int param@<W1>, CRefString *a3@<X8>)
SETUP_FUNC_PTR(FastAttribToString);                     // void FastAttribToString(CRefString *retstr@<X8>, const FastAttribKey tKey@<0:W0>
SETUP_FUNC_PTR(AttribStringToParam);                    // int32 AttribStringToParam(Attrib eAttrib, LPCSTR szParam)
SETUP_FUNC_PTR(StringListFetchLPCSTR);                  // LPCSTR StringListFetchLPCSTR(const SNO snoStringList, LPCSTR szLabel)
SETUP_FUNC_PTR(ClearAssignedHero);                      // ActorCommonData::ClearAssignedHero(ActorCommonData *this)
SETUP_FUNC_PTR(ACDInventoryItemAllowedInSlot);
SETUP_FUNC_PTR(ACDAddToInventory);                      // void ACDAddToInventory(ActorCommonData *tACDItem, const InventoryLocation *tLocation, const BOOL bMarkItemAsDirty, InventorySlotType eCameFrom)
SETUP_FUNC_PTR(ItemHasLabel);
SETUP_FUNC_PTR(SetBindingLevel);                        // ItemSetBindingLevel(ActorCommonData *tACD, const ItemBindingLevel eBindingType)
SETUP_FUNC_PTR(SetRecipient);                           // ItemSetPlayerRecipient(ActorCommonData *tACDItem, int32 nPlayerIndex)
SETUP_FUNC_PTR(PopulateGenerator);                      // ItemGeneratorPopulateFromACDID
SETUP_FUNC_PTR(CItemGenerate);                          // ACDID CItemGenerate(ACDNetworkName ann, const D3::Items::Generator *tGenerator, ItemAffixFastAttribMap *arAffixSpecificAttributes)
SETUP_FUNC_PTR(ItemsGenerator_ctor);                    // D3::Items::Generator::Generator (D3::Items::Generator *this)
SETUP_FUNC_PTR(ItemsGenerator_dtor);                    // D3::Items::Generator::~Generator (D3::Items::Generator *this)
SETUP_FUNC_PTR(AddItemToInventory);                     // SPlayerAddMailItemToInventory
SETUP_FUNC_PTR(GetCosmeticItemType);                    // CosmeticItemType CosmeticItems_GetCosmeticItemTypeFromGBID(GBID gbidItem)
SETUP_FUNC_PTR(ItemIsCosmeticItem);                     // BOOL ItemIsCosmeticItem(const GBID gbidItem)
SETUP_FUNC_PTR(SetCraftLevel);                          // void SetCraftLevel(const CrafterType eType, Player *tPlayer, const int32 nLevel)
SETUP_FUNC_PTR(Crafting_GetTransmogSlot);               // InventorySlotType ItemCrafting_GetTransmogSlot(GBID gbidItem)
SETUP_FUNC_PTR(SCosmeticItems_LearnCosmeticItem);       // BOOL SCosmeticItems_LearnCosmeticItem(const ACDID idACDPlayer, const GBID gbidItem, CosmeticItemType eType, bool bPurchased)
SETUP_FUNC_PTR(SCosmeticItems_LearnPet);                // BOOL SCosmeticItems_LearnPet(const ACDID idACDPlayer, const GBID gbidItem, bool bPurchased)
SETUP_FUNC_PTR(SItemPlayerExtractLegendaryPower);       // BOOL SItemPlayerExtractLegendaryPower(Player *tPlayer, GBID gbidItem, GameError *eGameError)
SETUP_FUNC_PTR(SItemCrafting_TryUnlockTransmog);        // BOOL SItemCrafting_TryUnlockTransmog(GBID gbidTransmog, Player *tPlayer, BOOL bMustBeInInventory, BOOL bSendFullUpdate)
SETUP_FUNC_PTR(SItemCrafting_TryUnlockSecondaryTmogs);  // BOOL SItemCrafting_TryUnlockSecondaryTransmogs(GBID gbidTransmog, Player *tPlayer)
SETUP_FUNC_PTR(SCrafterIncrementLevel);                 // BOOL SCrafterIncrementLevel(Player *tPlayer, CrafterType eType, uint32 nLevels)
SETUP_FUNC_PTR(SItemCrafting_LearnRecipe);              // void SItemCrafting_LearnRecipe(const ACDID idACDPlayer, const GBID gbidRecipe)
SETUP_FUNC_PTR(sCrafterOnLevelUp);                      // void sCrafterOnLevelUp(const Player *tPlayer, CrafterType eType, const BOOL fChangedRank, const BOOL fSendToClient)
SETUP_FUNC_PTR(LootRollForAncientLegendary);            // void LootRollForAncientLegendary(LootSpecifier *tSpecifier, const ACDID idACDLooter)
SETUP_FUNC_PTR(GlobalSNOGet);                           // SNO GlobalSNOGet(const GlobalSNO eGlobalSNO)
SETUP_FUNC_PTR(GBEnumerate);                            // void GBEnumerate(GameBalanceType eType, GBHandleList *listResults)
SETUP_FUNC_PTR(GBGetHandlePool);
// SETUP_FUNC_PTR(ItemGetAttributeFromInventorySlot);
SETUP_FUNC_PTR(SNOToString);
SETUP_FUNC_PTR(ExperienceSetLevel);
SETUP_FUNC_PTR(ExperienceDropLootForAll);
SETUP_FUNC_PTR(LootQuickCreateItem);
SETUP_FUNC_PTR(SACDInventoryPickupOrSpillOntoGround);
SETUP_FUNC_PTR(FlippyFindLandingLocation);
SETUP_FUNC_PTR(FlippyDropCreateOnActor);
SETUP_FUNC_PTR(SItemGenerate);

// player_actor.hpp
#include "symbols/player_actor.inc"
SETUP_FUNC_PTR(PlayerGetHeroDisplayName);

SETUP_FUNC_PTR(PlayerListGetElementPool);
SETUP_FUNC_PTR(PlayerList_ctor);
SETUP_FUNC_PTR(PlayerListGetSingle);
SETUP_FUNC_PTR(PlayerListGetAllInGame);
SETUP_FUNC_PTR(MessageSendToClient);
SETUP_FUNC_PTR(ACDTryToGet);
SETUP_FUNC_PTR(ACD_AttributesGetInt);      // __int64 ActorCommonData::AttributesGetInt(const ActorCommonData *this, FastAttribKey tKey)
SETUP_FUNC_PTR(ACD_AttributesSetInt);      // void ActorCommonData::AttributesSetInt(ActorCommonData *this, FastAttribKey tKey, int32 nValue)
SETUP_FUNC_PTR(ACD_AttributesGetFloat);    // float ActorCommonData::AttributesGetFloat(const ActorCommonData *this, FastAttribKey tKey)
SETUP_FUNC_PTR(ACD_AttributesSetFloat);    // float ActorCommonData::AttributesSetFloat(ActorCommonData *this, FastAttribKey tKey, float flValue)
SETUP_FUNC_PTR(ACD_ModifyCurrencyAmount);  // BOOL ActorCommonData::ModifyCurrencyAmount  (ActorCommonData *this, const int64 nAmount, const CurrencyType eCurrencyType, const GoldModifiedReason eReason, const BOOL bForceNoSound)
SETUP_FUNC_PTR(ACD_SetCurrencyAmount);     // void ActorCommonData::SetCurrencyAmount     (ActorCommonData *this, int64 nAmount, const CurrencyType eCurrencyType, const GoldModifiedReason eReason)
SETUP_FUNC_PTR(FastAttribGetValueInt);     // __int64 FastAttribGetValueInt(const FastAttribGroup *ptAttribGroup, FastAttribKey tKey)
SETUP_FUNC_PTR(FastAttribGetValueFloat);   // float FastAttribGetValueFloat(const FastAttribGroup *ptAttribGroup, FastAttribKey tKey
SETUP_FUNC_PTR(KeyGetDataType);            // __int64 KeyGetDataType(FastAttribKey tKey)
SETUP_FUNC_PTR(KeyGetParamType);           // __int64 KeyGetParamType(FastAttribKey tKey)
SETUP_FUNC_PTR(LocalPlayerGet);            // LocalPlayerGet
SETUP_FUNC_PTR(PlayerGetByACD);            // Player* PlayerGetByACD(const ACDID idACD)
SETUP_FUNC_PTR(GetPrimaryPlayerForGameConnection);
SETUP_FUNC_PTR(ActorGet);                  // Actor* ActorGet(ActorID idActor)
SETUP_FUNC_PTR(ActorFlagSet);              // void ActorFlagSet(const ActorID idActor, const ActorFlag eFlag, const BOOL bValue)
SETUP_FUNC_PTR(PlayerGetFirstAll);
SETUP_FUNC_PTR(PlayerGetNextAll);
SETUP_FUNC_PTR(SPlayerIsLocal);
SETUP_FUNC_PTR(GetPrimaryPlayer);
SETUP_FUNC_PTR(PlayerIsPrimary);
SETUP_FUNC_PTR(PlayerGetFirstInGame);    // Player *__cdecl PlayerGetFirstInGame()
SETUP_FUNC_PTR(PlayerGetByPlayerIndex);  // PlayerGetByPlayerIndex

/* clang-format on */
