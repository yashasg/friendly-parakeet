#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "systems/session_logger_system.h"

// Helper: inject an open in-memory SessionLog backed by the platform null device.
// /dev/null on POSIX; NUL on Windows — no real I/O, buffer stays in-memory.
static SessionLog make_open_log() {
    SessionLog log;
#ifdef _WIN32
    log.file  = std::fopen("NUL", "w");
#else
    log.file  = std::fopen("/dev/null", "w");
#endif
    log.frame = 0;
    return log;
}

// ── SessionLog construction: buffer pre-reserved ─────────────

TEST_CASE("SessionLog: buffer pre-reserved at construction", "[beat_log][session_log]") {
    SessionLog log;
    // Constructor must pre-reserve kMaxLogBufferBytes so the hot write path
    // never triggers a heap reallocation from an empty string.
    CHECK(log.buffer.capacity() >= SessionLog::kMaxLogBufferBytes);
}

TEST_CASE("SessionLog: clear preserves reserved capacity", "[beat_log][session_log]") {
    SessionLog log;
    log.buffer.append("some log line\n");
    std::size_t cap_before = log.buffer.capacity();
    log.buffer.clear();
    // clear() must not shrink capacity — no realloc on next frame's writes.
    CHECK(log.buffer.capacity() == cap_before);
}

// ── beat_log_system: no-op when logging disabled ─────────────

TEST_CASE("beat_log: no-op when SessionLog absent", "[beat_log]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.6f;
    set_beat_cursor(reg, 1);

    // No SessionLog in context — must not crash
    beat_log_system(reg, 0.0f);
    SUCCEED("No crash without SessionLog");
}

TEST_CASE("beat_log: no-op when log file is null", "[beat_log]") {
    auto reg = make_rhythm_registry();
    set_beat_cursor(reg, 3);

    SessionLog log;  // file = nullptr
    reg.ctx().emplace<SessionLog>(std::move(log));

    beat_log_system(reg, 0.0f);
    // LastLoggedBeat must not have been emplaced when file is null
    CHECK(last_logged_beat_value(reg) == -1);
}

TEST_CASE("beat_log: no-op when not in Playing phase", "[beat_log]") {
    auto reg = make_rhythm_registry();
    set_test_phase<GamePhaseTitleTag>(reg);
    set_beat_cursor(reg, 2);

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));

    tick_playing_systems(reg, 0.0f);
    CHECK(last_logged_beat_value(reg) == -1);
}

TEST_CASE("beat_log: no-op when song not playing", "[beat_log]") {
    auto reg = make_rhythm_registry();
    reg.ctx().erase<SongPlayingTag>();
    set_beat_cursor(reg, 2);

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));

    beat_log_system(reg, 0.0f);
    CHECK(last_logged_beat_value(reg) == -1);
}

// ── beat_log_system: advances LastLoggedBeat ─────────────────

TEST_CASE("beat_log: logs new beat and advances LastLoggedBeat", "[beat_log]") {
    auto reg = make_rhythm_registry();
    set_beat_cursor(reg, 1);

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));

    beat_log_system(reg, 0.0f);
    CHECK(last_logged_beat_value(reg) == 1);
}

TEST_CASE("beat_log: catches up multiple beats in one call", "[beat_log]") {
    auto reg = make_rhythm_registry();
    set_beat_cursor(reg, 5);  // jumped ahead 6 beats from absent cursor

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));

    beat_log_system(reg, 0.0f);
    CHECK(last_logged_beat_value(reg) == 5);
}

TEST_CASE("beat_log: falls back when authored beat_times are shorter than current beat", "[beat_log]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.offset = 0.25f;
    song.beat_period = 0.5f;
    song.song_time = 1.75f;
    set_beat_cursor(reg, 3);

    auto& map = beat_map(reg);
    map.beat_times = {0.25f};

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));

    beat_log_system(reg, 0.0f);

    auto& stored = reg.ctx().get<SessionLog>();
    CHECK(last_logged_beat_value(reg) == 3);
    CHECK(stored.buffer.find("BEAT 0 expected=0.250") != std::string::npos);
    CHECK(stored.buffer.find("BEAT 3 expected=1.750") != std::string::npos);
}

TEST_CASE("beat_log: does not re-log already-logged beats", "[beat_log]") {
    auto reg = make_rhythm_registry();
    set_beat_cursor(reg, 3);

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));
    set_last_logged_beat(reg, 3);  // already logged

    beat_log_system(reg, 0.0f);
    // LastLoggedBeat must remain 3 (no spurious advance)
    CHECK(last_logged_beat_value(reg) == 3);
}

// ── song_playback_system: no logging dependency ───────────────

TEST_CASE("song_playback: does not write to SessionLog", "[song_playback][beat_log]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    set_beat_cursor(reg, -1);

    // Place a SessionLog with open file so any accidental write would be detectable
    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));
    set_last_logged_beat(reg, -1);

    // Advance past two beats
    song_playback_system(reg, 1.1f);

    // Playback advanced the beat cursor
    CHECK(beat_cursor_value(reg) >= 1);
    // But LastLoggedBeat must remain absent — playback didn't touch the log
    CHECK(last_logged_beat_value(reg) == -1);
}

TEST_CASE("beat_log: LastLoggedBeat advances after beat_log_system runs", "[beat_log]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    set_beat_cursor(reg, -1);

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));

    song_playback_system(reg, 1.1f);
    // Still not logged yet (beat_log hasn't run)
    CHECK(last_logged_beat_value(reg) == -1);

    beat_log_system(reg, 0.0f);
    // Now it matches
    CHECK(last_logged_beat_value(reg) == beat_cursor_value(reg));
}
