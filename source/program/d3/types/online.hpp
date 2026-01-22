#pragma once

#include "d3/types/blizzard.hpp"
#include "d3/types/enums.hpp"

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
            _BYTE m_szFilename[0x98];
            _BYTE m_szTOCFilename[0x98];
            u32   m_uFileSize;
            _BYTE _pad0134[0x4];
            // blz::string (0x28)
            _BYTE m_szBuffer[0x28];
        };

        struct ALIGNED(8) TaskManager {
            // blz::list<bdReference<Console::Online::Task>> (0x18)
            _BYTE m_tTasks[0x18];
            s32   m_bRunning;
            _BYTE _pad001C[0x4];
        };

        enum ConnectionState : s32 {
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
        s32                         m_nUserIndex;                   // 0x14
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
        s32                         m_bRunningTokenAuthentication;  // 0xE8 (BOOL_0)
        s32                         m_bAttemptConnectForNX64;       // 0xEC (BOOL_0)
        uintptr_t                   m_pfnVerifyNSOCallback;         // 0xF0 (Console::Online::VerifyNSOCallback)
        s32                         m_bReconnect;                   // 0xF8 (BOOL_0)
        s32                         m_bReconnectWhenSignedIn;       // 0xFC (BOOL_0)
        Blizzard::Time::Millisecond m_uLastReconnectAttemptMS;      // 0x100
        Blizzard::Time::Millisecond m_uReconnectDelayMS;            // 0x104
        bdOnlineUserID              m_idOnlineUser;                 // 0x108
        s32                         m_bCheckedForBnetAccount;       // 0x110 (BOOL_0)
        s32                         m_nBalanceVersion;              // 0x114
        UpdateFileInfo              m_atUpdateFiles[2];             // 0x118
        s32                         m_nFileInProgress;              // 0x3D8
        _BYTE                       _pad03DC[0x4];
        TaskManager                 m_tTaskManager;                 // 0x3E0
    };

}  // namespace Console::Online
