#pragma once

#include "d3/types/hexrays.hpp"

#include "nn/fs/fs_types.hpp"
#include "nn/os/os_mutex_type.hpp"

namespace Blizzard {

    namespace Util {
        using BitField = _BYTE[0x20];
    }  // namespace Util

    namespace Time {
        using Timestamp   = s64;
        using Microsecond = u64;
        using Millisecond = u32;
        using Second      = u32;
    }  // namespace Time

    namespace Thread {
        struct nnTlsSlot {
            u32 _innerValue;
        };
        using TLSDestructor = void (*)(void *);

        struct TLSSlot {
            Blizzard::Thread::TLSDestructor destructor;
            bool                            initialized;
            nnTlsSlot                       tSlot;
        };
    }  // namespace Thread

    namespace Lock {
        using Mutex  = nn::os::MutexType;
        using RMutex = nn::os::MutexType;
    }  // namespace Lock

    namespace File {
        using INODE = s32;
    }  // namespace File

    namespace Storage {
        struct StorageUnit;
        struct StorageUnitHandle {
            Blizzard::File::INODE           mNode;
            Blizzard::Storage::StorageUnit *mUnit;
        };
    }  // namespace Storage

    namespace File {
        struct ALIGNED(4) FileInfo {
            char                     *path;
            s64                       size;
            s64                       sparseSize;
            s32                       attributes;
            Blizzard::Time::Timestamp createTime;
            Blizzard::Time::Timestamp modTime;
            Blizzard::Time::Timestamp accessTime;
            s32                       existence;
            bool                      regularFile;
        };

        struct StreamingInfo {
            Blizzard::Storage::StorageUnitHandle storagenode;
            Blizzard::File::INODE                inode;
            s32                                  streamRefCount;
            Blizzard::File::FileInfo             streaminginfo;
        };

        struct StreamRecord {
            nn::fs::FileHandle             fileHandle;
            s32                            mode;
            bool                           infoValid;
            Blizzard::File::FileInfo       info;
            Blizzard::File::StreamingInfo *streaming;
            char                          *pathname;
        };

        using Stream = StreamRecord *;

    }  // namespace File

}  // namespace Blizzard
