#include "program/config.hpp"
#include "program/d3/_util.hpp"
#include "program/d3/patches.hpp"
#include "program/d3/types/common.hpp"
#include "program/d3/types/attributes.hpp"
#include "lib/hook/inline.hpp"
#include "lib/hook/replace.hpp"
#include "lib/hook/trampoline.hpp"
#include "nn/oe.hpp"

namespace d3 {

    HOOK_DEFINE_TRAMPOLINE(AttackSpeed) {
        // float PowerGetFormulaValueAtLevel(AttribGroupID idFastAttrib, const ActorCommonData *ptACD, SNO snoPower, uint8 *pbFormula, int32 nFormulaSize, int32 nLevel)
        static auto Callback(AttribGroupID idFastAttrib, ActorCommonData *ptACD, SNO snoPower, uint8 *pbFormula, int32 nFormulaSize, int32 nLevel) -> float {
            auto ret = Orig(idFastAttrib, ptACD, snoPower, pbFormula, nFormulaSize, nLevel);
            if (snoPower != 0x50860)
                return ret;
            FastAttribKey tKey;
            tKey.nValue = ATTACKS_PER_SECOND_TOTAL, ACD_AttributesSetFloat(ptACD, tKey, 3.0f);
            PRINT_EXPR("ORIG ATKSPD: %f", ret);
            return ret * 5.5f;
        }
    };

    HOOK_DEFINE_TRAMPOLINE(MoveSpeed) {
        // @ float CPlayerGetMoveSpeedForStickInput(ActorCommonData *tACDPlayer, Player *tPlayer, SNO snoPower, float flMinClientWalkSpeed)
        static auto Callback(ActorCommonData *tACDPlayer, Player *tPlayer, SNO snoPower, float flMinClientWalkSpeed) -> float {
            auto MoveSpeed = Orig(tACDPlayer, tPlayer, snoPower, flMinClientWalkSpeed);
            if (!global_config.rare_cheats.active)
                return MoveSpeed;
            const auto mult = static_cast<float>(global_config.rare_cheats.move_speed);
            if (mult <= 0.0f)
                return MoveSpeed;
            return MoveSpeed * mult;
        }
    };

    HOOK_DEFINE_TRAMPOLINE(EquipAny) {
        //   @ GameError ACDInventoryItemAllowedInSlot(const ActorCommonData *tACDItem, const InventoryLocation *tInvLoc, const BOOL fSkipRequirements, const BOOL fSwapping)
        static auto Callback(const ActorCommonData *tACDItem, const InventoryLocation *tInvLoc, const BOOL fSkipRequirements, const BOOL fSwapping) -> GameError {
            auto rGameError = Orig(tACDItem, tInvLoc, fSkipRequirements, fSwapping);
            if (!global_config.rare_cheats.active || !global_config.rare_cheats.equip_any_slot)
                return rGameError;
            return GAMEERROR_NONE;
        }
    };

    inline void AugmentSpecifier(LootSpecifier *tSpecifier) {
        if (global_config.loot_modifiers.ForcedILevel > 0) {
            tSpecifier->tLooteeParams.nForcedILevel = global_config.loot_modifiers.ForcedILevel;
        }
        if (global_config.loot_modifiers.TieredLootRunLevel > 0) {
            tSpecifier->tLooteeParams.nTieredLootRunLevel = global_config.loot_modifiers.TieredLootRunLevel;
        }
        tSpecifier->tLooteeParams.bDisableAncientDrops       = static_cast<BOOL>(global_config.loot_modifiers.DisableAncientDrops);
        tSpecifier->tLooteeParams.bDisablePrimalAncientDrops = static_cast<BOOL>(global_config.loot_modifiers.DisablePrimalAncientDrops);
        tSpecifier->tLooteeParams.bDisableTormentDrops       = static_cast<BOOL>(global_config.loot_modifiers.DisableTormentDrops);
        tSpecifier->tLooteeParams.bDisableTormentCheck       = static_cast<BOOL>(global_config.loot_modifiers.DisableTormentCheck);
        tSpecifier->tLooteeParams.bSuppressGiftGeneration    = static_cast<BOOL>(global_config.loot_modifiers.SuppressGiftGeneration);
        tSpecifier->eAncientRank                             = global_config.loot_modifiers.AncientRankValue;
    }

    HOOK_DEFINE_REPLACE(ForceAncient) {
        static void Callback(LootSpecifier *tSpecifier, const ACDID idACDLooter) {
            AugmentSpecifier(tSpecifier);
            if (ActorCommonData *ptACDPlayer = ACDTryToGet(idACDLooter); ptACDPlayer) {
                FastAttribKey tKey;
                tKey.nValue = RECEIVED_SEASONAL_LEGENDARY;
                ACD_AttributesSetInt(ptACDPlayer, tKey, 0);
                // tKey.nValue = TARGETED_LEGENDARY_CHANCE;
                // ACD_AttributesSetInt(ptACDPlayer, tKey, 100000);
                // FastAttribKey flKey;
                // flKey.nValue = LEGENDARY_FIND_COMMUNITY_BUFF;
                // ACD_AttributesSetFloat(ptACDPlayer, flKey, 100.0f);
            }
        }
    };

    HOOK_DEFINE_REPLACE(Return_True) {
        static auto Callback() -> BOOL { return 1; }
    };

    HOOK_DEFINE_INLINE(FloatingDmgHook) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            // BB5E8
            auto *DmgColor = reinterpret_cast<RGBAColor *>(ctx->X[3]);
            *DmgColor      = crgbaRed;
        }
    };

    HOOK_DEFINE_INLINE(VarResHook) {
        static void Callback(exl::hook::InlineCtx *ctx) {
            auto *ptVarRWindow = reinterpret_cast<VariableResRWindowData *>(ctx->X[19]);
            if (ptVarRWindow != nullptr && global_config.resolution_hack.active) {
                const float min_pct        = std::clamp(global_config.resolution_hack.min_res_scale, 10.0f, 100.0f);
                const float max_pct        = std::clamp(global_config.resolution_hack.max_res_scale, min_pct, 100.0f);
                ptVarRWindow->flMinPercent = min_pct * 0.01f;
                ptVarRWindow->flMaxPercent = max_pct * 0.01f;
            }
        };
    };

    HOOK_DEFINE_TRAMPOLINE(SpoofDocked) {
        static auto Callback() -> nn::oe::OperationMode {
            if (!global_config.resolution_hack.spoof_docked)
                return Orig();
            return nn::oe::OperationMode_Docked;
        }
    };

    HOOK_DEFINE_TRAMPOLINE(SpoofPerfMode) {
        static auto Callback() -> nn::oe::PerformanceMode {
            if (!global_config.resolution_hack.spoof_docked)
                return Orig();
            return nn::oe::PerformanceMode_Boost;
        }
    };

    inline void SetupUtilityHooks() {
#if D3HACK_ENABLE_UTILITY_DEBUG_HOOKS
        SetupUtilityDebugHooks();
#endif

        if (global_config.rare_cheats.active && global_config.rare_cheats.equip_any_slot) {
            EquipAny::
                InstallAtFuncPtr(ACDInventoryItemAllowedInSlot);
        }

        if (global_config.resolution_hack.active) {
            VarResHook::
                InstallAtSymbol("sym_var_res_label");
        }
        if (global_config.loot_modifiers.active) {
            ForceAncient::
                InstallAtFuncPtr(LootRollForAncientLegendary);
        }
        if (global_config.rare_cheats.active && global_config.rare_cheats.move_speed != 1.0) {
            MoveSpeed::
                InstallAtFuncPtr(move_speed);
        }
        if (global_config.rare_cheats.active && global_config.rare_cheats.attack_speed != 1.0) {
            AttackSpeed::
                InstallAtFuncPtr(attack_speed);
        }
        if (global_config.rare_cheats.active && global_config.rare_cheats.floating_damage_color) {
            FloatingDmgHook::
                InstallAtSymbol("sym_floating_dmg");
        }

        if (global_config.resolution_hack.spoof_docked) {
            SpoofPerfMode::InstallAtFuncPtr(nn::oe::GetPerformanceMode);
            SpoofDocked::InstallAtFuncPtr(nn::oe::GetOperationMode);
        }
    }

}  // namespace d3
