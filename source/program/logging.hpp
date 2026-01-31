#pragma once

#include <cstdarg>
#include <string_view>

namespace exl::log {
    void LogV(const char *fmt, std::va_list vl);
    void PrintV(const char *fmt, std::va_list vl);
    void LogFmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
    void PrintFmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
    void ConfigureGameFileLogging();
    void GameLogMaybeBuffered(std::string_view string);
}  // namespace exl::log
