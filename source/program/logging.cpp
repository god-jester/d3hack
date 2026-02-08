#include "program/logging.hpp"

#include "lib/log/logger_mgr.hpp"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "d3/_util.hpp"
#include "program/loggers.hpp"
#include "program/system_allocator.hpp"

namespace exl::log {
    namespace {
        struct StreamConfig {
            OutputStreamType type;
            const char      *name;
        };

        constexpr StreamConfig kLogStreams[] = {
            // Match the game's default output stream filenames where possible.
            // Note: In the base game, OUTPUTSTREAM_LEADERBOARD shares "locks.log" with OUTPUTSTREAM_LOCKS.
            // We keep LEADERBOARD separate for clarity.
            {OUTPUTSTREAM_DEFAULT, "Debug.txt"},
            {OUTPUTSTREAM_BOOT, "Boot.txt"},
            {OUTPUTSTREAM_SYNC, "Sync.txt"},
            {OUTPUTSTREAM_STREAMING, "Streaming.log"},
            {OUTPUTSTREAM_BATTLENET, "Battlenet.log"},
            {OUTPUTSTREAM_LOCKS, "locks.log"},
            {OUTPUTSTREAM_NOTIFICATIONS, "Notifications.log"},
            {OUTPUTSTREAM_TRADE, "Trade.log"},
            {OUTPUTSTREAM_LEADERBOARD, "Leaderboard.log"},
            {OUTPUTSTREAM_RIFTSTATS, "RiftStats.log"},
        };

        constexpr size_t     kGameLogBufferMaxBytes = 64 * 1024;
        constexpr const char kGameLogPrefix[]       = ": ";

        alignas(d3::system_allocator::Buffer) std::byte s_game_log_storage[sizeof(d3::system_allocator::Buffer)];
        d3::system_allocator::Buffer *s_game_log_buffer = nullptr;
        std::atomic<KeyEventSink>     g_key_event_sink {nullptr};

        auto IsGameLogReady() -> bool {
            return TraceInternal_Log != nullptr && d3::g_tSigmaGlobals.ptEMGlobals != nullptr;
        }

        auto GetGameLogBuffer() -> d3::system_allocator::Buffer * {
            if (s_game_log_buffer != nullptr) {
                return s_game_log_buffer;
            }
            auto *allocator = d3::system_allocator::GetSystemAllocator();
            if (allocator == nullptr) {
                return nullptr;
            }
            s_game_log_buffer = new (s_game_log_storage) d3::system_allocator::Buffer(allocator);
            return s_game_log_buffer;
        }

        auto ReleaseGameLogBuffer() -> void {
            if (s_game_log_buffer == nullptr) {
                return;
            }
            s_game_log_buffer->~Buffer();
            s_game_log_buffer = nullptr;
        }

        auto EmitGameLogLine(std::string_view line) -> void {
            if (TraceInternal_Log == nullptr) {
                return;
            }
            TraceInternal_Log(
                SLVL_INFO,
                2u,
                OUTPUTSTREAM_DEFAULT,
                "%.*s",
                static_cast<int>(line.size()),
                line.data()
            );
        }

        auto FlushGameLogBufferIfReady() -> void {
            if (!IsGameLogReady() || s_game_log_buffer == nullptr || s_game_log_buffer->size() == 0) {
                return;
            }

            auto        view   = s_game_log_buffer->view();
            const char *cursor = view.data();
            const char *end    = cursor + view.size();

            while (cursor < end) {
                const void *newline = std::memchr(cursor, '\n', static_cast<size_t>(end - cursor));
                if (newline == nullptr) {
                    EmitGameLogLine(std::string_view(cursor, static_cast<size_t>(end - cursor)));
                    break;
                }
                const char  *line_end = static_cast<const char *>(newline);
                const size_t len      = static_cast<size_t>(line_end - cursor + 1);
                EmitGameLogLine(std::string_view(cursor, len));
                cursor = line_end + 1;
            }

            ReleaseGameLogBuffer();
        }

        auto EmitKeyEvent(KeyEventLevel level, std::string_view message) -> void {
            auto sink = g_key_event_sink.load(std::memory_order_acquire);
            if (sink != nullptr) {
                sink(level, message);
            }
        }
    }  // namespace

    void LogV(const char *fmt, std::va_list vl) {
        Logging.VLog(fmt, vl);
    }

    void PrintV(const char *fmt, std::va_list vl) {
        // NOTE: Printing to the game's log streams can recurse into ErrorManager.
        // When we're already handling an internal error or exception, that can
        // cascade into further faults. Keep OutputDebugString alive, but avoid
        // re-entering game logging from inside error handling.
        bool skip_game_logging = false;
        if (d3::g_tSigmaGlobals.ptEMGlobals != nullptr) {
            const auto &em    = *d3::g_tSigmaGlobals.ptEMGlobals;
            skip_game_logging = (em.fSigmaCurrentlyHandlingError != 0) || (em.fSigmaCurrentlyInExceptionHandler != 0);
        }

        Logging.VLog(fmt, vl);
        if (!skip_game_logging) {
            std::va_list vl_game;
            va_copy(vl_game, vl);
            GameLogging.VLog(fmt, vl_game);
            va_end(vl_game);
        }
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

    void KeyEventFmt(KeyEventLevel level, const char *fmt, ...) {
        std::array<char, 512> buf {};
        va_list               vl;
        va_start(vl, fmt);
        vsnprintf(buf.data(), buf.size(), fmt, vl);
        va_end(vl);
        buf.back() = '\0';

        PrintFmt("%s", buf.data());
        EmitKeyEvent(level, std::string_view(buf.data()));
    }

    void SetKeyEventSink(KeyEventSink sink) {
        g_key_event_sink.store(sink, std::memory_order_release);
    }

    void ConfigureGameFileLogging() {
        if (ErrorManagerEnableLocalLogging == nullptr || ErrorManagerSetLogStreamDirectory == nullptr ||
            ErrorManagerSetLogStreamFilename == nullptr) {
            return;
        }

        ErrorManagerEnableLocalLogging(true);
        ErrorManagerSetLogStreamDirectory(d3::g_szBaseDir);

        for (const auto &stream : kLogStreams) {
            ErrorManagerSetLogStreamFilename(stream.type, stream.name);
        }

        FlushGameLogBufferIfReady();
    }

    void GameLogMaybeBuffered(std::string_view string) {
        if (string.empty()) {
            return;
        }
        if (IsGameLogReady()) {
            TraceInternal_Log(
                SLVL_INFO,
                2u,
                OUTPUTSTREAM_DEFAULT,
                ": %.*s\n",
                static_cast<int>(string.size()),
                string.data()
            );
            return;
        }

        auto *buffer = GetGameLogBuffer();
        if (buffer == nullptr || !buffer->ok()) {
            return;
        }

        const size_t total_needed = (sizeof(kGameLogPrefix) - 1) + string.size() + 1;
        if (buffer->size() + total_needed > kGameLogBufferMaxBytes) {
            return;
        }

        buffer->Append(kGameLogPrefix, sizeof(kGameLogPrefix) - 1);
        buffer->Append(string.data(), string.size());
        buffer->Append("\n", 1);
    }
}  // namespace exl::log
