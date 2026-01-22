#pragma once

#include "d3/types/allocators.hpp"
#include "nn/os/os_mutex_type.hpp"

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
    s32                                               m_nCount;
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
    s32        m_nArraySize;
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
    s32         m_nHashMask;
    s32         m_nCount;
    TAllocator *m_arAssoc;      // XChainedFixedBlockAllocator<XMapAssoc<T1, T3>>
    THashTable  m_arHashTable;  // XGrowableArray<XMapAssoc<T1, T3> *, XMapAssoc<T2, T4> *>
};

template<typename T1, typename T2>
struct XMapMemoryPool : XChainedFixedBlockAllocator<XMapAssoc<T1, T2>> {};

template<typename T1, typename T2, typename T3, typename T4, typename AllocatorT, int arr_size>
struct XPooledMap_Sub : XBaseMap<T1, T2, T3, T4, AllocatorT, /* XArrayPregrown */ XGrowableArray<XMapAssoc<T1, T3> *, XMapAssoc<T2, T4> *>, arr_size> {};

template<typename T1, typename T2, typename T3, typename T4, int arr_size>  // <unsigned int,unsigned int,__int64,__int64,32>
struct ALIGNED(8) XPooledMap {
    XPooledMap_Sub<T1, T2, T3, T4, XMapMemoryPool<T1, T3>, arr_size> m_map;
    BOOL                                                             m_ConstructorCalled;
};

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
    CHAR szName[256];
    u32  dwMaximumCount;
    u32  dwElementSize;
    u16  wHighestUsedIndex;
    u32  dwTotalCount;
    u16  wNextIdentifier;
    u16  wFirstFreeItem;
    u32  dwFreeCount;
    BOOL fGrowable;
};

template<typename T, typename Int>
struct TDataArray : CDataArrayBase {
    T                         *ptData;
    XMemoryAllocatorInterface *ptMemoryInterface;
};
