#include "program/logging.hpp"

#include <lib/log/logger_mgr.hpp>
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
            { OUTPUTSTREAM_BOOT, "Boot.txt" },
            { OUTPUTSTREAM_SYNC, "Sync.txt" },
            { OUTPUTSTREAM_STREAMING, "Streaming.log" },
            { OUTPUTSTREAM_BATTLENET, "Battlenet.log" },
            { OUTPUTSTREAM_LOCKS, "locks.log" },
            { OUTPUTSTREAM_LEADERBOARD, "locks.log" },
            { OUTPUTSTREAM_NOTIFICATIONS, "Notifications.log" },
            { OUTPUTSTREAM_TRADE, "Trade.log" },
            { OUTPUTSTREAM_RIFTSTATS, "RiftStats.log" },
        };

        constexpr size_t kGameLogBufferMaxBytes = 64 * 1024;
        constexpr const char kGameLogPrefix[]   = ": ";

        alignas(d3::system_allocator::Buffer) std::byte s_game_log_storage[sizeof(d3::system_allocator::Buffer)];
        d3::system_allocator::Buffer *s_game_log_buffer = nullptr;

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

            auto view          = s_game_log_buffer->view();
            const char *cursor = view.data();
            const char *end    = cursor + view.size();

            while (cursor < end) {
                const void *newline = std::memchr(cursor, '\n', static_cast<size_t>(end - cursor));
                if (newline == nullptr) {
                    EmitGameLogLine(std::string_view(cursor, static_cast<size_t>(end - cursor)));
                    break;
                }
                const char *line_end = static_cast<const char *>(newline);
                const size_t len     = static_cast<size_t>(line_end - cursor + 1);
                EmitGameLogLine(std::string_view(cursor, len));
                cursor = line_end + 1;
            }

            ReleaseGameLogBuffer();
        }
    }  // namespace

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
