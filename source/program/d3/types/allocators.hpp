#pragma once

#include "d3/types/hexrays.hpp"

struct XMemoryAllocatorInterface {
    /* TODO: */
    uintptr_t vftable;
};

struct CCookie {
    s32 m_nCookie;
};

struct FixedBlockAllocator {
    struct XFreeNode {
        XFreeNode *m_pNext;
    };

    u8                        *m_pMemBlocks;
    XFreeNode                 *m_ptFreeList;
    s32                        m_nBlockCount;
    s32                        m_nBlockSize;
    s32                        m_nMaxBlockAlloced;
    u32                        m_dwFlags;
    u32                        m_dwUserData;
    s32                        m_nFreeBlockCount;
    XMemoryAllocatorInterface *m_ptMemoryInterface;
    u8                        *m_pbBlockFree;
    u32                        m_nBlockFreeLen;
    CCookie                    m_Cookie;
};

struct ALIGNED(8) ChainedFixedBlockAllocator {
    struct XFBANode {
        XFBANode           *pNext;
        FixedBlockAllocator tAlloc;
        char                pPadding[8];
    };

    s32                        m_nBlockSize;
    s32                        m_nBlockCount;
    s32                        m_nNumAllocators;
    XFBANode                  *m_pHeadAlloc;
    u8                         m_ucFlags;
    CHAR                       m_eGrow;
    u16                        m_nGrowParam;
    XMemoryAllocatorInterface *m_ptMemoryInterface;
    CCookie                    m_Cookie;
};

template<typename T>
struct XPtr64 {
    union {
        u64 m_uPtr;
        T  *m_pDebugPtr;
    } _anon_0;
};

struct XDynamicAllocator {
    XPtr64<u8>                        m_pMem;
    s32                               m_nSize;
    u8                                m_ucFlags;
    CHAR                              m_eGrowAlgorithm;
    u16                               m_nGrowParam;
    XPtr64<XMemoryAllocatorInterface> m_ptMemoryInterface;
    CCookie                           m_Cookie;
};
