#pragma once

#include "d3/types/common.hpp"

namespace d3 {
    constinit int8 g_nMats     = 22;
    constinit int8 g_nCrafters = 4;

    struct GameBalanceTable {
        const LPCSTR          name;
        const int32           eType;
        const std::size_t     maxElements;
    } g_tGBTableUnshifted[NUM_GBTYPES + 2] {
        {"GB_KEYWORD", -2},          // 0xFFFFFFFE
        {"GB_INVALID", -1},          // 0xFFFFFFFF
        {"", 0x0},
        {"GB_ITEMTYPES", 0x1, 215},  // ItemTypes
        {"GB_ITEMS", 0x2, 6691},     // Items
        {"GB_EXPERIENCETABLE", 0x3},
        {"GB_HIRELINGS", 0x4, 3},    // Hirelings
        {"GB_MONSTER_LEVELS", 0x5},
        {"", 0x6},
        {"GB_HEROS", 0x7, 7},             // Heroes
        {"GB_AFFIXES", 0x8, 4592},        // Affixes
        {"", 0x9},
        {"GB_MOVEMENT_STYLES", 0xA, 26},  // MovementStyles
        {"GB_LABELS", 0xB, 1100},         // Labels
        {"GB_LOOTDISTRIBUTION", 0xC},
        {"", 0xD},
        {"", 0xE},
        {"", 0xF},
        {"GB_RARE_ITEM_NAMES", 0x10, 920},   // RareItemNames
        {"GB_SCENERY", 0x11},
        {"GB_MONSTER_AFFIXES", 0x12, 65},    // MonsterAffixes
        {"GB_MONSTER_NAMES", 0x13, 437},     // MonsterNames
        {"", 0x14},
        {"GB_SOCKETED_EFFECTS", 0x15, 656},  // SocketedEffects
        {"", 0x16},
        {"", 0x17},
        {"GB_HELPCODES", 0x18},
        {"GB_ITEM_DROP_TABLE", 0x19, 1049},     // ItemDropTable
        {"GB_ITEM_LEVEL_MODIFIERS", 0x1A},
        {"GB_ITEM_QUALITY_CLASSES", 0x1B, 26},  // ItemQualityClasses
        {"GB_HANDICAPS", 0x1C},
        {"GB_ITEM_SALVAGE_LEVELS", 0x1D},
        {"", 0x1E},
        {"", 0x1F},
        {"GB_RECIPES", 0x20, 622},           // Recipes
        {"GB_SET_ITEM_BONUSES", 0x21, 656},  // SetItemBonuses
        {"GB_ELITE_MODIFIERS", 0x22, 26},    // EliteModifiers
        {"GB_ITEM_TIERS", 0x23},
        {"GB_POWER_FORMULA_TABLES", 0x24},
        {"GB_SCRIPTED_ACHIEVEMENT_EVENTS", 0x25, 119},  // ScriptedAchievementEvents
        {"", 0x26},
        {"GB_LOOTRUN_QUEST_TIERS", 0x27, 3},            // LootRunQuestTiers
        {"GB_PARAGON_BONUSES", 0x28, 28},               // ParagonBonuses
        {"", 0x29},
        {"", 0x2A},
        {"", 0x2B},
        {"", 0x2C},
        {"GB_LEGACY_ITEM_CONVERSIONS", 0x2D, 278},  // LegacyItemConversions
        {"GB_ENCHANT_ITEM_AFFIX_USE_COUNT_COST_SCALARS", 0x2E},
        {"GB_AFFIXFAMILY", 0x2F},
        {"GB_EXPERIENCETABLEALT", 0x30},
        {"GB_TIERED_LOOT_RUN_LEVELS", 0x31},
        {"GB_TRANSMUTE_RECIPES", 0x32, 16},    // TransmuteRecipes
        {"GB_CURRENCY_CONVERSIONS", 0x33, 22}  // CurrencyConversions
    };

}  // namespace d3