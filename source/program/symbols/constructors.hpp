#pragma once

using StorageResult       = ::Console::Online::StorageResult;
using ChallengeData       = ::D3::ChallengeRifts::ChallengeData;
using WeeklyChallengeData = ::D3::Leaderboard::WeeklyChallengeData;

namespace blz {

    template<class CharT, class TraitsT, class AllocatorT>
    blz::basic_string<CharT, TraitsT, AllocatorT>::basic_string() {
        m_storage[0]           = 0;
        m_elements             = m_storage;
        m_size                 = 0;
        m_capacity             = (sizeof(m_storage) / sizeof(m_storage[0])) - 1;
        m_capacity_is_embedded = 1;
    }

    template<class CharT, class TraitsT, class AllocatorT>
    blz::basic_string<CharT, TraitsT, AllocatorT>::basic_string(const char *str, u32 dwSize) {
        m_storage[0]           = 0;
        m_elements             = m_storage;
        m_size                 = 0;
        m_capacity             = (sizeof(m_storage) / sizeof(m_storage[0])) - 1;
        m_capacity_is_embedded = 1;

        if (!str || !dwSize)
            return;

        const auto len = static_cast<size_t>(dwSize);
        auto      *buf = static_cast<char *>(SigmaMemoryNew(len + 1, 0, nullptr, true));
        if (!buf)
            return;

        SigmaMemoryMove(buf, const_cast<char *>(str), len);
        buf[len]               = '\0';
        m_elements             = buf;
        m_size                 = len;
        m_capacity             = len;
        m_capacity_is_embedded = 0;
    }

    template<class CharT, class TraitsT, class AllocatorT>
    blz::basic_string<CharT, TraitsT, AllocatorT>::~basic_string() {
        if (!m_elements)
            return;

        const CharT *storage_start = &m_storage[0];
        const CharT *storage_end   = storage_start + (sizeof(m_storage) / sizeof(m_storage[0]));
        if (m_elements < storage_start || m_elements >= storage_end)
            SigmaMemoryFree(m_elements, nullptr);
    }

}  // namespace blz

namespace D3 {
    namespace ChallengeRifts {
        struct ChallengeData : google::protobuf::Message {
            google::protobuf::UnknownFieldSet _unknown_fields_;
            google::protobuf::uint32          _has_bits_[1];
            google::protobuf::int32           _cached_size_;  // int
            google::protobuf::int64           challenge_start_unix_time_;
            google::protobuf::int64           challenge_last_broadcast_unix_time_;
            google::protobuf::uint32          challenge_number_;
            float                             challenge_nephalem_orb_multiplier_;
            google::protobuf::int64           challenge_end_unix_time_console_;
            google::protobuf::uint64          challenge_hash_;

            ChallengeData() :
                challenge_nephalem_orb_multiplier_(1.0f) { ChallengeData_ctor(this); }

            /*TODO: dtor */
            ~ChallengeData() { return; }
        };
    }  // namespace ChallengeRifts

    namespace Leaderboard {
        using RiftSnapshot = uintptr_t;
        struct ALIGNED(8) WeeklyChallengeData : google::protobuf::Message {
            google::protobuf::UnknownFieldSet     _unknown_fields_;
            google::protobuf::uint32              _has_bits_[1];
            google::protobuf::int32               _cached_size_;  // int
            D3::Leaderboard::RiftSnapshot        *rift_snapshot_;
            D3::Hero::SavedDefinition            *hero_snapshot_;
            D3::Account::SavedDefinition         *account_snapshot_;
            D3::OnlineService::GameAccountHandle *game_account_id_;
            google::protobuf::uint32              bnet_account_id_;

            WeeklyChallengeData() :
                _cached_size_(0) { WeeklyChallengeData_ctor(this); }

            /*TODO: dtor */
            ~WeeklyChallengeData() { return; }
        };
    }  // namespace Leaderboard
}  // namespace D3

template<class T1, class T2>
XPooledList<T1, T2>::~XPooledList() {
    XBaseListNode<T1>                     *m_pHead;
    XBaseListNode<T1>                     *m_pNext;
    XBaseListNode<T1>                    **p_pFinal;
    XListMemoryPool<T1>                   *m_ptNodeAllocator;
    ChainedFixedBlockAllocator::XFBANode  *m_pHeadAlloc;
    ChainedFixedBlockAllocator::XFBANode **p_m_pHeadAlloc;
    FixedBlockAllocator::XFreeNode        *p_pNext;
    u64                                    m_pMemBlocks;
    s32                                    m_nBlockCount;

    if (this->m_list.m_nCount) {
        // PRINT_EXPR("%s dtor entered, count %d", "XPooledList", this->m_list.m_nCount)
        while (1) {
            m_pHead              = this->m_list.m_pHead;
            m_pNext              = this->m_list.m_pHead->m_pNext;
            this->m_list.m_pHead = m_pNext;
            p_pFinal             = m_pNext ? &m_pNext->m_pPrev : &this->m_list.m_pTail;
            *p_pFinal            = nullptr;
            m_ptNodeAllocator    = this->m_list.m_ptNodeAllocator;
            --this->m_list.m_nCount;
            m_pHeadAlloc = m_ptNodeAllocator->m_tAlloc.m_pHeadAlloc;
            if (m_pHeadAlloc)
                break;
            p_m_pHeadAlloc = &m_ptNodeAllocator->m_tAlloc.m_pHeadAlloc;
            if ((m_ptNodeAllocator->m_tAlloc.m_ucFlags & 4) != 0)
                goto LABEL_15;
        LABEL_21:
            if (!this->m_list.m_nCount) {
                // PRINT_EXPR("%s dtor returned, count 0", "XPooledList")
                return;
            }
        }
        p_m_pHeadAlloc = &m_ptNodeAllocator->m_tAlloc.m_pHeadAlloc;
        while (1) {
            m_pMemBlocks = (unsigned __int64)m_pHeadAlloc->tAlloc.m_pMemBlocks;
            if (m_pMemBlocks <= (unsigned __int64)m_pHead && m_pMemBlocks + m_pHeadAlloc->tAlloc.m_nBlockSize * (__int64)m_pHeadAlloc->tAlloc.m_nBlockCount > (unsigned __int64)m_pHead) {
                break;
            }
            p_m_pHeadAlloc = &m_pHeadAlloc->pNext;
            m_pHeadAlloc   = m_pHeadAlloc->pNext;
            if (!m_pHeadAlloc) {
                if ((m_ptNodeAllocator->m_tAlloc.m_ucFlags & 4) != 0)
                    goto LABEL_15;
                goto LABEL_21;
            }
        }
        p_pNext          = m_pHeadAlloc->tAlloc.m_ptFreeList;
        m_pHead->m_tData = p_pNext ? __coerce<T1>(*p_pNext) : __coerce<T1>(p_pNext);

        m_pHeadAlloc->tAlloc.m_ptFreeList = (FixedBlockAllocator::XFreeNode *)m_pHead;
        ++m_pHeadAlloc->tAlloc.m_nFreeBlockCount;
        if ((m_ptNodeAllocator->m_tAlloc.m_ucFlags & 4) == 0)
            goto LABEL_21;
    LABEL_15:
        m_nBlockCount = m_pHeadAlloc->tAlloc.m_nBlockCount;
        if (m_nBlockCount == m_pHeadAlloc->tAlloc.m_nFreeBlockCount && m_pHeadAlloc != m_ptNodeAllocator->m_tAlloc.m_pHeadAlloc) {
            m_ptNodeAllocator->m_tAlloc.m_nBlockCount -= m_nBlockCount;
            *p_m_pHeadAlloc = m_pHeadAlloc->pNext;
            if ((m_ptNodeAllocator->m_tAlloc.m_ucFlags & 2) != 0)
                rawfree(m_pHeadAlloc);
            else
                SigmaMemoryFree(m_pHeadAlloc, m_ptNodeAllocator->m_tAlloc.m_ptMemoryInterface);
            --m_ptNodeAllocator->m_tAlloc.m_nNumAllocators;
        }
        goto LABEL_21;
    }
}
