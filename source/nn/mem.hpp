/**
 * @file mem.h
 * @brief Memory allocation functions.
 */

#pragma once

#include "types.h"

namespace nn {
namespace mem {
class StandardAllocator {
 public:
  StandardAllocator();

  void Initialize(void* address, u64 size);

  void Finalize();

  void* Allocate(u64 size);

  void Free(void* address);

  void* Reallocate(void* address, u64 newSize);

  u64 GetSizeOf(const void*) const;

  u64 GetTotalFreeSize(void) const;

  u64 GetAllocatableSize(void) const;

  void Dump();

  bool m_Initialized;
  bool m_EnableThreadCache;
  uintptr_t m_CentralAllocatorAddr;
  int m_TlsSlot;
  u64* m_CentralHeap;
};
}  // namespace mem
}  // namespace nn
