#pragma once

#include <lib/log/svc_logger.hpp>
#include <string_view>
#include "program/logging.hpp"
#include "d3/types/enums.hpp"

using TraceInternal_Log_t = void (*)(const SeverityLevel eLevel, const uint32 dwFlags, const OutputStreamType eStream, const char *szFormat, ...);
using ErrorManagerEnableLocalLogging_t = void (*)(int32 bEnabled);
using ErrorManagerSetLogStreamDirectory_t = void (*)(const char *pszDirectory);
using ErrorManagerSetLogStreamFilename_t = void (*)(const OutputStreamType eStream, const char *pszFileName);
using ErrorManagerLogToStream_t = void (*)(const SeverityLevel eLevel, const OutputStreamType eStream, const char *szMessage, int32 useRingBuffer);

extern TraceInternal_Log_t TraceInternal_Log;
extern ErrorManagerEnableLocalLogging_t ErrorManagerEnableLocalLogging;
extern ErrorManagerSetLogStreamDirectory_t ErrorManagerSetLogStreamDirectory;
extern ErrorManagerSetLogStreamFilename_t ErrorManagerSetLogStreamFilename;
extern ErrorManagerLogToStream_t ErrorManagerLogToStream;

/* Specify logger implementations here. */
namespace exl::log {
    struct GameLogger : public ILogger {
        void LogRaw(std::string_view string) final {
            GameLogMaybeBuffered(string);
        }
    };
}  // namespace exl::log

inline exl::log::LoggerMgr<exl::log::SvcLogger> Logging;

inline exl::log::LoggerMgr<exl::log::GameLogger> GameLogging;
