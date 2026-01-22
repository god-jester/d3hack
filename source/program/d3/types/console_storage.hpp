#pragma once

#include "d3/types/hexrays.hpp"

namespace Console::Online {

    enum StorageResult : s32 {
        STORAGE_SUCCESS               = 0x0,
        STORAGE_FAILED                = 0x1,
        STORAGE_FAILED_NOT_CONNECTED  = 0x2,
        STORAGE_FAILED_FILE_NOT_FOUND = 0x3,
        STORAGE_FAILED_DATA_INVALID   = 0x4,
    };

    struct ALIGNED(8) StorageGetFileCallback {
        uintptr_t *m_impl;
        // OnlineService::Callback<void (Console::Online::StorageResult,blz::shared_ptr<blz::basic_string<char,blz::char_traits<char>,blz::allocator<char> > >)>::node_t *m_impl;
    };

}  // namespace Console::Online
