#pragma once

#include "d3/types/enums.hpp"

#include <array>
#include <cstddef>

namespace bdTaskOld {
    enum bdStatus : int32 {
        BD_EMPTY      = 0x0,
        BD_PENDING    = 0x1,
        BD_DONE       = 0x2,
        BD_FAILED     = 0x3,
        BD_TIMED_OUT  = 0x4,
        BD_CANCELLED  = 0x5,
        BD_MAX_STATUS = 0x6,
    };
}  // namespace bdTaskOld

template<typename T>
struct bdReference {
    T *m_ptr;
};

struct ALIGNED(8) bdReferencable {
    int (**_vptr$bdReferencable)(void);
    int m_refCount;  // std::atomic<int>
};

struct ALIGNED(8) bdByteBuffer : bdReferencable {
    bdUInt    m_size;
    bdUByte8 *m_data;
    bdUByte8 *m_readPtr;
    bdUByte8 *m_writePtr;
    bdBool    m_typeChecked;
    bdBool    m_typeCheckedCopy;
    bdBool    m_allocatedData;
};

struct bdTaskResultProcessor {
    int (**_vptr$bdTaskResultProcessor)(void);
};

struct ALIGNED(8) bdTaskResult {
    int (**_vptr$bdTaskResult)(void);
    _BYTE gap8[16];
};

struct ALIGNED(8) bdFileData {
    __int8 baseclass_0[8];
    void  *m_fileData;
    bdUInt m_fileSize;
    _BYTE  gap14[20];
};

struct bdStorage {
    int (**_vptr$bdStorage)(void);
    uintptr_t *m_remoteTaskManager; /* bdRemoteTaskManager */
    bdNChar8   m_context[16];
};

struct bdStopwatch {
    bdUInt64 m_start;
};

struct bdTask : bdReferencable {
    enum bdStatus : int32 {
        BD_EMPTY      = 0x0,
        BD_PENDING    = 0x1,
        BD_DONE       = 0x2,
        BD_FAILED     = 0x3,
        BD_TIMED_OUT  = 0x4,
        BD_CANCELLED  = 0x5,
        BD_MAX_STATUS = 0x6,
    };
};

using bdByteBufferRef = bdReference<bdByteBuffer>;
struct bdRemoteTask : bdTask {
    using bdStatus = bdTask::bdStatus;
    bdStopwatch            m_timer;
    bdFloat32              m_timeout;
    bdRemoteTask::bdStatus m_status;
    bdByteBufferRef        m_byteResults;
    bdTaskResult          *m_taskResult;
    bdTaskResult         **m_taskResultList;
    bdUInt                 m_numResults;
    bdUInt                 m_maxNumResults;
    bdUInt                 m_totalNumResults;
    bdUInt64               m_transactionID;
    bdLobbyErrorCode       m_errorCode;
    bdTaskResultProcessor *m_taskResultProcessor;
};

using bdRemoteTaskRef = bdReference<bdRemoteTask>;
struct bdRemoteTaskRefA : std::array<_BYTE, 0x8> {};
struct bdRemoteTaskRefB : std::array<std::byte, 0x8> {};
