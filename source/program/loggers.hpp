#pragma once

#include <lib/log/svc_logger.hpp>
#include "d3/types/enums.hpp"

using TraceInternal_Log_t = void (*)(const SeverityLevel eLevel, const uint32 dwFlags, const OutputStreamType eStream, const char *szFormat, ...);
extern TraceInternal_Log_t TraceInternal_Log;

/* Specify logger implementations here. */
namespace exl::log {
    struct GameLogger : public ILogger {
        void LogRaw(std::string_view string) final {
            if (TraceInternal_Log == nullptr)
                return;
            TraceInternal_Log(SLVL_INFO, 3u, OUTPUTSTREAM_DEFAULT, ": %.*s\n", static_cast<int>(string.size()), string.data());
        }
    };
}  // namespace exl::log

inline exl::log::LoggerMgr<exl::log::SvcLogger> Logging;

inline exl::log::LoggerMgr<exl::log::GameLogger> GameLogging;
