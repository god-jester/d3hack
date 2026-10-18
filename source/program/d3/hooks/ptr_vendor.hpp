#pragma once

#include "lib/hook/trampoline.hpp"
#include "program/config.hpp"
#include "d3/types/common.hpp"
#include "symbols/common.hpp"

namespace d3 {
    namespace {
        // Djank Mi'em the PTR Vendor, as identified by his actor record: the 32-bit key at record+0x18
        // (the same field the game hands to the vendor-type attribute lookup).
        constexpr u32 kPtrVendorActorKey = 0x705ED;
        constexpr u32 kActorRecordKeyOff = 0x18;

        // The vendor-open message picks the UI from this test: the vendor-type attribute (0x41003) of the
        // vendor's actor record == 1 opens the Blood Shard Vendor window, anything else the ordinary shop.
        // Djank's stock is the PTR pouches (miscellaneous items), which only the ordinary shop can list and
        // sell, so report him as a normal merchant. Every other vendor goes through the original test.
        HOOK_DEFINE_TRAMPOLINE(VendorIsBloodShardHook) {
            static int32 Callback(void *actor_record) {
                if (actor_record != nullptr) {
                    const u32 key = *reinterpret_cast<const u32 *>(reinterpret_cast<const u8 *>(actor_record) + kActorRecordKeyOff);
                    if (key == kPtrVendorActorKey)
                        return 0;
                }
                return Orig(actor_record);
            }
        };
    }  // namespace

    void SetupPtrVendorHooks() {
        VendorIsBloodShardHook::
            InstallAtFuncPtr(VendorIsBloodShard);
    }
}  // namespace d3
