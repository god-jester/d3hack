#pragma once

#include "symbols/common.hpp"
#include "d3/types/sno.hpp"
#include "d3/types/attributes.hpp"
#include "d3/gamebalance.hpp"

/*
    struct RequiredMessageHeader {
        uint16          dwSize;
        int8            nControllerIndex;
        int8            nPadding;
        GameMessageType eType;
    };

    struct PlayerLevel {
        RequiredMessageHeader tHeader;
        int32                 nPlayerIndex;
        int32                 nNewLevel;
    };

    struct DWordDataMessage {
        RequiredMessageHeader tHeader;
        uint32                dwData;
    };
*/

namespace d3 {
    void PopulatePlayerList(PlayerList *pList, const Player *tTarget = nullptr) {
        if (!_HOSTCHK) return;
        TPlayerPointerPool *ElementPool = PlayerListGetElementPool();
        PlayerList_ctor(pList, ElementPool);
        if (tTarget == nullptr)
            PlayerListGetAllInGame(pList, -1, 0, -1);
        else
            PlayerListGetSingle(pList, tTarget);
        // SigmaMemoryClear(&_pMessage, 8uLL);
    }

    void ShowError(const Player *tTarget, int32 eError) {
        if (!_HOSTCHK) return;
        PlayerList     pList;
        IntDataMessage _pMessage {};
        size_t         nMsgSize = sizeof(IntDataMessage);

        memset(&_pMessage, 0, nMsgSize);
        _pMessage.nData = eError;

        PopulatePlayerList(&pList);
        MessageSendToClient(&pList, SMSG_DISPLAY_ERROR, &_pMessage, nMsgSize);
    }

    void SyncFlags(const Player *tTarget, uint32 DigestFlags) {
        if (!_HOSTCHK) return;
        PlayerList       pList;
        DWordDataMessage _pMessage {};
        size_t           nMsgSize = sizeof(DWordDataMessage);

        memset(&_pMessage, 0, nMsgSize);
        _pMessage.dwData = DigestFlags;

        PopulatePlayerList(&pList, tTarget);
        MessageSendToClient(&pList, SMSG_ACCOUNT_FLAGS_SYNC + 1, &_pMessage, nMsgSize);
    }

    // DWordDataMessage    _pMessage;
    // DWordDataMessage _pMessage {
    //     .dwData = 4
    // };
    // PlayerList          pList;
    // TPlayerPointerPool *ElementPool = PlayerListGetElementPool();

    // void SyncFlags(const Player *tTarget) {
    //     PlayerList_ctor(&pList, ElementPool);
    //     // PlayerListGetAllInGame(&tTarget, -1, 0, -1);
    //     PlayerListGetSingle(&pList, tTarget);
    //     // SigmaMemoryClear(&_pMessage, 8uLL);
    //     memset(&_pMessage, 0, 8uLL);
    //     // *(_pMessage).tHeader.dwSize           = 0xC;
    //     // *(_pMessage).tHeader.nControllerIndex = 0x0;
    //     // *(_pMessage).tHeader.nPadding         = 0x0;
    //     // *(_pMessage).tHeader.eType            = 0x1A1;
    //     (&_pMessage)->dwData = 4;
    //     MessageSendToClient(&pList, 0x1A1, &_pMessage, 12u);
    //     // MessageSendToClient(&v34, 121, pMessage, 0x54u);
    // }

    //     GameMessageType __fastcall NameToGameMessageType(const char *szType) {
    //         int              v2;               // w22
    //         const CHAR      *v3;               // x1
    //         const char      *v4;               // x0
    //         int              v5;               // w21
    //         CRefString       v7;               // [xsp+0h] [xbp-50h] BYREF
    //         char            *pszMessageLabel;  // [xsp+18h] [xbp-38h] BYREF
    //         uint32           dwSize;           // [xsp+24h] [xbp-2Ch] BYREF
    //         XTypeDescriptor *pTypeDescriptor;  // [xsp+28h] [xbp-28h] BYREF

    //         v2 = 0;
    //         while (1) {
    //             CRefString(&v7);
    //             pTypeDescriptor = 0LL;
    //             dwSize          = 0;
    //             v3              = GetGameMessageTypeAndSize(v2 + 1, &pTypeDescriptor, &dwSize, &pszMessageLabel)
    //                                   ? pszMessageLabel
    //                                   : "Unknown GameMessageType";
    //             v7              = v3;
    //             v4              = v7.str();
    //             v5              = strcasecmp(v4, szType);
    //             // ~CRefString(&v7);
    //             if (!v5)
    //                 break;
    //             if (++v2 >= 0x249)
    //                 return 0;
    //         }
    //         return v2 + 1;
    //     }
    //     void __fastcall sSendMessageNew(const ConsoleCommand *tCmd, LPSTR uszOutput) {
    //         GameMessageType                                                              v4;                // w20
    //         uint8                                                                       *v5;                // x19
    //         Player                                                                      *v6;                // x0
    //         Player                                                                      *v7;                // x21
    //         TPlayerPointerPool                                                          *ElementPool;       // x0
    //         XBaseList<Player *, Player *, XListMemoryPool<Player *>>::XBaseListNodeType *m_pHead;           // x1
    //         XBaseListNode<Player *>                                                    **p_m_pPrev;         // x8
    //         const char                                                                  *v11;               // x19
    //         const char                                                                  *v12;               // x0
    //         PlayerList                                                                   listPlayers;       // [xsp+10h] [xbp-90h] BYREF
    //         CRefString                                                                   sError;            // [xsp+38h] [xbp-68h] BYREF
    //         uint32                                                                       v15;               // [xsp+50h] [xbp-50h] BYREF
    //         int32                                                                        nCurParam;         // [xsp+54h] [xbp-4Ch] BYREF
    //         uint8                                                                       *ptMessage;         // [xsp+58h] [xbp-48h] BYREF
    //         const char                                                                  *pszMessageLabel;   // [xsp+60h] [xbp-40h] BYREF
    //         uint32                                                                       dwMessageSize;     // [xsp+6Ch] [xbp-34h] BYREF
    //         const XTypeDescriptor                                                       *ptTypeDescriptor;  // [xsp+78h] [xbp-28h] BYREF
    //         const ConsoleCommand                                                        *ptPushGameMessageCmd;

    //         if (tCmd->nParameters > 0) {
    //             v4 = NameToGameMessageType(tCmd->apszParameters[0]);
    //             if (!v4) {
    //                 PRINT("Invalid message type %s!\n")
    //                 return;
    //             }
    //             ptPushGameMessageCmd = tCmd;
    //             ptTypeDescriptor     = 0LL;
    //             dwMessageSize        = 0;
    //             pszMessageLabel      = 0LL;
    //             GetGameMessageTypeAndSize(v4, &ptTypeDescriptor, &dwMessageSize, &pszMessageLabel);
    //             v5        = malloc(dwMessageSize);
    //             ptMessage = v5;
    //             nCurParam = 1;
    //             v15       = 0;
    //             CRefString::CRefString(&sError);
    //             if (sPushGameMessageField(&sError, &ptMessage, &v15, dwMessageSize, tCmd, &nCurParam, ptTypeDescriptor, 0LL, 0)) {
    //                 if (nCurParam == tCmd->nParameters && v15 < dwMessageSize && ((v15 + 7) & 0xFFFFFFF8) == dwMessageSize) {
    //                     v15                  = dwMessageSize;
    //                     ptPushGameMessageCmd = 0LL;
    //                     goto LABEL_13;
    //                 }
    //                 ptPushGameMessageCmd = 0LL;
    //                 if (v15 >= dwMessageSize) {
    //                 LABEL_13:
    //                     if (GameActiveGameCommonDataIsClient() && !strcasecmp(tCmd->pszCommand, "cmsg")) {
    //                         v6 = PlayerGetByPlayerIndex(tCmd->nPlayerIndex);
    //                         MessageSendToServer(v4, v5, dwMessageSize, v6);
    //                     }
    //                     if (GameActiveGameCommonDataIsServer()) {
    //                         if (!strcasecmp(tCmd->pszCommand, "smsg")) {
    //                             v7          = PlayerGetByPlayerIndex(tCmd->nPlayerIndex);
    //                             ElementPool = PlayerListGetElementPool();
    //                             PlayerList::PlayerList(&listPlayers, ElementPool);
    //                             PlayerListGetSingle(&listPlayers, v7);
    //                             MessageSendToClient(&listPlayers, v4, v5, dwMessageSize);
    //                             while (listPlayers.m_list.m_nCount) {
    //                                 m_pHead = listPlayers.m_list.m_pHead;
    //                                 if (!listPlayers.m_list.m_pHead) {
    //                                     DisplayInternalError(
    //                                         "Tried to remove head from list but it was missing.\n",
    //                                         "../Sigma/DataStructures/XBaseList.h",
    //                                         356,
    //                                         ERROR_INTERNAL_FATAL
    //                                     );
    //                                     (ExitProgram[0])(ERROR_INTERNAL_FATAL);
    //                                     JUMPOUT(0x79FDC8LL);
    //                                 }
    //                                 listPlayers.m_list.m_pHead = listPlayers.m_list.m_pHead->m_pNext;
    //                                 if (listPlayers.m_list.m_pHead)
    //                                     p_m_pPrev = &listPlayers.m_list.m_pHead->m_pPrev;
    //                                 else
    //                                     p_m_pPrev = &listPlayers.m_list.m_pTail;
    //                                 *p_m_pPrev = 0LL;
    //                                 --listPlayers.m_list.m_nCount;
    //                                 ChainedFixedBlockAllocator::Free(&listPlayers.m_list.m_ptNodeAllocator->m_tAlloc, m_pHead);
    //                             }
    //                         }
    //                     }
    //                     free(v5);
    //                     goto LABEL_26;
    //                 }
    //                 free(v5);
    //                 Blizzard::String::Format(
    //                     uszOutput,
    //                     0x400u,
    //                     "Error parsing message type %s:  Message too small!  Please notify a gameplay programmer!\n",
    //                     tCmd->apszParameters[0]
    //                 );
    //             } else {
    //                 v11 = tCmd->apszParameters[0];
    //                 v12 = CRefString::str(&sError);
    //                 PRINT("Error parsing message type %s:  %s\n", v11, v12)
    //             }
    //         LABEL_26:
    //             CRefString::~CRefString(&sError);
    //             return;
    //         }
    //         PRINT("You must specify a message type!\n")
    //     }
}  // namespace d3