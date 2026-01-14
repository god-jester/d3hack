#include "program/logging.hpp"

#include <lib/log/logger_mgr.hpp>
#include <program/loggers.hpp>

namespace exl::log {
    void LogV(const char *fmt, std::va_list vl) {
        Logging.VLog(fmt, vl);
    }

    void PrintV(const char *fmt, std::va_list vl) {
        std::va_list vl_game;
        va_copy(vl_game, vl);
        Logging.VLog(fmt, vl);
        GameLogging.VLog(fmt, vl_game);
        va_end(vl_game);
    }

    void LogFmt(const char *fmt, ...) {
        std::va_list vl;
        va_start(vl, fmt);
        LogV(fmt, vl);
        va_end(vl);
    }

    void PrintFmt(const char *fmt, ...) {
        std::va_list vl;
        va_start(vl, fmt);
        PrintV(fmt, vl);
        va_end(vl);
    }
}  // namespace exl::log
