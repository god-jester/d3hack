#pragma once

#include "d3/_util.hpp"
#include "d3/messages.hpp"
#include "nn.hpp"
#include "nn/os/impl/os_timeout_helper.hpp"

namespace d3 {
    inline RareItemName g_tRareItemName = {.fItemNameIsPrefix = 0, .snoAffixStringList = _rarenamestrings_prefix_dex, .nAffixStringListIndex = 16, .nItemStringListIndex = 33};  //33 for goat, 22 for Leo

    inline void AppGlobalFlagDisable(int8 idx, uint32 flag) {
        return;
        switch (idx) {
        case -1:
            g_tAppGlobals.dwDebugFlags  = 0xFFFFFFFF;
            g_tAppGlobals.dwDebugFlags2 = 0xFFFFFFFF;
            g_tAppGlobals.dwDebugFlags3 = 0xFFFFFFFF;
        case 1:
            g_tAppGlobals.dwDebugFlags ^= ~(1 << flag);
        case 2:
            g_tAppGlobals.dwDebugFlags2 ^= ~(1 << flag);
        case 3:
            g_tAppGlobals.dwDebugFlags3 ^= ~(1 << flag);
        default:
            return;
        }
    }

    inline void EnumGBIDUnlocks(GameBalanceType eType, Player *ptSPlayer) {
        ACDID const idPlayerACD = ptSPlayer->idPlayerACD;
        GameError   errorCode   = GAMEERROR_NONE;
        // s32       nCosCount   = 0;
        GBID eItem;

        XBaseListNode<GBHandle> *p_gbH;

        GBHandleList listResults = {};
        memset(static_cast<void *>(&listResults), 0, 20);
        listResults.m_list.m_ptNodeAllocator = reinterpret_cast<XListMemoryPool<GBHandle> *>(GBGetHandlePool());
        listResults.m_ConstructorCalled      = 1;
        GBEnumerate(eType, &listResults);

        if (s32 nCount = listResults.m_list.m_nCount; nCount > 0) {
            // PRINT_EXPR("%d", nCount)
            p_gbH = listResults.m_list.m_pHead;
            while (p_gbH && p_gbH->m_tData.eType == eType) {
                eItem = p_gbH->m_tData.gbid;
                p_gbH = p_gbH->m_pNext;
                // _SERVERCODE_ON
                // if (eItem == 0 || eItem == -1)
                //     continue;
                if (CosmeticItemType const eType = GetCosmeticItemType(eItem); eType != COSMETIC_ITEM_INVALID) {
                    if (eType == COSMETIC_ITEM_PET) {
                        SCosmeticItems_LearnPet(idPlayerACD, eItem, false);
                    } else {
                        SCosmeticItems_LearnCosmeticItem(idPlayerACD, eItem, eType, false);
                    }
                }
                SItemPlayerExtractLegendaryPower(ptSPlayer, eItem, &errorCode);
                SItemCrafting_TryUnlockSecondaryTmogs(eItem, ptSPlayer);
                SItemCrafting_TryUnlockTransmog(eItem, ptSPlayer, 0, 1);
                // auto gbString = d3::GbidStringAll(eItem);
                // PRINT_EXPR("Trying to unlock %i = %s", eItem, gbString)
                // _SERVERCODE_OFF
            }
            PRINT_EXPR("DONE: %d", nCount)
        }
    }

    inline void UnbindACD(ActorCommonData *tACD) {
        if (!_HOSTCHK)
            return;
        _SERVERCODE_ON
        SetRecipient(tACD, -1);
        SetBindingLevel(tACD, IBL_ACCOUNT_PICKUP);
        IntAttribSet(tACD, ITEM_BOUND_TO_ACD, -1, -1);
        IntAttribSet(tACD, ITEM_LOCKED_TO_ACD, -1, -1);
        IntAttribSet(tACD, ITEM_STORE_PLAYER_LOW, 0, -1), IntAttribSet(tACD, ITEM_STORE_PLAYER_HIGH, 0, -1);
        IntAttribSet(tACD, ITEM_ASSIGNED_HERO_LOW, 0, -1), IntAttribSet(tACD, ITEM_ASSIGNED_HERO_HIGH, 0, -1);
        IntAttribSet(tACD, ITEM_INDESTRUCTIBLE);  // just for fun
        ClearAssignedHero(tACD);
        _SERVERCODE_OFF
    }

    inline void FlagsAreFun() {
        g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_ALL_HITS_CRIT_BIT);
        g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_FLYTHROUGH_BIT);
        // g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_FORCE_SPAWN_ALL_GIZMO_LOCATIONS);
        g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_DISPLAY_ALL_SKILLS);
        g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_DISPLAY_ITEM_AFFIXES);
        g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_DISPLAY_ITEM_ATTRIBUTES);
        g_tAppGlobals.dwDebugFlags |= (1 << APP_GLOBAL_ALWAYS_PLAY_GETHIT_BIT);
        g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_DONT_IDENTIFY_RARES_BIT);
        g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_ALWAYS_PLAY_KNOCKBACK_BIT);
        g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_ALL_HITS_OVERKILL_BIT);
        g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_ALL_HITS_CRUSHING_BLOW_BIT);
        g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_DONT_THROTTLE_FLOATING_NUMBERS);
        g_tAppGlobals.dwDebugFlags2 |= (1 << APP_GLOBAL2_DISPLAY_ITEM_TARGETED_CLASS);
        g_tAppGlobals.dwDebugFlags3 |= (1 << APP_GLOBAL3_ANCIENT_ROLLS_WILL_ALWAYS_SUCCEED);
        g_tAppGlobals.dwDebugFlags3 |= (1 << APP_GLOBAL3_ANCIENTS_ALWAYS_ROLL_PRIMAL);
        g_tAppGlobals.dwDebugFlags3 |= (1 << APP_GLOBAL3_LOW_HEALTH_SCREEN_GLOW_DISABLED);
        g_tAppGlobals.dwDebugFlags3 |= (1 << APP_GLOBAL3_CONSOLE_COLLECTORS_EDITION);

        // g_tAppGlobals.dwDebugFlags3 = 0xFFFFFFFF;
    }

    inline void EnableGod() {
        if (!_HOSTCHK)
            return;
        // _SERVERCODE_ON
        ActorCommonData *ptACDCurPlayer = nullptr;
        Actor const     *ptActorPlayer  = nullptr;
        ACDID            idCurPlayer    = ACDID_INVALID;
        // GameError        errorCode      = GAMEERROR_NONE;
        // auto             nTimer         = nn::TimeSpan::FromMilliSeconds(10);
        FlagsAreFun();
        // auto v_items   = AllGBIDsOfType(GB_ITEMS);
        // vector<GBID> arItemQualityClasses;
        // AllGBIDsOfType(GB_ITEM_QUALITY_CLASSES, &arItemQualityClasses);
        // arItemQualityClasses.clear();
        // vector<GBID> arGBIDAll;
        // AllGBIDsOfType(GB_INVALID, &arGBIDAll);
        // arGBIDAll.clear();

        for (auto *ptSPlayer = PlayerGetFirstAll(); ptSPlayer != nullptr; ptSPlayer = PlayerGetNextAll(ptSPlayer)) {
            // nn::os::SleepThread(nTimer);
            if (ptSPlayer->idPlayerACD != ACDID_INVALID) {
                idCurPlayer    = ptSPlayer->idPlayerACD;
                ptACDCurPlayer = ACDTryToGet(idCurPlayer);
                if (ptACDCurPlayer == nullptr)
                    continue;
                ptActorPlayer  = ActorGet(ptACDCurPlayer->idOwner);
                if (ptActorPlayer == nullptr)
                    continue;

                ActorFlagSet(ptActorPlayer->id, ACTOR_GOD_MODE_BIT, 1);      // Just for fun
                ActorFlagSet(ptActorPlayer->id, ACTOR_EASYKILL_BIT, 1);      // Just for fun
                ActorFlagSet(ptActorPlayer->id, ACTOR_NO_COLLISION_BIT, 1);  // Just for fun
                // ActorFlagSet(ptActorPlayer->id, ACTOR_NO_DAMAGE_RANGE_BIT, 1);  // Just for fun

                IntAttribSet(ptACDCurPlayer, GOD);
                IntAttribSet(ptACDCurPlayer, INVULNERABLE);
                IntAttribSet(ptACDCurPlayer, PLAYER_ALL_ITEMS_INDESTRUCTIBLE);
                // IntAttribSet(ptACDCurPlayer, SHARED_STASH_SLOTS, -1, 789);
                IntAttribSet(ptACDCurPlayer, IMMUNE_TO_KNOCKBACK);
                IntAttribSet(ptACDCurPlayer, IMMUNE_TO_BLIND);
                IntAttribSet(ptACDCurPlayer, IMMUNE_TO_CHARM);
                IntAttribSet(ptACDCurPlayer, STUN_IMMUNE);
                IntAttribSet(ptACDCurPlayer, FEAR_IMMUNE);
                IntAttribSet(ptACDCurPlayer, SLOWDOWN_IMMUNE);
                IntAttribSet(ptACDCurPlayer, FREEZE_IMMUNE);
                IntAttribSet(ptACDCurPlayer, GETHIT_IMMUNE);
                // // IntAttribSet(ptACDCurPlayer, CORE_ATTRIBUTES_FROM_ITEM_BONUS_MULTIPLIER, 5);
                // // IntAttribSet(ptACDCurPlayer, PARAGONCAPENABLED, 0, 0);
            }
        }
    };

    inline void UnlockAll() {
        if (!_HOSTCHK)
            return;
        // _SERVERCODE_ON
        ActorCommonData *ptACDCurPlayer = nullptr;
        Actor const     *ptActorPlayer  = nullptr;
        ACDID            idCurPlayer    = ACDID_INVALID;
        // GameError        errorCode      = GAMEERROR_NONE;
        // auto             nTimer         = nn::TimeSpan::FromMilliSeconds(10);
        FlagsAreFun();
        // auto v_items   = AllGBIDsOfType(GB_ITEMS);
        // vector<GBID> arItemQualityClasses;
        // AllGBIDsOfType(GB_ITEM_QUALITY_CLASSES, &arItemQualityClasses);
        // arItemQualityClasses.clear();
        // vector<GBID> arGBIDAll;
        // AllGBIDsOfType(GB_INVALID, &arGBIDAll);
        // arGBIDAll.clear();

        for (auto *ptSPlayer = PlayerGetFirstAll(); ptSPlayer != nullptr; ptSPlayer = PlayerGetNextAll(ptSPlayer)) {
            // nn::os::SleepThread(nTimer);
            if (ptSPlayer->idPlayerACD != ACDID_INVALID) {
                idCurPlayer    = ptSPlayer->idPlayerACD;
                ptACDCurPlayer = ACDTryToGet(idCurPlayer);
                ptActorPlayer  = ActorGet(ptACDCurPlayer->idOwner);

                ActorFlagSet(ptActorPlayer->id, ACTOR_GOD_MODE_BIT, 1);      // Just for fun
                ActorFlagSet(ptActorPlayer->id, ACTOR_EASYKILL_BIT, 1);      // Just for fun
                ActorFlagSet(ptActorPlayer->id, ACTOR_NO_COLLISION_BIT, 1);  // Just for fun
                // ActorFlagSet(ptActorPlayer->id, ACTOR_NO_DAMAGE_RANGE_BIT, 1);  // Just for fun

                IntAttribSet(ptACDCurPlayer, GOD);
                IntAttribSet(ptACDCurPlayer, INVULNERABLE);
                IntAttribSet(ptACDCurPlayer, PLAYER_ALL_ITEMS_INDESTRUCTIBLE);
                // IntAttribSet(ptACDCurPlayer, SHARED_STASH_SLOTS, -1, 789);
                IntAttribSet(ptACDCurPlayer, IMMUNE_TO_KNOCKBACK);
                IntAttribSet(ptACDCurPlayer, IMMUNE_TO_BLIND);
                IntAttribSet(ptACDCurPlayer, IMMUNE_TO_CHARM);
                IntAttribSet(ptACDCurPlayer, STUN_IMMUNE);
                IntAttribSet(ptACDCurPlayer, FEAR_IMMUNE);
                IntAttribSet(ptACDCurPlayer, SLOWDOWN_IMMUNE);
                IntAttribSet(ptACDCurPlayer, FREEZE_IMMUNE);
                IntAttribSet(ptACDCurPlayer, GETHIT_IMMUNE);
                // // IntAttribSet(ptACDCurPlayer, CORE_ATTRIBUTES_FROM_ITEM_BONUS_MULTIPLIER, 5);
                // // IntAttribSet(ptACDCurPlayer, PARAGONCAPENABLED, 0, 0);

                PRINT_EXPR("%s: %x | %x | %x \n", ptSPlayer->tAccount.usName, ptSPlayer->idPlayerACD, ptSPlayer->idGameConnection, ptACDCurPlayer->ann)
                PRINT_EXPR("g_mapLobby[%x] = %i", idCurPlayer, g_mapLobby[idCurPlayer])
                PRINT_EXPR("ptACDID[%x]->uDigestFlags = %i", idCurPlayer, ptSPlayer->tAccount.uDigestFlags)
                auto *p_uDigestFlags = &ptSPlayer->tAccount.uDigestFlags;
                *p_uDigestFlags      = 0xFFF;
                SyncFlags(ptSPlayer, *p_uDigestFlags);
                auto kParam = static_cast<int>(0xFEFEFEFE);

                // ExperienceDropLootForAll(ptACDCurPlayer, idCurPlayer, 0xFFFFFFFF, 0, 14 + 7.0);
                // ShowError(ptSPlayer, (6 + GAMEERROR_BNET_MULTIPLAYER_GAMENOTPAUSED));
                // ShowError(ptSPlayer, (6 + GAMEERROR_CANNOT_DO_THAT_RIGHT_NOW));

                // GBHandle   hGB {2, 356350083};
                // auto       Item     = LootQuickCreateItem(&hGB, 1);
                // auto       tACDItem = ACDTryToGet(Item);
                // WorldPlace tPlace;
                // if (FlippyFindLandingLocation(ptACDCurPlayer, tACDItem->snoActor, &tPlace, ptSPlayer, 0LL, ACDID_INVALID)) {
                //     PRINT_EXPR("Created arbitrary item, GBID: %i", &hGB.gbid)
                //     FlippyDropCreateOnActor(ptACDCurPlayer->id, tACDItem->id, &tPlace);
                // }  // SACDInventoryPickupOrSpillOntoGround(tACDCurPlayer, tACDItem, 0, 0LL, 1);
                DisplayGameMessageForAllPlayers("RareNameStrings_Prefix_Dex:Dex 17", kParam, kParam);
                // nn::os::SleepThread(nTimer);
                // DisplayGameMessageForAllPlayers("UIToolTips:Seasonal", kParam, kParam);

                ImageTextureFrame iframe;
                ImageTextureFrame_ctor(&iframe, "UI:ConsoleSeasonJourneyRingLeaf");
                auto msg = PlayerGetHeroDisplayName(ptSPlayer, 0);
                msg += "{c_red} - ";
                msg += ptSPlayer->tAccount.usName;
                msg += "{/c}: all items unlocked! {icon:x1_shard}";
                DisplayLongMessageForPlayer(&msg, ptSPlayer, &iframe, 1);

                // nn::os::SleepThread(nTimer);
                ImageTextureFrame_ctor(&iframe, "Items:p4_SeasonalCache_Icon");
                msg = "{c_gold}" D3HACK_WEB "{/c}";
                DisplayLongMessageForPlayer(&msg, ptSPlayer, &iframe, 0);

                // nn::os::SleepThread(nTimer);
                ImageTextureFrame_ctor(&iframe, "UI:Console_SkillSelectionRune00_Selected");
                msg = "Enjoy!";
                DisplayLongMessageForPlayer(&msg, ptSPlayer, &iframe, 0);
                // nn::os::SleepThread(nTimer);
                // DisplayGameMessageForAllPlayers("UIToolTips:Gold", kParam, kParam);
                DisplayGameMessageForAllPlayers("ConsoleUI:AutosaveWarningScreenText_Switch", kParam, kParam);
                if (!g_mapLobby[idCurPlayer]) {
                    // g_mapLobby[idCurPlayer] = true;
                    /* Mod stash slots to maximum of 910 */
                    IntAttribSet(ptACDCurPlayer, SHARED_STASH_SLOTS, -1, 910);
                    IntAttribSet(ptACDCurPlayer, HIGHEST_UNLOCKED_RIFT_LEVEL, -1, 147);
                    IntAttribSet(ptACDCurPlayer, HIGHEST_SOLO_RIFT_LEVEL, -1, 147);
                    IntAttribSet(ptACDCurPlayer, HIGHEST_HERO_SOLO_RIFT_LEVEL, -1, 147);
                    IntAttribSet(ptACDCurPlayer, LAST_RIFT_STARTED_LEVEL, -1, 146);
                    /* Level to 70 with Paragon level of 799 */
                    ExperienceSetLevel(idCurPlayer, 70 + 799, 1);

                    /* Set very high gold amount @ index 0 before iterating other mats */
                    ACD_SetCurrencyAmount(ptACDCurPlayer, 1337420699999999, 0, 0);

                    /* Edit all materials quantity */
                    for (auto i = 1; i <= g_nMats; ++i)
                        ACD_SetCurrencyAmount(ptACDCurPlayer, 1337420696, i, 0);

                    /* Level up all crafters safely, max level is either 1 or 12  */
                    for (auto i = 0; i <= g_nCrafters; ++i)
                        SCrafterIncrementLevel(ptSPlayer, i, 69);

                    /* Iterate through every item GBID in the game for cosmetics/transmogs/powers */
                    // EnumGBIDUnlocks(GB_INVALID, ptSPlayer);
                    // EnumGBIDUnlocks(GB_RECIPES, ptSPlayer);
                    // EnumGBIDUnlocks(GB_TRANSMUTE_RECIPES, ptSPlayer);
                    EnumGBIDUnlocks(GB_ITEMS, ptSPlayer);

                    // if (const Item *eItemPtr = GetItemPtr(eItem); eItemPtr->nSeasonRequiredToDrop > 21) {
                    //     // PRINT_EXPR("%i", eItemPtr->gbid)
                    //     // PRINT_EXPR("%i", eItemPtr->nSeasonRequiredToDrop)
                    //     // PRINT_EXPR("%i", eItemPtr->ptItemType->nLootLevelRange)
                    //     // PRINT_EXPR("%i", eItemPtr->ptItemType->ptParentType->nLootLevelRxnge)
                    //     PRINT("(S%d) %i | %s", eItemPtr->nSeasonRequiredToDrop, eItemPtr->gbid, "rip memory")  //(LPCSTR)GBIDToStringAll(&eItemPtr->gbid))
                    //     // PRINT_EXPR("%s", (LPCSTR)GBIDToStringAll(&eItemPtr->ptItemType->gbid))
                    //     // PRINT_EXPR("%s", (LPCSTR)GBIDToStringAll(&eItemPtr->ptItemType->ptParentType->gbid))
                    //     PRINT_EXPR("\n0x%x | 0x%x | %d", eItemPtr->dwFlags, *eItemPtr->pdwLabelMask, *eItemPtr->pnDropWeights)
                    //     // PRINT_EXPR("0x%x", *eItemPtr->ptItemType->pdwLabelMask)
                    //     // PRINT_EXPR("0x%x", *eItemPtr->ptItemType->ptParentType->pdwLabelMask)

                    PRINT("%d", 0)
                    // break;
                    /* Teach all crafting recipes */
                    std::vector<GBID> eRecipes;
                    // (static_cast<size_t>(1000));
                    AllGBIDsOfType(GB_RECIPES, eRecipes);
                    for (GBID eRecipe : eRecipes)
                        if (eRecipe != -1)
                            SItemCrafting_LearnRecipe(ptSPlayer->idPlayerACD, eRecipe);
                    PRINT("%d", 1)
                }

            } else {
                PRINT_EXPR("invalid target: %p", ptSPlayer)
                continue;
            }
        }
        // _SERVERCODE_OFF
    }

    inline void CreateFlippy(ActorCommonData *tACDPlayer, ActorCommonData *tACDNewItem) {
        if (!_HOSTCHK)
            return;
        _SERVERCODE_ON
        WorldPlace tPlace;  // SACDInventoryPickupOrSpillOntoGround(tACDPlayer, tACDNewItem, 0, 0LL, 1);
        if ((tACDNewItem != nullptr) &&
            (FlippyFindLandingLocation(tACDPlayer, tACDNewItem->snoActor, &tPlace, PlayerGetByACD(tACDPlayer->id), nullptr, ACDID_INVALID) != 0))
            FlippyDropCreateOnActor(tACDPlayer->id, tACDNewItem->id, &tPlace);
        _SERVERCODE_OFF
    }

    inline auto DupeItem(ActorCommonData * /*tACDPlayer*/, ActorCommonData *tACDItem, bool /*bCreateFlippy*/ = false) -> ActorCommonData * {
        if (!_HOSTCHK || (ItemHasLabel(tACDItem->hGB.gbid, 114) != 0))
            return nullptr;
        D3::Items::Generator tGenerator;
        ItemsGenerator_ctor(&tGenerator);
        auto tRareItemName = *FollowPtr<RareItemName, 0x138>(tACDItem);

        *FollowPtr<RareItemName, 0x138>(tACDItem) = g_tRareItemName;  // Always force template item rare name to "Jester's"
        PopulateGenerator(tACDItem, &tGenerator, 0);                  // Populate template for new item
        *FollowPtr<RareItemName, 0x138>(tACDItem) = tRareItemName;    // Restore original rare name
        _SERVERCODE_ON
        auto *tACDNewItem = ACDTryToGet(SItemGenerate(&g_itemInvalid, &tGenerator, &g_cPlaceNull, 0.0, 0));
        _SERVERCODE_OFF
        ItemsGenerator_dtor(&tGenerator);  // Destruct generator
        return tACDNewItem;
    }

}  // namespace d3
