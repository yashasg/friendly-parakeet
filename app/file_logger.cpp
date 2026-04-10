#include "file_logger.h"
#include <raylib.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>

static FILE* s_log_file = nullptr;

// ── Portable wrappers for deprecated-on-Windows CRT functions ────────
// On Windows the CRT marks localtime() and fopen() as deprecated because
// they are not thread-safe or lack buffer-size checks.  The _s variants
// exist only on Windows, while POSIX provides localtime_r.  These thin
// wrappers keep the rest of the file free of #ifdef clutter.

static std::tm safe_localtime(const std::time_t* t) {
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, t);
#else
    localtime_r(t, &result);
#endif
    return result;
}

static FILE* safe_fopen(const char* path, const char* mode) {
#ifdef _WIN32
    FILE* fp = nullptr;
    fopen_s(&fp, path, mode);
    return fp;
#else
    return std::fopen(path, mode);
#endif
}

static const char* log_level_str(int level) {
    switch (level) {
        case LOG_TRACE:   return "TRACE";
        case LOG_DEBUG:   return "DEBUG";
        case LOG_INFO:    return "INFO";
        case LOG_WARNING: return "WARN";
        case LOG_ERROR:   return "ERROR";
        case LOG_FATAL:   return "FATAL";
        default:          return "???";
    }
}

static void file_log_callback(int level, const char* text, va_list args) {
    if (level == LOG_NONE) return;

    // Timestamp
    std::time_t now = std::time(nullptr);
    std::tm tm = safe_localtime(&now);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    const char* tag = log_level_str(level);

    // Write to stdout
    std::printf("[%s] [%-5s] ", ts, tag);
    va_list args_copy;
    va_copy(args_copy, args);
    std::vprintf(text, args_copy);
    va_end(args_copy);
    std::printf("\n");

    // Write to log file
    if (s_log_file) {
        std::fprintf(s_log_file, "[%s] [%-5s] ", ts, tag);
        std::vfprintf(s_log_file, text, args);
        std::fprintf(s_log_file, "\n");
        std::fflush(s_log_file);
    }
}

void file_logger_init(const char* log_path) {
    s_log_file = safe_fopen(log_path, "a");
    if (s_log_file) {
        // Session separator
        std::time_t now = std::time(nullptr);
        std::tm tm = safe_localtime(&now);
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
        std::fprintf(s_log_file, "\n══════ Session started %s ══════\n", ts);
        std::fflush(s_log_file);
    }
    SetTraceLogCallback(file_log_callback);
}

void file_logger_shutdown() {
    if (s_log_file) {
        std::fflush(s_log_file);
        std::fclose(s_log_file);
        s_log_file = nullptr;
    }
}
