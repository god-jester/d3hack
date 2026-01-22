#pragma once

#include "d3/types/protobuf.hpp"

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

}  // namespace D3::Achievements
