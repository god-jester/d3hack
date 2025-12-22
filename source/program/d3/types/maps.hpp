#pragma once

#include "nn/os/os_mutex_type.hpp"
#include "d3/types/enums.hpp"

struct XMemoryAllocatorInterface {
    /* TODO: */
    uintptr_t vftable;
};

struct CCookie {
    int32 m_nCookie;
};

struct FixedBlockAllocator {
    struct XFreeNode {
        XFreeNode *m_pNext;
    };

    uint8                     *m_pMemBlocks;
    XFreeNode                 *m_ptFreeList;
    int32                      m_nBlockCount;
    int32                      m_nBlockSize;
    int32                      m_nMaxBlockAlloced;
    uint32                     m_dwFlags;
    uint32                     m_dwUserData;
    int32                      m_nFreeBlockCount;
    XMemoryAllocatorInterface *m_ptMemoryInterface;
    uint8                     *m_pbBlockFree;
    uint32                     m_nBlockFreeLen;
    CCookie                    m_Cookie;
};

struct ALIGNED(8) ChainedFixedBlockAllocator {
    struct XFBANode {
        XFBANode           *pNext;
        FixedBlockAllocator tAlloc;
        char                pPadding[8];
    };

    int32                      m_nBlockSize;
    int32                      m_nBlockCount;
    int32                      m_nNumAllocators;
    XFBANode                  *m_pHeadAlloc;
    uint8                      m_ucFlags;
    CHAR                       m_eGrow;
    uint16                     m_nGrowParam;
    XMemoryAllocatorInterface *m_ptMemoryInterface;
    CCookie                    m_Cookie;
};

template<typename T>
struct XPtr64 {
    union {
        uint64_t m_uPtr;
        T       *m_pDebugPtr;
    } _anon_0;
};

struct XDynamicAllocator {
    XPtr64<unsigned char>             m_pMem;
    int32                             m_nSize;
    uint8                             m_ucFlags;
    CHAR                              m_eGrowAlgorithm;
    uint16                            m_nGrowParam;
    XPtr64<XMemoryAllocatorInterface> m_ptMemoryInterface;
    CCookie                           m_Cookie;
};

template<typename KeyT, typename ValueT>
struct XMapAssoc {
    XMapAssoc<KeyT, ValueT> *m_pNext;
    KeyT                     m_Key;
    ValueT                   m_Value;
};

template<typename T>
struct XBaseListNode {
    T                 m_tData;
    XBaseListNode<T> *m_pPrev;
    XBaseListNode<T> *m_pNext;
};

template<typename T1, typename T2, typename AllocatorT>
struct ALIGNED(8) XBaseList {
    using XBaseListNodeType = XBaseListNode<T1>;
    XBaseList<T1, T2, AllocatorT>::XBaseListNodeType *m_pHead;
    XBaseList<T1, T2, AllocatorT>::XBaseListNodeType *m_pTail;
    int32                                             m_nCount;
    AllocatorT                                       *m_ptNodeAllocator;
};

template<typename T>
struct XChainedFixedBlockAllocator {  // <XBaseListNode<int>>
    ChainedFixedBlockAllocator m_tAlloc;
};

template<typename T>
struct XListMemoryPool : XChainedFixedBlockAllocator<XBaseListNode<T>> {};

template<typename T1, typename T2, typename AllocatorT>
struct XPooledList_Sub : XBaseList<T1, T2, AllocatorT> {};

template<typename T1, typename T2>
struct ALIGNED(8) XPooledList {  // using XPooledDataIDList      = _BYTE[0x28];  // XPooledList<int, int>;
    XPooledList_Sub<T1, T2, XListMemoryPool<T1>> m_list;

    BOOL m_ConstructorCalled;
    ~XPooledList();
};

template<typename T1, typename T2, typename AllocatorT>
struct XBaseArray {
    XPtr64<T1> m_pData;
    int32      m_nArraySize;
    AllocatorT m_MemAllocator;
};

template<typename T1, typename T2>
struct XGrowableArray_Sub : XBaseArray<T1, T2, XDynamicAllocator> {};

template<typename T1, typename T2>
struct ALIGNED(8) XGrowableArray {
    XGrowableArray_Sub<T1, T2> m_array;
    BOOL                       m_ConstructorCalled;
};

template<typename T1, typename T2, typename T3, typename T4, typename TAllocator, typename THashTable, int arr_size = 0>
struct XBaseMap {  // <int, int, int, int, XChainedFixedBlockAllocator<XMapAssoc<int, int>>, XGrowableArray<XMapAssoc<int, int> *, XMapAssoc<int, int> *>, 0> {
    int32       m_nHashMask;
    int32       m_nCount;
    TAllocator *m_arAssoc;      // XChainedFixedBlockAllocator<XMapAssoc<T1, T3>>
    THashTable  m_arHashTable;  // XGrowableArray<XMapAssoc<T1, T3> *, XMapAssoc<T2, T4> *>
};

template<typename T1, typename T2>
struct XMapMemoryPool : XChainedFixedBlockAllocator<XMapAssoc<T1, T2>> {};

template<typename T1, typename T2, typename T3, typename T4, typename AllocatorT, int arr_size>
struct XPooledMap_Sub : XBaseMap<T1, T2, T3, T4, AllocatorT, /* XArrayPregrown */ XGrowableArray<XMapAssoc<T1, T3> *, XMapAssoc<T2, T4> *>, arr_size> {};

// struct __cppobj XPooledMap_Sub<unsigned int,unsigned int,__int64,__int64,
//         XMapMemoryPool<unsigned int,__int64>,
//         32>
//     :
//     XBaseMap
//     <unsigned int,unsigned int,__int64,__int64,
//         XMapMemoryPool<unsigned int,__int64>,
//         XArrayPregrown
//             <XMapAssoc<unsigned int,__int64> *,XMapAssoc<unsigned int,__int64> *,32>
//         ,32>
// {
// };

template<typename T1, typename T2, typename T3, typename T4, int arr_size>  // <unsigned int,unsigned int,__int64,__int64,32>
struct ALIGNED(8) XPooledMap {
    XPooledMap_Sub<T1, T2, T3, T4, XMapMemoryPool<T1, T3>, arr_size> m_map;
    BOOL                                                             m_ConstructorCalled;
};

typedef XPooledMap<unsigned int, unsigned int, __int64, __int64, 32> InventoryMap;

template<typename T1, typename T2, typename T3, typename T4>
struct XGrowableMap_Sub : XBaseMap<T1, T2, T3, T4, XMapMemoryPool<T1, T3>, XGrowableArray<XMapAssoc<T1, T3> *, XMapAssoc<T2, T4> *>> {};  // , XChainedFixedBlockAllocator<XMapAssoc<int, int>>, XGrowableArray<XMapAssoc<int, int> *, XMapAssoc<int, int> *>, 0> {

template<typename T1, typename T2, typename T3, typename T4>
struct ALIGNED(8) XGrowableMap {
    XGrowableMap_Sub<T1, T2, T3, T4> m_map;
    BOOL                             m_ConstructorCalled;
};

template<typename T1, typename T2>
struct XGrowableMapTS {
    nn::os::MutexType /* Blizzard::Lock::RMutex */ m_tLock;
    XGrowableMap<T1, const T1 &, T2, const T2 &>   m_tMap;
};

struct CDataArrayBase {
    CHAR   szName[256];
    uint32 dwMaximumCount;
    uint32 dwElementSize;
    uint16 wHighestUsedIndex;
    uint32 dwTotalCount;
    uint16 wNextIdentifier;
    uint16 wFirstFreeItem;
    uint32 dwFreeCount;
    BOOL   fGrowable;
};

template<typename T, typename Int>
struct TDataArray : CDataArrayBase {
    T                         *ptData;
    XMemoryAllocatorInterface *ptMemoryInterface;
};

// struct __cppobj XGrowableArray_Sub<XMapAssoc<int, int> *, XMapAssoc<int, int> *> : XBaseArray<XMapAssoc<int, int> *, XMapAssoc<int, int> *, XDynamicAllocator> {
// };

// /* 581 */
// struct __cppobj __attribute__((aligned(8))) XGrowableArray<XMapAssoc<int, int> *, XMapAssoc<int, int> *> {
//     XGrowableArray_Sub<XMapAssoc<int, int> *, XMapAssoc<int, int> *> m_array;
//     BOOL                                                             m_ConstructorCalled;
// };

// struct XBaseMap
//     <int,int,Item *,Item *,
//         XChainedFixedBlockAllocator<XMapAssoc<int,Item *> >,
//         XGrowableArray<XMapAssoc<int,Item *> *,
//         XMapAssoc<int,Item *> *>,
//     0>
// {
//   int32 m_nHashMask;
//   int32 m_nCount;XMapMemoryPool
//   XChainedFixedBlockAllocator<XMapAssoc<int,Item *> > *m_arAssoc;
//   XGrowableArray<XMapAssoc<int,Item *> *,XMapAssoc<int,Item *> *> m_arHashTable;
// };

// struct __cppobj XGrowableMap_Sub<ACDID, ACDID, CCInfo, CCInfo &> :
// XBaseMap<ACDID, ACDID, CCInfo, CCInfo &,
// XChainedFixedBlockAllocator<XMapAssoc<ACDID, CCInfo>>,
// XGrowableArray<XMapAssoc<ACDID, CCInfo> *,
//  XMapAssoc<ACDID, CCInfo> *>, 0> {

//     XChainedFixedBlockAllocator<XMapAssoc<ACDID, CCInfo>> m_AssocAllocator;
// }

// /* 580 */
// struct __cppobj XChainedFixedBlockAllocator<XMapAssoc<int, int>> {
//     ChainedFixedBlockAllocator m_tAlloc;
// };

// template<typename T1, typename T2, typename AllocatorT>
// struct XPooledList_Sub : XBaseList<T1, T2, AllocatorT> {};

// /* 601 */
// struct __cppobj XMemoryAllocatorInterface {
//     int (**_vptr$XMemoryAllocatorInterface)(void);
// };

// /* 2876 */
// struct ChainedFixedBlockAllocator::XFBANode {
//     ChainedFixedBlockAllocator::XFBANode *pNext;
//     FixedBlockAllocator                   tAlloc;
//     char                                  pPadding[8];
// };
