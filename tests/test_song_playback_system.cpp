#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── song_playback_system: time advancement ───────────────────

TEST_CASE("song_playback: song_time advances by dt", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 1.0f;

    song_playback_system(reg, 0.5f);

    CHECK_THAT(song.song_time, Catch::Matchers::WithinAbs(1.5f, 0.001f));
}

TEST_CASE("song_playback: no advancement when not Playing", "[song_playback]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 1.0f;

    song_playback_system(reg, 0.5f);

    CHECK(song.song_time == 1.0f);
}

TEST_CASE("song_playback: no advancement when song not playing", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.playing = false;
    song.song_time = 1.0f;

    song_playback_system(reg, 0.5f);

    CHECK(song.song_time == 1.0f);
}

TEST_CASE("song_playback: no advancement when song already finished", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.finished = true;
    song.song_time = 50.0f;

    song_playback_system(reg, 0.5f);

    CHECK(song.song_time == 50.0f);
}

TEST_CASE("song_playback: no advancement when SongState absent", "[song_playback]") {
    auto reg = make_registry();
    // No SongState in context
    song_playback_system(reg, 0.5f);
    SUCCEED("No crash without SongState");
}

// ── song_playback_system: beat tracking ──────────────────────

TEST_CASE("song_playback: current_beat updates correctly", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    song.offset = 0.0f;
    // beat_period = 0.5 at 120 BPM

    // Advance to 0.6s — should be beat 1 (0.6 / 0.5 = 1.2 → beat 1)
    song_playback_system(reg, 0.6f);

    CHECK(song.current_beat == 1);
}

TEST_CASE("song_playback: current_beat starts at -1 before offset", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = -1.0f;
    song.offset = 0.5f;

    song_playback_system(reg, 0.1f);  // song_time = -0.9, still before offset

    CHECK(song.current_beat == -1);
}

TEST_CASE("song_playback: current_beat advances with offset", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    song.offset = 1.0f;
    // beat = (song_time - offset) / beat_period
    // After advancing 1.6s: beat = (1.6 - 1.0) / 0.5 = 1.2 → beat 1

    song_playback_system(reg, 1.6f);

    CHECK(song.current_beat == 1);
}

// ── song_playback_system: song end ───────────────────────────

TEST_CASE("song_playback: song finishes at duration", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 59.5f;
    song.duration_sec = 60.0f;

    song_playback_system(reg, 1.0f);  // song_time = 60.5 > duration

    CHECK(song.finished);
    CHECK_FALSE(song.playing);
}

TEST_CASE("song_playback: song does not finish before duration", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 58.0f;
    song.duration_sec = 60.0f;

    song_playback_system(reg, 1.0f);  // song_time = 59.0 < duration

    CHECK_FALSE(song.finished);
    CHECK(song.playing);
}

TEST_CASE("song_playback: song finishes exactly at duration", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.duration_sec = 60.0f;
    song.song_time = 59.0f;

    song_playback_system(reg, 1.0f);  // song_time = 60.0 == duration

    CHECK(song.finished);
    CHECK_FALSE(song.playing);
}

// ── song_playback_system: beat tracking edge cases ───────────

TEST_CASE("song_playback: beat does not go backwards", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 2.0f;
    song.current_beat = 3;

    // Small advancement that doesn't cross a new beat
    song_playback_system(reg, 0.01f);

    CHECK(song.current_beat >= 3);
}

TEST_CASE("song_playback: multiple beats can be crossed in one frame", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    song.current_beat = -1;

    // Advance 3 seconds at 120 BPM (beat_period=0.5) = 6 beats
    song_playback_system(reg, 3.0f);

    CHECK(song.current_beat == 6);
}

TEST_CASE("song_playback: zero beat_period handled safely", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.beat_period = 0.0f;
    song.song_time = 1.0f;

    // Should not crash or update beat
    song_playback_system(reg, 0.1f);

    // Just verify no crash
    CHECK(song.song_time > 1.0f);
}
