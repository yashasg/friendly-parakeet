#pragma once

#include <entt/entt.hpp>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>

// Per-tag obstacle-kind label for log lines. Replaces the former
// `enum_name(ObstacleKind)` lookup with a tag-presence dispatch
// (issue #1202/#1204). Returns "???" when no kind tag is present.
std::string_view obstacle_kind_label(const entt::registry& reg, entt::entity entity);

struct SessionLog {
    // Maximum bytes buffered per frame before a flush. Pre-reserved at
    // construction so the hot write path never triggers heap reallocation.
    static constexpr std::size_t kMaxLogBufferBytes = 4096;

    FILE*       file  = nullptr;
    uint32_t    frame = 0;
    std::string buffer;        // buffered log lines, flushed once per frame
    // The former `int last_logged_beat = -1` cursor migrated to the
    // `LastLoggedBeat` ctx-singleton row table below (Fabian Principle 3 /
    // issue #1545): membership IS "at least one beat has been emitted",
    // and `beat` is always meaningful while the row exists. Reset path:
    // test_player_session erases any prior row when re-opening the log.

    SessionLog() { buffer.reserve(kMaxLogBufferBytes); }
    SessionLog(const SessionLog&) = delete;
    SessionLog& operator=(const SessionLog&) = delete;
    SessionLog(SessionLog&& other) noexcept;
    SessionLog& operator=(SessionLog&& other) noexcept;
    ~SessionLog();

    void release();
};

// ── Last Logged Beat (singleton row table, lives in registry context) ────────
// Presence in `reg.ctx()` IS "at least one beat has been emitted to the
// session log for the current session"; `beat` is the highest beat index
// emitted, always meaningful while the row exists. Per Fabian Principle 3
// (.squad/decisions.md § 9 / issue #1545): the former
// `SessionLog::last_logged_beat = -1` sentinel was a NULL column in
// disguise. Producer/consumer with `BeatCursor` (see song_state.h):
// beat_log_system advances `LastLoggedBeat::beat` toward
// `BeatCursor::last_crossed`, emitting a `BEAT` line per integer step.
struct LastLoggedBeat {
    int beat = 0;
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

// Advance the frame counter once from the central game-loop path.
void session_log_begin_frame(SessionLog& log);

// EnTT signal handlers — connect to registry during test_player setup.
void session_log_on_obstacle_spawn(entt::registry& reg, entt::entity entity);
void session_log_on_scored(entt::registry& reg, entt::entity entity);
