#pragma once

#include <nvn/nvn_Cpp.h>
#include "enums.hpp"
#include "maps.hpp"
#include "namespaces.hpp"

struct XRemoteHeap;
struct OcclusionQuery;
struct VBFormatInfo;

namespace nn::vi {
    using NativeWindowHandle = void *;
    struct Display;
    struct Layer;
}  // namespace nn::vi

namespace blz {
    struct mutex;

    template<typename T, bool>
    struct _atomic_base {
        T x;
    };

    template<typename T>
    struct _atomic_base<T, true> : blz::_atomic_base<T, false> {};

    template<typename T>
    struct atomic : blz::_atomic_base<T, true> {};
}  // namespace blz

using BOOL_0        = int32;
using TextureFormat = int32;

enum NVNTexType : int32 {
    NVNTEXTYPE_2D       = 0x0,
    NVNTEXTYPE_CUBEMAP  = 0x1,
    NVNTEXTYPE_3D       = 0x2,
    NVNTEXTYPE_2D_ALIAS = 0x3,
};

struct NVNGraphicsMemory {
    enum MemType : int32 {
        MEMTYPE_RENDER_TARGET    = 0x0,
        MEMTYPE_COMMAND_BUFFER   = 0x1,
        MEMTYPE_GPU_EVENTS       = 0x2,
        MEMTYPE_STATIC_BUFFER    = 0x3,
        MEMTYPE_DYNAMIC_BUFFER   = 0x4,
        MEMTYPE_DESCRIPTOR_POOL  = 0x5,
        MEMTYPE_TEXTURE_RESOURCE = 0x6,
    };

    void                      *ptMemory;
    size_t                     nAllocSize;
    ptrdiff_t                  nAllocOffset;
    nvn::MemoryPool           *ptMemoryPool;
    nvn::BufferAddress         tNVNBufferAddress;
    NVNGraphicsMemory::MemType eMemType;
    BOOL_0                     bOwnsMemory;
    BOOL_0                     bOwnsMemoryPool;
};

struct NVNTexInfo {
    TextureFormat     eTexFormat;
    uint32            dwMSAALevel;
    uint32            dwLastFrameUsed;
    NVNTexType        eType;
    uint32            dwPixelWidth;
    uint32            dwPixelHeight;
    uint32            dwPixelDepth;
    uint32            dwMipLevels;
    NVNGraphicsMemory tMemory;
    BOOL_0            bTargetUpdated[1];
    BOOL_0            bIsWindowTexture;
    int               nTextureID;
    int               nReducedTextureID;
    nvn::Texture     *ptTexture;
    nvn::Texture     *ptReducedTexture;
};

struct NVNSampler {
    nvn::Sampler tSampler;
    int32        nSamplerID;
};

struct NVNGraphicsLinearAllocator {
    NVNGraphicsMemory tMemContainer;
    ptrdiff_t         nCurOffset;
};

struct NVNShaderAllocator {
    nvn::MemoryPool tMemoryPool;
    XRemoteHeap    *ptHeap;
};

template<int Size, int Count>
struct XFixedAllocator {
    uint8 m_Data[Size * Count];
};

template<typename T1, typename T2, int Count>
struct ALIGNED(8) XArray_Sub : XBaseArray<T1, T2, XFixedAllocator<8, Count>> {};

template<typename T1, typename T2, int Count>
struct XArray {
    XArray_Sub<T1, T2, Count> m_array;
};

struct NVNVertexInputBindingSet;
template<typename T1, typename T2, typename T3, typename T4>
struct XGrowableMap;
using NVNVertexAttribMap = XGrowableMap<unsigned long, unsigned long, NVNVertexInputBindingSet *, NVNVertexInputBindingSet *>;

namespace GFXNX64NVN {
    struct GpuTimer {
        const char           *pName;
        uint32                dwDepth;
        nvn::CounterData     *ptStartCounter;
        nvn::CounterData     *ptEndCounter;
        GFXNX64NVN::GpuTimer *ptIdleChild;
    };

    struct FrameProfile {
        GFXNX64NVN::GpuTimer                                                         tTimers[512];
        uint32                                                                       dwNumTimersUsed;
        blz::vector<GFXNX64NVN::GpuTimer *, blz::allocator<GFXNX64NVN::GpuTimer *>> *tActiveTimerStack;
    };

    struct CommandBufferMemoryRing {
        NVNGraphicsMemory         tCommandMemory;
        void                     *pControlMemory;
        blz::atomic<unsigned int> dwCurCommandBlock;
        blz::atomic<unsigned int> dwCurControlBlock;
        uint32                    CommandBlockFrameFence[256];
        uint32                    ControlBlockFrameFence[256];
    };

    struct IDManager {
        blz::atomic<int>                       dwCurrentTextureID;
        blz::atomic<int>                       dwCurrentSamplerID;
        blz::mutex                            *tTextureIdFreeListLock;
        blz::vector<int, blz::allocator<int>> *tTextureIdFreeList;
        nvn::TexturePool                       tTexturePool;
        nvn::SamplerPool                       tSamplerPool;
        NVNGraphicsMemory                      tTextureDescriptorMemory;
        NVNGraphicsMemory                      tSamplerDescriptorMemory;
    };

    struct Globals {  // sizeof=0x15900
        uint32 dwDeviceWidth;
        uint32 dwDeviceHeight;
        uint32 dwReducedDeviceWidth;
        uint32 dwReducedDeviceHeight;
        float  flDeviceAspectRatio;
        int32  nPresentInterval;
        uint32 dwFrameCount;
        _BYTE  _pad001C[4];
        ALIGNED(8)
        nvn::Event                 tProcessedFrameCount;
        VBFormatInfo              *ptVBFormatInfo;
        NVNTexInfo                 tSwapChainBuffer[3];
        int32                      nSwapChainBackBufferIndex;
        _BYTE                      _pad01D4[4];
        NVNTexInfo                 tMissing2DTexture;
        NVNSampler                 tGenericSampler;
        nn::vi::NativeWindowHandle tViNativeWindow;
        nn::vi::Display           *ptViDisplay;
        nn::vi::Layer             *ptViLayer;
        nvn::Device                tDevice;
        nvn::Window                tWindow;
        nvn::Sync                  tBackBufferSync;
        BOOL_0                     bNeedToWaitForBackBufferSync;
        _BYTE                      _pad03494[4];
        ALIGNED(8)
        nvn::Queue                                      tQueue;
        void                                           *pQueueMemory;
        NVNGraphicsLinearAllocator                      tGPUEventsMemory;
        NVNShaderAllocator                              tShaderAllocator;
        GFXNX64NVN::CommandBufferMemoryRing             tCommandBufferMemory;
        nvn::VertexStreamState                          tStreamStates[12];
        NVNVertexAttribMap                             *ptVertexAttribMap;
        uint32                                          dwConstantBufferSize[3];
        _BYTE                                           _pad05EA4[4];
        NVNGraphicsMemory                               tConstantBufferMemory;
        nvn::BufferAddress                              tConstantBuffers[3];
        NVNGraphicsLinearAllocator                      tDynamicBuffers[3];
        NVNGraphicsLinearAllocator                     *ptDynamicBuffer;
        GFXNX64NVN::IDManager                           tIDManager;
        Blizzard::Lock::Mutex                           tWorkerLock;
        int32                                           nCurrentProfileFrameIndex;
        _BYTE                                           _pad060AC[4];
        GFXNX64NVN::FrameProfile                        tFrameProfiles[3];
        OcclusionQuery                                 *pOcclusionQueryAllocation;
        XArray<OcclusionQuery *, OcclusionQuery *, 256> tFreeOcclusionQueries;
        float                                           fGammaValue;
        BOOL_0                                          bIsDocked;
    };

    static_assert(sizeof(GFXNX64NVN::Globals) == 0x15900);
}  // namespace GFXNX64NVN
