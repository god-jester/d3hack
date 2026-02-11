#pragma once

#include "program/d3/_lobby.hpp"
#include "program/d3/_util.hpp"
#include "program/d3/types/common.hpp"
#include "lib/hook/inline.hpp"
#include "lib/hook/replace.hpp"
#include "lib/hook/trampoline.hpp"

namespace d3 {

    HOOK_DEFINE_TRAMPOLINE(RequestDropItemHook) {
        static void Callback(ActorCommonData *ptACD, const ACDID idACDOwner) {
            const bool can_dupe = _HOSTCHK && ptACD && ptACD->id != ACDID_INVALID && !ItemHasLabel(ptACD->hGB.gbid, 114);
            if (can_dupe) {
                PRINT_EXPR("Duping: %p", ptACD);
                // _SERVERCODE_ON
                if (ActorCommonData *ptACDPlayer = ACDTryToGet(idACDOwner); ptACDPlayer) {
                    if (ActorCommonData *ptACDNewItem = DupeItem(ptACDPlayer, ptACD); ptACDNewItem)
                        CreateFlippy(ptACDPlayer, ptACDNewItem), UnbindACD(ptACDNewItem);
                }
            } else {
                PRINT_EXPR("ERROR! Invalid dupe attempt, wanting to drop: %x", idACDOwner)
            }
            Orig(ptACD, idACDOwner);
            // if (can_dupe) {
            // ParagonGBIDTests();
            // UnlockAll();
            // _SERVERCODE_OFF
            // }
        }
    };

    HOOK_DEFINE_TRAMPOLINE(DupeDroppedItem) {
        static auto Callback(const ACDID idACDWantingToDrop, const ACDID idACDItem, const BOOL bCheatEnabled) -> BOOL {
            if (idACDItem == ACDID_INVALID || bCheatEnabled == 0) {
                PRINT_EXPR("ERROR! Invalid dupe attempt, ACDID wanting to drop: 0x%x", idACDWantingToDrop)
            } else {
                if (ActorCommonData *ptACDPlayer = ACDTryToGet(idACDWantingToDrop); ptACDPlayer) {
                    auto ptACD = ACDTryToGet(idACDItem);
                    if (ActorCommonData *ptACDNewItem = DupeItem(ptACDPlayer, ptACD); ptACDNewItem)
                        CreateFlippy(ptACDPlayer, ptACDNewItem), UnbindACD(ptACDNewItem);
                }
                UnlockAll();
            }
            return Orig(idACDWantingToDrop, idACDItem, true);
        }
    };

    HOOK_DEFINE_TRAMPOLINE(FastAttribGetIntValue) {
        static auto Callback(const FastAttribGroup *ptAttribGroup, FastAttribKey tKey) -> __int64 {
            auto            ret = Orig(ptAttribGroup, tKey);
            FastAttribValue ret1(static_cast<int32>(ret));
            // AttribStringInfo(tKey, ret1);
            // = {_anon_0_ = ret};
            // int intValue = ret1._anon_0.nValue;

            // retval = FastAttribGetValueInt(*((FastAttribGroup **)tACD + 45), tKey);

            switch (KeyGetAttrib(tKey)) {
            case ITEM_EQUIPPED_BUT_DISABLED:
            case ITEM_EQUIPPED_BUT_DISABLED_DUPLICATE_LEGENDARY:
                // AttribStringInfo(tKey, ret1);
                // case ParagonCapEnabled:
                return 0;
                // case PARAGON_BONUS:
                // PRINT_EXPR("%s", AttribToStr(tKey))
                // if (KeyGetParam(tKey) != -1) { PRINT_EXPR("%li", KeyGetParam(tKey))}
                // AttribStringInfo(tKey, ret1);
                //     return ret;
                // case PARAGONCAPENABLED:
                // AttribStringInfo(tKey, ret1);
                // AttribToStr(tKey);
                // ParamToStr(KeyGetFullAttrib(tKey), KeyGetParam(tKey));
                // if (KeyGetParam(tKey) != -1) { PRINT_EXPR("%li", KeyGetParam(tKey))}
                // CheckStringList(tKey.nValue, AttribToStr(tKey));
                // CheckStringList(tKey.nValue, ParamToStr(KeyGetFullAttrib(tKey), KeyGetParam(tKey)));
                // return ret;
            default:
                return ret;
            }

            return ret;
        };
    };

    HOOK_DEFINE_TRAMPOLINE(FastAttribGetFloatValue) {
        static auto Callback(FastAttribGroup *ptAttribGroup, FastAttribKey tKey) -> float {
            auto ret = Orig(ptAttribGroup, tKey);
            switch (KeyGetAttrib(tKey)) {
            case TARGETED_LEGENDARY_CHANCE:
            case SEASONAL_LEGENDARY_CHANCE:
                return 3.40282347e+38f;
            case GOLD_FIND_TOTAL:
                return 100000000.0f;
            case GOLD_PICKUP_RADIUS:
                return 10000000000.0f;
            case ATTACKS_PER_SECOND_TOTAL:
                return 100.0f;
            case BLOCK_CHANCE_CAPPED_TOTAL:
            case DODGE_CHANCE_BONUS:
            case TARGETED_MAGIC_CHANCE:
            case TARGETED_RARE_CHANCE:
                return 0.0f;
            default:
                break;
            }
            // if (ret >= 1736.000001f && ret <= 1738.000001f)
            //     PRINT("FastAttribGetFloatValue 1737: (tKey.nValue: 0x%x), Value: %f", tKey.nValue, ret)
            return ret;
        };
    };

    HOOK_DEFINE_REPLACE(AttributesGetInt) {
        static auto Callback(ActorCommonData *tACD, FastAttribKey tKey) -> __int64 {
            // retval = FastAttribGetValueInt(*((FastAttribGroup **)tACD + 45), tKey);
            // auto lParam = KeyGetParam(tKey);
            // auto lParam = KeyGetDataType(tKey);
            switch (KeyGetAttrib(tKey)) {
            case ITEM_EQUIPPED_BUT_DISABLED:
            case ITEM_EQUIPPED_BUT_DISABLED_DUPLICATE_LEGENDARY:
            case FORCE_GRIPPED:
                return 0;
            case HAS_INFINITE_SHRINE_BUFFS:
                return 1;
            case ATTRIBUTE_SET_ITEM_DISCOUNT:
                return 4;
            case PARAGON_BONUS:
            case PARAGONCAPENABLED:
                // if (KeyGetParam(tKey) != -1) {
                //     PRINT_EXPR("%i %li", tKey.nValue, KeyGetParam(tKey))
                // }
                // CheckStringList(tKey.nValue, AttribToStr(tKey));
                // CheckStringList(tKey.nValue, ParamToStr(KeyGetFullAttrib(tKey), KeyGetParam(tKey)));
                // return FastAttribGetValueInt(*(reinterpret_cast<FastAttribGroup **>(tACD) + 45), tKey);
            default:
                return FastAttribGetValueInt(*(reinterpret_cast<FastAttribGroup **>(tACD) + 45), tKey);
            }
            return FastAttribGetValueInt(*(reinterpret_cast<FastAttribGroup **>(tACD) + 45), tKey);
        }
    };

    HOOK_DEFINE_REPLACE(AttributesGetFloat) {
        static auto Callback(ActorCommonData *tACD, FastAttribKey tKey) -> float {
            float const ret = FastAttribGetValueFloat(*(reinterpret_cast<FastAttribGroup **>(tACD) + 45), tKey);

            switch (KeyGetAttrib(tKey)) {
            // case TARGETED_LEGENDARY_CHANCE:
            // case SEASONAL_LEGENDARY_CHANCE:
            // case BLOCK_CHANCE_CAPPED_TOTAL:
            // case DODGE_CHANCE_BONUS: // this applied to *monster* dodge chance too, unable to hit
            // return 1.0f;
            // case GOLD_FIND_TOTAL:
            // case MAGIC_FIND_TOTAL:
            //     return 100000000.0f;
            // case TREASURE_FIND:
            case GOLD_PICKUP_RADIUS:
                return 10000000000.0f;
            default:
                break;
            }
            return ret;
        }
    };

    HOOK_DEFINE_INLINE(EvalMod) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            // auto  idACDLooter       = static_cast<ACDID>(ctx->W[1]);
            auto *tLootDropModifier = reinterpret_cast<LootDropModifier *>(ctx->X[3]);
            if (tLootDropModifier && tLootDropModifier->flChance > 0.0f) {
                // PRINT_EXPR("LootDropModifier: %.3f", tLootDropModifier->flChance)
                tLootDropModifier->flChance = 1000.00f;
            }
            if (tLootDropModifier && tLootDropModifier->flGoldMultiplier > 0.0f) {
                // PRINT_EXPR("LootDropModifier: %.3f", tLootDropModifier->flChance)
                tLootDropModifier->flGoldMultiplier = 10000.00f;
            }
            // if (ActorCommonData *ptACDPlayer = ACDTryToGet(idACDLooter); ptACDPlayer) {
            //     if (!ptACDPlayer) return;
            //     FastAttribKey tKey;
            //     tKey.nValue = SEASONAL_LEGENDARY_CHANCE;
            //     ACD_AttributesSetInt(ptACDPlayer, tKey, 100000);
            //     tKey.nValue = TARGETED_LEGENDARY_CHANCE;
            //     ACD_AttributesSetInt(ptACDPlayer, tKey, 100000);
            //     FastAttribKey flKey;
            //     flKey.nValue = LEGENDARY_FIND_COMMUNITY_BUFF;
            //     ACD_AttributesSetFloat(ptACDPlayer, flKey, 100.0f);
            // }
        }
    };

    HOOK_DEFINE_INLINE(SpecifiersFromModifier) {
        //@ void  sGetSpecifiersFromModifier(__int64 a1, int idACDLooter, const LootDropModifier *tModifier, GBID gbidInheritedQualityClass_1, LootSpecifierArray *a5, const CRefString **a6, unsigned int a7, SharedLootManager *a8)
        static void Callback(exl::hook::InlineCtx *ctx) {
            // auto  idACDLooter       = static_cast<ACDID>(ctx->W[1]);
            auto *tLootDropModifier = reinterpret_cast<LootDropModifier *>(ctx->X[2]);
            if (tLootDropModifier && tLootDropModifier->flChance > 0.0f) {
                // PRINT_EXPR("LootDropModifier: %.3f", tLootDropModifier->flChance)
                tLootDropModifier->flChance = 1000.00f;
            }
            if (tLootDropModifier && tLootDropModifier->flGoldMultiplier > 0.0f) {
                // PRINT_EXPR("LootDropModifier: %.3f", tLootDropModifier->flChance)
                tLootDropModifier->flGoldMultiplier = 10000.00f;
            }
            // if (ActorCommonData *ptACDPlayer = ACDTryToGet(idACDLooter); ptACDPlayer) {
            //     if (!ptACDPlayer) return;
            //     FastAttribKey tKey;
            //     tKey.nValue = SEASONAL_LEGENDARY_CHANCE;
            //     ACD_AttributesSetInt(ptACDPlayer, tKey, 100000);
            //     tKey.nValue = TARGETED_LEGENDARY_CHANCE;
            //     ACD_AttributesSetInt(ptACDPlayer, tKey, 100000);
            //     FastAttribKey flKey;
            //     flKey.nValue = LEGENDARY_FIND_COMMUNITY_BUFF;
            //     ACD_AttributesSetFloat(ptACDPlayer, flKey, 100.0f);
            // }
        }
    };

    HOOK_DEFINE_INLINE(SGameGet) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto idSGame = ctx->W[22];
            PRINT_EXPR("SGAMEID: %x", idSGame)
            // PRINT_EXPR("%i", ServerIsLocal())
            // auto ptSGame = reinterpret_cast<SGame *>(ctx->X[23]);
        }
    };

    HOOK_DEFINE_INLINE(CPlayerNewPlayerMsg) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            if (auto *ptP = reinterpret_cast<Player *>(ctx->X[20]); ptP != nullptr) {
                auto szN = ptP->tAccount.usName;
                PRINT_EXPR("NEW PLAYER! %s %p->%lx ACD: %x", szN, ptP, ptP->uHeroId, ptP->idPlayerACD)
                if (GameConnectionID const idGameConnection = ptP->idGameConnection) {  //ServerGetOnlyGameConnection()) {
                    PRINT_EXPR("NEW PLAYER! %s %x : %x (%x)", szN, ptP->idGameConnection, idGameConnection, AppServerGetOnlyGame())
                    // PRINT_EXPR("NEW PLAYER! %s %p : %p", szN, ptP, GetPrimaryPlayerForGameConnection(idGameConnection))
                }
                // PRINT_EXPR("NEW PLAYER! %s %x", szN, g_ptGCData->nNumPlayers)
            }
        }
    };

    inline void SetupLobbyHooks() {
        SpecifiersFromModifier::
            InstallAtSymbol("sym_specifiers_from_modifier");
        EvalMod::
            InstallAtSymbol("sym_eval_mod");
        DupeDroppedItem::                         // The primary hack trigger and functionality is here, when the host drops an item
            InstallAtFuncPtr(dupe_dropped_item);  // FuncPtr(InventoryDropRequest); /* BOOL SACDInventoryProcessDropRequest(const ACDID idACDWantingToDrop, const ACDID idACDItem, const BOOL bCheckAlive) */

        RequestDropItemHook::                     // The primary hack trigger and functionality is here, when the host drops an item
            InstallAtFuncPtr(request_drop_item);  // @ void CACDInventoryRequestDrop(ActorCommonData *ptACD, const ACDID idACDOwner)

        FastAttribGetIntValue::
            InstallAtFuncPtr(FastAttribGetValueInt);
        FastAttribGetFloatValue::
            InstallAtFuncPtr(FastAttribGetValueFloat);

        /*** Replacements ***/
        AttributesGetInt::
            InstallAtFuncPtr(ACD_AttributesGetInt);
        AttributesGetFloat::
            InstallAtFuncPtr(ACD_AttributesGetFloat);
    }

}  // namespace d3
