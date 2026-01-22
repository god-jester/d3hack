#pragma once

#include "d3/types/d3_achievements.hpp"
#include "d3/types/d3_items.hpp"
#include "d3/types/protobuf.hpp"

namespace D3::Account {

    struct ALIGNED(4) AvengerVictim : google::protobuf::Message {
        google::protobuf::UnknownFieldSet _unknown_fields_;
        google::protobuf::uint32          _has_bits_[1];
        google::protobuf::int32           _cached_size_;  // int
        google::protobuf::uint32          gbid_class_;
        bool                              is_female_;
    };

    struct ALIGNED(8) Avenger : google::protobuf::Message {
        google::protobuf::UnknownFieldSet                              _unknown_fields_;
        google::protobuf::uint32                                       _has_bits_[1];
        google::protobuf::int32                                        _cached_size_;  // int
        blz::string                                                   *avenger_name_;
        google::protobuf::uint32                                       deprecated_player_kills_;
        google::protobuf::int32                                        deprecated_monster_sno_;
        bool                                                           deprecated_resolved_;
        google::protobuf::int32                                        deprecated_result_;  // int
        google::protobuf::uint64                                       sent_from_;
        google::protobuf::RepeatedPtrField<D3::Account::AvengerVictim> victims_;
        google::protobuf::int32                                        affix_bucket_;
    };

    struct AvengerData : google::protobuf::Message {
        google::protobuf::UnknownFieldSet _unknown_fields_;
        google::protobuf::uint32          _has_bits_[1];
        google::protobuf::int32           _cached_size_;  // int
        D3::Account::Avenger             *deprecated_avenger_hardcore_;
        D3::Account::Avenger             *avenger_solo_;
        D3::Account::Avenger             *avenger_friends_;
    };

    struct ALIGNED(8) ConsoleChallengeRiftReward : google::protobuf::Message {
        google::protobuf::UnknownFieldSet                        _unknown_fields_;
        google::protobuf::uint32                                 _has_bits_[1];
        google::protobuf::int32                                  _cached_size_;  // int
        google::protobuf::RepeatedPtrField<D3::Items::SavedItem> items_;
        google::protobuf::uint32                                 challenge_rift_;
        google::protobuf::uint32                                 create_time_;
        google::protobuf::uint32                                 season_earned_;
    };

    struct ConsoleChallengeRiftPersonalBest : google::protobuf::Message {
        google::protobuf::UnknownFieldSet _unknown_fields_;
        google::protobuf::uint32          _has_bits_[1];
        google::protobuf::int32           _cached_size_;  // int
        google::protobuf::uint64          id_;
        google::protobuf::uint64          score_;
        google::protobuf::int64           timestamp_;
    };

    struct ConsoleData : google::protobuf::Message {
        google::protobuf::UnknownFieldSet                                                 _unknown_fields_;
        google::protobuf::uint32                                                          _has_bits_[1];
        google::protobuf::int32                                                           _cached_size_;  // int
        D3::Achievements::Snapshot                                                       *achievement_snapshot_;
        google::protobuf::uint32                                                          version_required_;
        google::protobuf::int32                                                           highest_completed_difficulty_deprecated_;
        D3::Account::AvengerData                                                         *avenger_data_;
        bool                                                                              has_demo_save_;
        bool                                                                              has_bnet_account_;
        float                                                                             progress_;
        google::protobuf::uint32                                                          legacy_license_bits_;
        google::protobuf::uint32                                                          leaderboard_heal_achievements_;
        google::protobuf::RepeatedField<unsigned long>                                    leaderboard_heal_conquests_;
        google::protobuf::RepeatedPtrField<D3::Account::ConsoleChallengeRiftReward>       challenge_rift_reward_;
        google::protobuf::RepeatedPtrField<D3::Account::ConsoleChallengeRiftPersonalBest> challenge_rift_personal_best_;
        google::protobuf::uint32                                                          challenge_rift_personal_best_rift_number_;
        google::protobuf::uint32                                                          console_cube_base_seed_;
        google::protobuf::int32                                                           nfp_default_time_;
        google::protobuf::int32                                                           nfp_goblin_time_;
    };

    struct ALIGNED(8) AccountPartition : google::protobuf::Message {
        google::protobuf::UnknownFieldSet _unknown_fields_;
        google::protobuf::uint32          _has_bits_[1];
        google::protobuf::int32           _cached_size_;  //int
        // D3::AttributeSerializer::SavedAttributes *saved_attributes_;
        // D3::Items::ItemList                      *items_;
        // D3::ItemCrafting::CrafterSavedData       *crafter_data_;
        google::protobuf::int32  partition_id_;
        google::protobuf::uint32 alt_level_;
        // D3::OnlineService::EntityId              *gold_id_deprecated_;
        blz::string             *stash_icons_;
        google::protobuf::uint64 accepted_license_bits_;
        // D3::Items::CurrencySavedData             *currency_data_;
        // D3::Account::ConsolePartitionData        *console_partition_data_;
        google::protobuf::uint32 flags_;
    };

    struct ALIGNED(8) Digest : google::protobuf::Message {
        google::protobuf::UnknownFieldSet _unknown_fields_;
        google::protobuf::uint32          _has_bits_[1];
        int                               _cached_size_;
        // D3::OnlineService::EntityId                  *last_played_hero_id_;
        google::protobuf::uint32 version_;
        google::protobuf::uint32 flags_;
        // D3::Account::BannerConfiguration             *banner_configuration_;
        google::protobuf::uint64                      pvp_cooldown_;
        google::protobuf::uint64                      guild_id_;
        google::protobuf::uint32                      season_id_;
        google::protobuf::uint32                      stash_tabs_rewarded_from_seasons_;
        google::protobuf::RepeatedField<unsigned int> alt_levels_;
        blz::string                                  *patch_version_;
        google::protobuf::uint32                      rebirths_used_;
        bool                                          completed_solo_rift_;
        // D3::ChallengeRifts::AccountData              *challenge_rift_account_data_;
        google::protobuf::uint32 last_publish_time_;
    };

    struct ALIGNED(8) SavedDefinition : google::protobuf::Message {
        google::protobuf::UnknownFieldSet _unknown_fields_;
        google::protobuf::uint32          _has_bits_[1];
        int                               _cached_size_;
        D3::Account::Digest              *digest_;
        // D3::AttributeSerializer::SavedAttributes                         *saved_attributes_;
        blz::string                                                      *seen_tutorials_;
        google::protobuf::int64                                           num_vote_kicks_participated_in_;
        google::protobuf::uint32                                          version_;
        google::protobuf::uint32                                          create_time_;
        google::protobuf::int64                                           num_vote_kicks_initiated_;
        google::protobuf::int64                                           num_public_games_no_kick_;
        google::protobuf::int64                                           times_vote_kicked_;
        google::protobuf::RepeatedPtrField<D3::Account::AccountPartition> partitions_;
        // D3::AttributeSerializer::SavedAttributes                         *deprecated_saved_attributes_hardcore_;
        // D3::Items::ItemList                                              *deprecated_normal_shared_saved_items_;
        // D3::Items::ItemList                                              *deprecated_hardcore_shared_saved_items_;
        // D3::ItemCrafting::CrafterSavedData                               *deprecated_crafter_normal_data_;
        // D3::ItemCrafting::CrafterSavedData                               *deprecated_crafter_hardcore_data_;
        // D3::OnlineService::EntityId                                      *deprecated_gold_id_normal_;
        // D3::OnlineService::EntityId                                      *deprecated_gold_id_hardcore_;
        blz::string              *deprecated_stash_icons_normal_;
        google::protobuf::uint64  deprecated_accepted_license_bits_;
        blz::string              *deprecated_stash_icons_hardcore_;
        D3::Account::ConsoleData *console_data_;
        // D3::GameBalance::BitPackedGbidArray                              *account_wide_transmog_data_;
        // D3::CosmeticItems::CosmeticItemSavedData                         *account_wide_cosmetic_item_data_;
        // D3::Items::CurrencySavedData                                     *account_wide_currency_data_;
        // D3::Account::DeliveredRewards                                    *delivered_rewards_;
        // D3::Account::Consumables                                         *consumables_;
        google::protobuf::uint32 num_groups_created_deprecated_;
    };

}  // namespace D3::Account
