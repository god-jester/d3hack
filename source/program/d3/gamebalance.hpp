#pragma once

#include "d3/types/common.hpp"

namespace d3 {
    inline constinit int8 g_nMats     = 22;
    inline constinit int8 g_nCrafters = 4;

    struct GameBalanceTable {
        const LPCSTR      name;
        const int32       eType;
        const std::size_t maxElements;
    };

    inline constinit GameBalanceTable g_tGBTableUnshifted[NUM_GBTYPES + 2] {
        {.name = "GB_KEYWORD", .eType = -2},                         // 0xFFFFFFFE
        {.name = "GB_INVALID", .eType = -1},                         // 0xFFFFFFFF
        {.name = "", .eType = 0x0},
        {.name = "GB_ITEMTYPES", .eType = 0x1, .maxElements = 215},  // ItemTypes
        {.name = "GB_ITEMS", .eType = 0x2, .maxElements = 6691},     // Items
        {.name = "GB_EXPERIENCETABLE", .eType = 0x3},
        {.name = "GB_HIRELINGS", .eType = 0x4, .maxElements = 3},    // Hirelings
        {.name = "GB_MONSTER_LEVELS", .eType = 0x5},
        {.name = "", .eType = 0x6},
        {.name = "GB_HEROS", .eType = 0x7, .maxElements = 7},             // Heroes
        {.name = "GB_AFFIXES", .eType = 0x8, .maxElements = 4592},        // Affixes
        {.name = "", .eType = 0x9},
        {.name = "GB_MOVEMENT_STYLES", .eType = 0xA, .maxElements = 26},  // MovementStyles
        {.name = "GB_LABELS", .eType = 0xB, .maxElements = 1100},         // Labels
        {.name = "GB_LOOTDISTRIBUTION", .eType = 0xC},
        {.name = "", .eType = 0xD},
        {.name = "", .eType = 0xE},
        {.name = "", .eType = 0xF},
        {.name = "GB_RARE_ITEM_NAMES", .eType = 0x10, .maxElements = 920},   // RareItemNames
        {.name = "GB_SCENERY", .eType = 0x11},
        {.name = "GB_MONSTER_AFFIXES", .eType = 0x12, .maxElements = 65},    // MonsterAffixes
        {.name = "GB_MONSTER_NAMES", .eType = 0x13, .maxElements = 437},     // MonsterNames
        {.name = "", .eType = 0x14},
        {.name = "GB_SOCKETED_EFFECTS", .eType = 0x15, .maxElements = 656},  // SocketedEffects
        {.name = "", .eType = 0x16},
        {.name = "", .eType = 0x17},
        {.name = "GB_HELPCODES", .eType = 0x18},
        {.name = "GB_ITEM_DROP_TABLE", .eType = 0x19, .maxElements = 1049},     // ItemDropTable
        {.name = "GB_ITEM_LEVEL_MODIFIERS", .eType = 0x1A},
        {.name = "GB_ITEM_QUALITY_CLASSES", .eType = 0x1B, .maxElements = 26},  // ItemQualityClasses
        {.name = "GB_HANDICAPS", .eType = 0x1C},
        {.name = "GB_ITEM_SALVAGE_LEVELS", .eType = 0x1D},
        {.name = "", .eType = 0x1E},
        {.name = "", .eType = 0x1F},
        {.name = "GB_RECIPES", .eType = 0x20, .maxElements = 622},           // Recipes
        {.name = "GB_SET_ITEM_BONUSES", .eType = 0x21, .maxElements = 656},  // SetItemBonuses
        {.name = "GB_ELITE_MODIFIERS", .eType = 0x22, .maxElements = 26},    // EliteModifiers
        {.name = "GB_ITEM_TIERS", .eType = 0x23},
        {.name = "GB_POWER_FORMULA_TABLES", .eType = 0x24},
        {.name = "GB_SCRIPTED_ACHIEVEMENT_EVENTS", .eType = 0x25, .maxElements = 119},  // ScriptedAchievementEvents
        {.name = "", .eType = 0x26},
        {.name = "GB_LOOTRUN_QUEST_TIERS", .eType = 0x27, .maxElements = 3},            // LootRunQuestTiers
        {.name = "GB_PARAGON_BONUSES", .eType = 0x28, .maxElements = 28},               // ParagonBonuses
        {.name = "", .eType = 0x29},
        {.name = "", .eType = 0x2A},
        {.name = "", .eType = 0x2B},
        {.name = "", .eType = 0x2C},
        {.name = "GB_LEGACY_ITEM_CONVERSIONS", .eType = 0x2D, .maxElements = 278},  // LegacyItemConversions
        {.name = "GB_ENCHANT_ITEM_AFFIX_USE_COUNT_COST_SCALARS", .eType = 0x2E},
        {.name = "GB_AFFIXFAMILY", .eType = 0x2F},
        {.name = "GB_EXPERIENCETABLEALT", .eType = 0x30},
        {.name = "GB_TIERED_LOOT_RUN_LEVELS", .eType = 0x31},
        {.name = "GB_TRANSMUTE_RECIPES", .eType = 0x32, .maxElements = 16},    // TransmuteRecipes
        {.name = "GB_CURRENCY_CONVERSIONS", .eType = 0x33, .maxElements = 22}  // CurrencyConversions
    };

}  // namespace d3
