#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "systems/session_logger.h"
#include "components/session_log.h"

// Helper: inject an open in-memory SessionLog backed by a temp file path.
// We use /dev/null so no real I/O is needed; the buffer is in-memory.
static SessionLog make_open_log() {
    SessionLog log;
    log.file  = std::fopen("/dev/null", "w");
    log.frame = 0;
    log.last_logged_beat = -1;
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
    song.current_beat = 1;

    // No SessionLog in context — must not crash
    beat_log_system(reg, 0.0f);
    SUCCEED("No crash without SessionLog");
}

TEST_CASE("beat_log: no-op when log file is null", "[beat_log]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.current_beat = 3;

    SessionLog log;  // file = nullptr
    reg.ctx().emplace<SessionLog>(std::move(log));

    beat_log_system(reg, 0.0f);
    auto& stored = reg.ctx().get<SessionLog>();
    // last_logged_beat must not have advanced when file is null
    CHECK(stored.last_logged_beat == -1);
}

TEST_CASE("beat_log: no-op when not in Playing phase", "[beat_log]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto& song = reg.ctx().get<SongState>();
    song.current_beat = 2;

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));

    beat_log_system(reg, 0.0f);
    CHECK(reg.ctx().get<SessionLog>().last_logged_beat == -1);
}

TEST_CASE("beat_log: no-op when song not playing", "[beat_log]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.playing = false;
    song.current_beat = 2;

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));

    beat_log_system(reg, 0.0f);
    CHECK(reg.ctx().get<SessionLog>().last_logged_beat == -1);
}

// ── beat_log_system: advances last_logged_beat ───────────────

TEST_CASE("beat_log: logs new beat and advances last_logged_beat", "[beat_log]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.current_beat = 1;

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));

    beat_log_system(reg, 0.0f);
    CHECK(reg.ctx().get<SessionLog>().last_logged_beat == 1);
}

TEST_CASE("beat_log: catches up multiple beats in one call", "[beat_log]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.current_beat = 5;  // jumped ahead 6 beats from -1

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));

    beat_log_system(reg, 0.0f);
    CHECK(reg.ctx().get<SessionLog>().last_logged_beat == 5);
}

TEST_CASE("beat_log: does not re-log already-logged beats", "[beat_log]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.current_beat = 3;

    auto slog = make_open_log();
    slog.last_logged_beat = 3;  // already logged
    reg.ctx().emplace<SessionLog>(std::move(slog));

    beat_log_system(reg, 0.0f);
    // last_logged_beat must remain 3 (no spurious advance)
    CHECK(reg.ctx().get<SessionLog>().last_logged_beat == 3);
}

// ── song_playback_system: no logging dependency ───────────────

TEST_CASE("song_playback: does not write to SessionLog", "[song_playback][beat_log]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    song.current_beat = -1;

    // Place a SessionLog with open file so any accidental write would be detectable
    auto slog = make_open_log();
    slog.last_logged_beat = -1;
    reg.ctx().emplace<SessionLog>(std::move(slog));

    // Advance past two beats
    song_playback_system(reg, 1.1f);

    // Playback advanced current_beat
    CHECK(song.current_beat >= 1);
    // But last_logged_beat must remain -1 — playback didn't touch the log
    CHECK(reg.ctx().get<SessionLog>().last_logged_beat == -1);
}

TEST_CASE("beat_log: last_logged_beat advances after beat_log_system runs", "[beat_log]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    song.current_beat = -1;

    auto slog = make_open_log();
    reg.ctx().emplace<SessionLog>(std::move(slog));

    song_playback_system(reg, 1.1f);
    // Still not logged yet (beat_log hasn't run)
    CHECK(reg.ctx().get<SessionLog>().last_logged_beat == -1);

    beat_log_system(reg, 0.0f);
    // Now it matches
    CHECK(reg.ctx().get<SessionLog>().last_logged_beat == song.current_beat);
}
