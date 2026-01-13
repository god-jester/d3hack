#pragma once

#include "fs_types.hpp"

namespace nn::fs {

    /*
        Mount SD card. Must have explicit permission.
        mount: drive to mount to.
    */
    Result MountSdCardForDebug(char const* mount);

    /*
        Query required working/cache size for MountRom().
        outSize: required size in bytes.
    */
    Result QueryMountRomCacheSize(u64* outSize);

    /*
        Mount the current process's Application RomFS as a filesystem device.
        name: mount name (e.g. "romfs", accessed as "romfs:/...").
        buffer: working/cache memory (must remain valid for the lifetime of the mount).
        bufferSize: size of buffer in bytes.
    */
    Result MountRom(char const* name, void* buffer, ulong bufferSize);
};
