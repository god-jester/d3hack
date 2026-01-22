#pragma once

#include "d3/types/blizzard.hpp"
#include "d3/types/blz_types.hpp"
#include "d3/types/enums.hpp"

#include "d3/types/d3_account.hpp"
#include "d3/types/d3_hero.hpp"
#include "d3/types/d3_items.hpp"

namespace OnlineService {

    using HeroId        = u64;
    using AccountId     = u32;
    using GameAccountId = u32;

    struct ItemId {
        u64 m_high;
        u64 m_low;
    };

    struct GameAccountHandle {
        OnlineService::GameAccountId m_id;
        XFourCC                      m_program;
        u32                          m_region;
    };

    struct ALIGNED(8) GameId {
        u64 m_uMatchmakerGuid;
        u64 m_uGameServerGuid;
        u32 m_uGameInstanceId;
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
        u32                              uResolveResultFlags;
        u64                              uAuthToken;
        _BYTE                            tRawLicenses[0x18];      // OnlineServiceServer::LicenseArray
        _BYTE                            tContentLicenses[0x18];  // OnlineService::ContentLicenseArray
        u32                              uSessionFlags;
        u32                              uLastChallengeRewardEarned;

        blz::vector<blz::pair<unsigned long, unsigned long>, blz::allocator<blz::pair<unsigned long, unsigned long>>> tChallengeRiftPersonalBests;

        s32                    nControllerIndex;
        s32                    nPartyIndex;
        BOOL                   bIsPrimary;
        BOOL                   bIsGuest;
        BOOL                   bIsLocal;
        u32                    uConsoleLicenses;
        XLocale                eLocale;
        ServerSocketID         idServerSocket;
        u32                    uJoinFlags;
        Blizzard::Time::Second tAdded;
        u64                    nTeamId;
        float                  teamRating;
        float                  teamVariance;
        float                  individualRating;
        float                  individualVariance;
    };

}  // namespace OnlineService
