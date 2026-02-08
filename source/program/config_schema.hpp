#pragma once

#include "program/config.hpp"
#include "tomlplusplus/toml.hpp"

#include <span>
#include <string_view>

namespace d3::config_schema {

    enum class ValueKind : u8 {
        Bool,
        U32,
        Float,
        String,
    };

    enum class RestartPolicy : u8 {
        RuntimeSafe,
        RestartRequired,
    };

    struct Entry {
        std::string_view                  section;
        std::string_view                  key;   // canonical key for writes
        std::span<const std::string_view> keys;  // key aliases for reads

        ValueKind     kind    = ValueKind::Bool;
        RestartPolicy restart = RestartPolicy::RestartRequired;

        // GUI metadata (optional).
        const char *tr_label       = nullptr;
        const char *label_fallback = nullptr;
        const char *tr_help        = nullptr;
        const char *help_fallback  = nullptr;

        // IO policy.
        bool omit_if_empty = false;  // strings only

        // Numeric metadata (optional).
        float min_f  = 0.0f;
        float max_f  = 0.0f;
        float step_f = 0.0f;
        u32   min_u  = 0;
        u32   max_u  = 0;
        u32   step_u = 0;

        // Accessors. Only the ones matching `kind` are expected to be non-null.
        bool (*get_bool)(const PatchConfig &) = nullptr;
        void (*set_bool)(PatchConfig &, bool) = nullptr;
        u32 (*get_u32)(const PatchConfig &)   = nullptr;
        void (*set_u32)(PatchConfig &, u32)   = nullptr;
        // Optional string parsers for numeric settings (allows values like "1080p").
        u32 (*parse_u32_str)(std::string_view, u32 fallback)       = nullptr;
        float (*get_float)(const PatchConfig &)                    = nullptr;
        void (*set_float)(PatchConfig &, float)                    = nullptr;
        float (*parse_float_str)(std::string_view, float fallback) = nullptr;
        const std::string *(*get_string)(const PatchConfig &)      = nullptr;
        void (*set_string)(PatchConfig &, std::string_view)        = nullptr;
    };

    auto Entries() -> std::span<const Entry>;
    auto EntriesForSection(std::string_view section) -> std::span<const Entry>;
    auto FindEntry(std::string_view section, std::string_view key) -> const Entry *;

    // Apply schema-backed settings from a TOML root table into a config (subset only).
    void ApplyTomlTable(PatchConfig &config, const toml::table &root);

    // Insert schema-backed settings into a TOML root table (subset only).
    void InsertIntoToml(toml::table &root, const PatchConfig &config);

}  // namespace d3::config_schema
