#pragma once

#include <array>

#include "d3/types/enums.hpp"
#include "nn/os/os_mutex_type.hpp"
#include "nn/fs/fs_types.hpp"
#include "bits/allocator.h"
#include "bits/stl_function.h"
#include "bits/unordered_map.h"
#include "bits/functional_hash.h"
#include "../idadefs.h"

namespace Blizzard {

    namespace Util {
        using BitField = _BYTE[0x20];
    }  // namespace Util

    namespace Time {
        using Timestamp   = int64;
        using Microsecond = uint64;
        using Millisecond = uint32;
        using Second      = uint32;
    }  // namespace Time

    namespace Thread {
        struct nnTlsSlot {
            uint32_t _innerValue;
        };
        using TLSDestructor = void (*)(void *);

        struct TLSSlot {
            Blizzard::Thread::TLSDestructor destructor;
            bool                            initialized;
            nnTlsSlot                       tSlot;
        };
    }  // namespace Thread

    namespace Lock {
        using Mutex  = nn::os::MutexType;
        using RMutex = nn::os::MutexType;
    }  // namespace Lock

    namespace File {
        using INODE = int32;
    }  // namespace File

    namespace Storage {
        struct StorageUnit;
        struct StorageUnitHandle {
            Blizzard::File::INODE           mNode;
            Blizzard::Storage::StorageUnit *mUnit;
        };
    }  // namespace Storage

    namespace File {
        struct ALIGNED(4) FileInfo {
            char                     *path;
            int64                     size;
            int64                     sparseSize;
            int32                     attributes;
            Blizzard::Time::Timestamp createTime;
            Blizzard::Time::Timestamp modTime;
            Blizzard::Time::Timestamp accessTime;
            int                       existence;
            bool                      regularFile;
        };

        struct StreamingInfo {
            Blizzard::Storage::StorageUnitHandle storagenode;
            Blizzard::File::INODE                inode;
            int32                                streamRefCount;
            Blizzard::File::FileInfo             streaminginfo;
        };

        struct StreamRecord {
            nn::fs::FileHandle             fileHandle;
            int                            mode;
            bool                           infoValid;
            Blizzard::File::FileInfo       info;
            Blizzard::File::StreamingInfo *streaming;
            char                          *pathname;
        };

        typedef StreamRecord *Stream;

    }  // namespace File

}  // namespace Blizzard

namespace blz {

    template<typename T>
    using char_traits = std::char_traits<T>;

    template<typename T>
    using stlallocator = std::allocator<T>;

    template<typename... Ts>
    struct allocator {
        int8_t gap0[1];
    };

    template<typename T>
    using hash = std::hash<T>;

    template<typename T>
    using stlequal_to = std::equal_to<T>;

    template<typename... Ts>
    struct equal_to {
        int8_t gap0[1];
    };

    template<typename FirstT, typename SecondT>
    struct pair {
        FirstT  first;
        SecondT second;
    };

    template<typename CharT, typename TraitsT = blz::char_traits<CharT>, typename AllocatorT = blz::allocator<CharT>>
    struct basic_string {
        using value_type = CharT;
        using pointer    = value_type *;

        pointer   m_elements;
        size_type m_size;
        uint64_t  m_capacity : 63;
        uint64_t  m_capacity_is_embedded : 1;
        CharT     m_storage[16];

        basic_string();
        basic_string(const char *, u32);
        ~basic_string();
    };

    template<typename T, typename AllocatorT = blz::allocator<T>>
    struct vector {
        using value_type = T;
        using pointer    = value_type *;

        pointer   m_elements;
        size_type m_size;
        uint64_t  m_capacity : 63;
        uint64_t  m_capacity_is_embedded : 1;
    };

    struct _shared_ptr_manager {
        int (**_vptr$_shared_ptr_manager)(void) = (*&_vptr$_shared_ptr_manager);
        int32_t m_use_count                     = 0;
        int32_t m_weak_count                    = 0;
    };

    template<typename T1, bool T2>
    struct _shared_ptr_base {
        T1 *m_pointer;
    };

    template<typename T>
    struct shared_ptr : blz::_shared_ptr_base<T, false> {
        blz::_shared_ptr_manager *m_manager = new _shared_ptr_manager;
    };

    // typedef basic_string<char, blz::char_traits<char>, blz::allocator<char>> string;
    using string = basic_string<char, blz::char_traits<char>, blz::allocator<char>>;

    template<typename T>
    struct chained_hash_node {
        chained_hash_node<T> *next;
        T                     val;
    };

    /* 18749 */
    // typedef blz::chained_hash_node<blz::pair<const int, int>> blz::chained_hash_table<blz::unordered_map_traits<int, int>, blz::hash<int>, blz::equal_to<int>, blz::allocator<blz::pair<const int, int>>>::node_type;
    template<typename T>
    struct ALIGNED(8) chained_hash_table {
        size_t         m_bucket_count;
        T::node_type **m_buckets;
        size_t         m_entry_count;
        float          m_max_load_factor;
    };

    /* 18747 */
    // struct ALIGNED(8) blz::chained_hash_table<blz::unordered_map_traits<int, int>, blz::hash<int>, blz::equal_to<int>, blz::allocator<blz::pair<const int, int>>> {
    //     size_t                                                                                                                                                   m_bucket_count;
    //     blz::chained_hash_table<blz::unordered_map_traits<int, int>, blz::hash<int>, blz::equal_to<int>, blz::allocator<blz::pair<const int, int>>>::node_type **m_buckets;
    //     size_t                                                                                                                                                   m_entry_count;
    //     float                                                                                                                                                    m_max_load_factor;
    // };

    // template<typename T1, typename T2>
    // struct unordered_map : blz::chained_hash_table<blz::unordered_map_traits<T1, T1>, blz::hash<T1>, blz::equal_to<T1>, blz::allocator<blz::pair<const T2, T1>>> {};
    // <T1, T1, blz::hash<T1>, blz::equal_to<T1>, blz::allocator<blz::pair<const T2, T1>>>

}  // namespace blz

namespace Console::Online {

    struct LobbyService {
        int (**_vptr$LobbyService)(void);
    };

    struct bdLobbyEventHandler {
        int (**_vptr$bdLobbyEventHandler)(void);
    };

    struct LobbyServiceInternal : LobbyService, bdLobbyEventHandler {
        struct ALIGNED(8) UpdateFileInfo {
            // blz::embedded_string<128> (0x98)
            _BYTE  m_szFilename[0x98];
            _BYTE  m_szTOCFilename[0x98];
            uint32 m_uFileSize;
            _BYTE  _pad0134[0x4];
            // blz::string (0x28)
            _BYTE m_szBuffer[0x28];
        };

        struct ALIGNED(8) TaskManager {
            // blz::list<bdReference<Console::Online::Task>> (0x18)
            _BYTE m_tTasks[0x18];
            int32 m_bRunning;
            _BYTE _pad001C[0x4];
        };

        enum ConnectionState : int32 {
            CONNSTATE_INACTIVE        = 0x0,
            CONNSTATE_RESOLVE         = 0x1,
            CONNSTATE_RESOLVE_PENDING = 0x2,
            CONNSTATE_AUTH_START      = 0x3,
            CONNSTATE_AUTH_PENDING    = 0x4,
            CONNSTATE_CONNECT         = 0x5,
            CONNSTATE_CONNECTING      = 0x6,
            CONNSTATE_CONNECTED       = 0x7,
            CONNSTATE_DISCONNECTING   = 0x8,
            CONNSTATE_DISCONNECTED    = 0x9,
        };

        using SubscriptionEvent = _BYTE[0x30];
        using ConnectEvent      = SubscriptionEvent;
        using DisconnectEvent   = SubscriptionEvent;
        using NewMailEvent      = SubscriptionEvent;
        using bdOnlineUserID    = bdUInt64;

        bool                        m_bFatalErrorOccurred;          // 0x10
        _BYTE                       _pad0011[0x3];
        int32                       m_nUserIndex;                   // 0x14
        bdTitleID                   m_idTitle;                      // 0x18
        ConnectionState             m_eConnectionState;             // 0x1C
        bool                        m_bWasConnected;                // 0x20
        _BYTE                       _pad0021[0x7];
        bdLobbyService             *m_ptLobbyService;               // 0x28
        bdAuthSwitch               *m_ptAuthService;                // 0x30
        bdNChar8                    m_szNickNameUsedForAuth[32];    // 0x38
        ConnectEvent                m_tConnectEvent;                // 0x58
        DisconnectEvent             m_tDisconnectEvent;             // 0x88
        NewMailEvent                m_tNewMailEvent;                // 0xB8
        int32                       m_bRunningTokenAuthentication;  // 0xE8 (BOOL_0)
        int32                       m_bAttemptConnectForNX64;       // 0xEC (BOOL_0)
        uintptr_t                   m_pfnVerifyNSOCallback;         // 0xF0 (Console::Online::VerifyNSOCallback)
        int32                       m_bReconnect;                   // 0xF8 (BOOL_0)
        int32                       m_bReconnectWhenSignedIn;       // 0xFC (BOOL_0)
        Blizzard::Time::Millisecond m_uLastReconnectAttemptMS;      // 0x100
        Blizzard::Time::Millisecond m_uReconnectDelayMS;            // 0x104
        bdOnlineUserID              m_idOnlineUser;                 // 0x108
        int32                       m_bCheckedForBnetAccount;       // 0x110 (BOOL_0)
        int32                       m_nBalanceVersion;              // 0x114
        UpdateFileInfo              m_atUpdateFiles[2];             // 0x118
        int32                       m_nFileInProgress;              // 0x3D8
        _BYTE                       _pad03DC[0x4];
        TaskManager                 m_tTaskManager;                 // 0x3E0
    };
    // auto test = sizeof(LobbyServiceInternal);
    // auto test = offsetof(LobbyServiceInternal, m_eConnectionState);
}  // namespace Console::Online

namespace google::protobuf {

    using int32  = int32_t;
    using uint32 = uint32_t;
    using int64  = int64_t;
    using uint64 = uint64_t;

    struct MessageLite {
        /* TODO: */  // uintptr_t __vftable;
        // int (**_vptr$MessageLite)(void);
        int (**_vptr$MessageLite)(void) = (*&_vptr$MessageLite);
    };

    /* TODO: */
    struct Message : MessageLite {};

    struct UnknownFieldSet;

    struct UnknownField {
        google::protobuf::uint32 number_;
        google::protobuf::uint32 type_;
        union $8D969D3D8B1F7C70BA062C04483EF129 {
            google::protobuf::uint64 varint_;
            google::protobuf::uint32 fixed32_;
            google::protobuf::uint64 fixed64_;
            union $C0F9398B6E4BE334723439A8C7E0BC65 {
                blz::string *string_value_;
            } length_delimited_;
            google::protobuf::UnknownFieldSet *group_;
        } _anon_0;
    };

    struct UnknownFieldSet {
        blz::vector<UnknownField, blz::allocator<UnknownField>> *fields_;
    };

    struct UnknownFieldUnion {
        uint32 number_;
        uint32 type_;
        union {  // $8D969D3D8B1F7C70BA062C04483EF129 _anon_0;
            uint64 varint_;
            uint32 fixed32_;
            uint64 fixed64_;

            union {
                blz::string *string_value_;
            } length_delimited_;

            UnknownFieldSet *group_;
        } _anon_0;  // // $8D969D3D8B1F7C70BA062C04483EF129 _anon_0;
    };

    struct UnknownFieldStable {
        uint32 number_;
        uint32 type_;
        struct {
            uint64 varint_;
            uint32 fixed32_;
            uint64 fixed64_;
            struct {
                blz::string *string_value_;
            } length_delimited_;
            UnknownFieldSet *group_;
        };
    };

    namespace internal {
        struct ALIGNED(8) RepeatedField_Base {
            size_t *elements_;
            int     current_size_;
            int     total_size_;
        };
    };  // namespace internal

    template<typename Element>
    class RepeatedField final : private internal::RepeatedField_Base {};

    namespace internal {
        struct ALIGNED(8) RepeatedPtrFieldBase {
            void **elements_;
            int    current_size_;
            int    allocated_size_;
            int    total_size_;
        };
    };  // namespace internal

    template<typename Element>
    class RepeatedPtrField final : private internal::RepeatedPtrFieldBase {};

};  // namespace google::protobuf

namespace D3::Items {
    struct ALIGNED(8) Mail : std::array<_BYTE, 0x68> {};
    struct SavedItem : std::array<_BYTE, 0x50> {};
    struct Generator : std::array<_BYTE, 0xF0> {};
};  // namespace D3::Items

namespace D3::Achievements {

    struct ALIGNED(8) AchievementUpdateRecord : google::protobuf::Message {
        google::protobuf::UnknownFieldSet _unknown_fields_;
        google::protobuf::uint32          _has_bits_[1];
        int                               _cached_size_;
        google::protobuf::uint64          achievement_id_;
        google::protobuf::int32           completion_;
    };

    struct ALIGNED(8) CriteriaUpdateRecord : google::protobuf::Message {
        google::protobuf::UnknownFieldSet _unknown_fields_;
        google::protobuf::uint32          _has_bits_[1];
        int                               _cached_size_;
        google::protobuf::uint32          criteria_id_32_and_flags_8_;
        google::protobuf::uint32          start_time_32_;
        google::protobuf::uint32          quantity_32_;
    };

    struct Snapshot : google::protobuf::Message {
        google::protobuf::UnknownFieldSet                                             _unknown_fields_;
        google::protobuf::uint32                                                      _has_bits_[1];
        int                                                                           _cached_size_;
        google::protobuf::RepeatedPtrField<D3::Achievements::AchievementUpdateRecord> achievement_snapshot_;
        google::protobuf::RepeatedPtrField<D3::Achievements::CriteriaUpdateRecord>    criteria_snapshot_;
        google::protobuf::uint64                                                      header_;
        blz::string                                                                  *content_handle_;
    };
};  // namespace D3::Achievements

namespace D3::ChallengeRifts {
    struct ChallengeData;
    // struct ChallengeData : google::protobuf::Message {
    //     google::protobuf::UnknownFieldSet _unknown_fields_;
    //     google::protobuf::uint32          _has_bits_[1];
    //     int                               _cached_size_;
    //     google::protobuf::int64           challenge_start_unix_time_;
    //     google::protobuf::int64           challenge_last_broadcast_unix_time_;
    //     google::protobuf::uint32          challenge_number_;
    //     float                             challenge_nephalem_orb_multiplier_;
    //     google::protobuf::int64           challenge_end_unix_time_console_;
    //     google::protobuf::uint64          challenge_hash_;

    //     ChallengeData() :
    //         _unknown_fields_() {CData_ctor(this)};
    // };
}  // namespace D3::ChallengeRifts

namespace D3::Hero {
    using SavedDefinition = uintptr_t;
}

namespace D3::OnlineService {
    using GameAccountHandle = uintptr_t;
}

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

};  // namespace D3::Account

namespace D3::Leaderboard {
    using RiftSnapshot = uintptr_t;
    struct ALIGNED(8) WeeklyChallengeData;  // : google::protobuf::Message {
    //     google::protobuf::UnknownFieldSet     _unknown_fields_;
    //     google::protobuf::uint32              _has_bits_[1];
    //     google::protobuf::int32               _cached_size_;  // int
    //     D3::Leaderboard::RiftSnapshot        *rift_snapshot_;
    //     D3::Hero::SavedDefinition            *hero_snapshot_;
    //     D3::Account::SavedDefinition         *account_snapshot_;
    //     D3::OnlineService::GameAccountHandle *game_account_id_;
    //     google::protobuf::uint32              bnet_account_id_;

    //     // WeeklyChallengeData() :
    //     //     _unknown_fields_() {};
    // };
}  // namespace D3::Leaderboard

namespace D3::CS {

    struct AccountData : google::protobuf::Message {
        google::protobuf::UnknownFieldSet                                 _unknown_fields_;
        google::protobuf::uint32                                          _has_bits_[1];
        int                                                               _cached_size_;
        D3::Account::Digest                                              *digest_;
        google::protobuf::RepeatedPtrField<D3::Account::AccountPartition> partitions_;
        // D3::Items::CurrencySavedData                                     *account_wide_currency_data_;
    };
};  // namespace D3::CS

namespace OnlineService {

    using HeroId        = uint64;
    using AccountId     = uint32;
    using GameAccountId = uint32;

    struct ItemId {
        uint64 m_high;
        uint64 m_low;
    };

    struct GameAccountHandle {
        OnlineService::GameAccountId m_id;
        XFourCC                      m_program;
        uint32                       m_region;
    };

    struct ALIGNED(8) GameId {
        uint64 m_uMatchmakerGuid;
        uint64 m_uGameServerGuid;
        uint32 m_uGameInstanceId;
    };

    struct PlayerResolveData {
        OnlineService::HeroId            uHeroId;
        OnlineService::AccountId         uBnetAccountId;
        OnlineService::GameAccountHandle tAccountId;
        char                             szAccountName[128];
        Team                             eTeam;
        D3::Account::SavedDefinition     tAccountDefinition;
        _BYTE                            tHeroDefinition[0x88];  // D3::Hero::SavedDefinition
        _BYTE                            tMails[0x30];           // D3::Items::Mails
        uint32                           uResolveResultFlags;
        uint64                           uAuthToken;
        _BYTE                            tRawLicenses[0x18];      // OnlineServiceServer::LicenseArray
        _BYTE                            tContentLicenses[0x18];  // OnlineService::ContentLicenseArray
        uint32                           uSessionFlags;
        uint32                           uLastChallengeRewardEarned;

        blz::vector<blz::pair<unsigned long, unsigned long>, blz::allocator<blz::pair<unsigned long, unsigned long>>> tChallengeRiftPersonalBests;

        int32                  nControllerIndex;
        int32                  nPartyIndex;
        BOOL                   bIsPrimary;
        BOOL                   bIsGuest;
        BOOL                   bIsLocal;
        uint32                 uConsoleLicenses;
        XLocale                eLocale;
        ServerSocketID         idServerSocket;
        uint32                 uJoinFlags;
        Blizzard::Time::Second tAdded;
        uint64                 nTeamId;
        float                  teamRating;
        float                  teamVariance;
        float                  individualRating;
        float                  individualVariance;
    };
}  // namespace OnlineService

namespace Console::Online {

    enum StorageResult : int32 {
        STORAGE_SUCCESS               = 0x0,
        STORAGE_FAILED                = 0x1,
        STORAGE_FAILED_NOT_CONNECTED  = 0x2,
        STORAGE_FAILED_FILE_NOT_FOUND = 0x3,
        STORAGE_FAILED_DATA_INVALID   = 0x4,
    };

    struct ALIGNED(8) StorageGetFileCallback {
        uintptr_t *m_impl;
        //   OnlineService::Callback<void (Console::Online::StorageResult,blz::shared_ptr<blz::basic_string<char,blz::char_traits<char>,blz::allocator<char> > >)>::node_t *m_impl;
    };
}  // namespace Console::Online
