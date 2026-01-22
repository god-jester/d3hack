#pragma once

#include "d3/types/d3_account.hpp"
#include "d3/types/protobuf.hpp"

namespace D3::CS {

    struct AccountData : google::protobuf::Message {
        google::protobuf::UnknownFieldSet                                 _unknown_fields_;
        google::protobuf::uint32                                          _has_bits_[1];
        int                                                               _cached_size_;
        D3::Account::Digest                                              *digest_;
        google::protobuf::RepeatedPtrField<D3::Account::AccountPartition> partitions_;
        // D3::Items::CurrencySavedData                                     *account_wide_currency_data_;
    };

}  // namespace D3::CS
