#pragma once

#include <entt/entt.hpp>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>

struct SessionLog {
    // Maximum bytes buffered per frame before a flush. Pre-reserved at
    // construction so the hot write path never triggers heap reallocation.
    static constexpr std::size_t kMaxLogBufferBytes = 4096;

    FILE*       file  = nullptr;
    uint32_t    frame = 0;
    std::string buffer;        // buffered log lines, flushed once per frame
    int         last_logged_beat = -1;  // beat_log_system tracks last emitted beat

    SessionLog() { buffer.reserve(kMaxLogBufferBytes); }
};

// ── Free functions ───────────────────────────────────────────
// Open/close the session log file
void session_log_open(SessionLog& log, const char* path);
void session_log_close(SessionLog& log);

// Write a tagged, timestamped line.
// tag: "PLAYER" or "GAME  " (6 chars, caller pads).
void session_log_write(SessionLog& log, float song_time,
                       const char* tag, const char* fmt, ...);

// Flush buffered log lines to disk. Call once per frame, after all systems.
void session_log_flush(SessionLog& log);

// EnTT signal handlers — connect to registry during test_player setup.
void session_log_on_obstacle_spawn(entt::registry& reg, entt::entity entity);
void session_log_on_scored(entt::registry& reg, entt::entity entity);
