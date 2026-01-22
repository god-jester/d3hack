#pragma once

#include "d3/types/hexrays.hpp"

#include <functional>
#include <memory>
#include <string>

namespace blz {

    template<typename T>
    using char_traits = std::char_traits<T>;

    template<typename T>
    using stlallocator = std::allocator<T>;

    template<typename... Ts>
    struct allocator {
        s8 gap0[1];
    };

    template<typename T>
    using hash = std::hash<T>;

    template<typename T>
    using stlequal_to = std::equal_to<T>;

    template<typename... Ts>
    struct equal_to {
        s8 gap0[1];
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
        u64       m_capacity : 63;
        u64       m_capacity_is_embedded : 1;
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
        u64       m_capacity : 63;
        u64       m_capacity_is_embedded : 1;
    };

    struct _shared_ptr_manager {
        int (**_vptr$_shared_ptr_manager)(void) = (*&_vptr$_shared_ptr_manager);
        s32 m_use_count                         = 0;
        s32 m_weak_count                        = 0;
    };

    template<typename T1, bool T2>
    struct _shared_ptr_base {
        T1 *m_pointer;
    };

    template<typename T>
    struct shared_ptr : blz::_shared_ptr_base<T, false> {
        blz::_shared_ptr_manager *m_manager = new _shared_ptr_manager;
    };

    using string = basic_string<char, blz::char_traits<char>, blz::allocator<char>>;

    template<typename T>
    struct chained_hash_node {
        chained_hash_node<T> *next;
        T                     val;
    };

    template<typename T>
    struct ALIGNED(8) chained_hash_table {
        size_t         m_bucket_count;
        T::node_type **m_buckets;
        size_t         m_entry_count;
        float          m_max_load_factor;
    };

}  // namespace blz
