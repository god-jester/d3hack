#pragma once

#include "d3/types/blz_types.hpp"

namespace google::protobuf {

    using int32  = s32;
    using uint32 = u32;
    using int64  = s64;
    using uint64 = u64;

    struct MessageLite {
        /* TODO: */
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
    }  // namespace internal

    template<typename Element>
    class RepeatedField final : private internal::RepeatedField_Base {};

    namespace internal {
        struct ALIGNED(8) RepeatedPtrFieldBase {
            void **elements_;
            int    current_size_;
            int    allocated_size_;
            int    total_size_;
        };
    }  // namespace internal

    template<typename Element>
    class RepeatedPtrField final : private internal::RepeatedPtrFieldBase {};

}  // namespace google::protobuf
