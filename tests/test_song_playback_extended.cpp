#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── song_playback_system: beat advancement ───────────────────

TEST_CASE("song_playback: beat advances when song_time crosses beat boundary", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    song.current_beat = -1;
    song.offset = 0.0f;

    // Advance past first beat (beat_period = 0.5 at 120 BPM)
    song_playback_system(reg, 0.6f);

    CHECK(song.current_beat >= 0);
    CHECK_THAT(song.song_time, Catch::Matchers::WithinAbs(0.6f, 0.01f));
}

TEST_CASE("song_playback: multiple beats advance correctly", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    song.current_beat = -1;
    song.offset = 0.0f;

    // Advance by 2.5 seconds at 120 BPM (0.5s per beat) → 5 beats
    song_playback_system(reg, 2.5f);

    CHECK(song.current_beat >= 4);
}

TEST_CASE("song_playback: song ends when duration exceeded", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.duration_sec = 5.0f;
    song.song_time = 4.9f;

    song_playback_system(reg, 0.2f);

    CHECK(song.finished);
    CHECK_FALSE(song.playing);
}

TEST_CASE("song_playback: does nothing when not in Playing phase", "[song_playback]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto& song = reg.ctx().get<SongState>();
    float original_time = song.song_time;

    song_playback_system(reg, 1.0f);

    CHECK(song.song_time == original_time);
}

TEST_CASE("song_playback: does nothing when song not playing", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.playing = false;
    float original_time = song.song_time;

    song_playback_system(reg, 1.0f);

    CHECK(song.song_time == original_time);
}

TEST_CASE("song_playback: does nothing when song finished", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.finished = true;
    float original_time = song.song_time;

    song_playback_system(reg, 1.0f);

    CHECK(song.song_time == original_time);
}

TEST_CASE("song_playback: offset delays beat counting", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    song.current_beat = -1;
    song.offset = 1.0f;

    // song_time = 0.5, before offset → no beat yet
    song_playback_system(reg, 0.5f);
    CHECK(song.current_beat == -1);

    // song_time = 1.5, past offset → beat counting starts
    song_playback_system(reg, 1.0f);
    CHECK(song.current_beat >= 0);
}

TEST_CASE("song_playback: current_beat is non-decreasing", "[song_playback]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 0.0f;
    song.current_beat = -1;

    int prev_beat = -1;
    for (int i = 0; i < 20; ++i) {
        song_playback_system(reg, 0.1f);
        CHECK(song.current_beat >= prev_beat);
        prev_beat = song.current_beat;
    }
}

