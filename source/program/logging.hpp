#pragma once

#include <cstdarg>
#include <cstdint>
#include <string_view>

namespace exl::log {
    enum class KeyEventLevel : uint8_t {
        Info,
        Warning,
        Error,
    };

    using KeyEventSink = void (*)(KeyEventLevel level, std::string_view message);

    void LogV(const char *fmt, std::va_list vl);
    void PrintV(const char *fmt, std::va_list vl);
    void LogFmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
    void PrintFmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
    void KeyEventFmt(KeyEventLevel level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    void SetKeyEventSink(KeyEventSink sink);
    void ConfigureGameFileLogging();
    void GameLogMaybeBuffered(std::string_view string);
}  // namespace exl::log
