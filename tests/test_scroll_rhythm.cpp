#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

#include <cmath>
#include <limits>

// ── scroll_system: rhythm-mode BeatInfo positioning ──────────

TEST_CASE("scroll: rhythm obstacles positioned from song_time and BeatInfo", "[scroll][rhythm]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 2.0f;

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, 0.0f}});
    reg.emplace<BeatInfo>(obs, 0, 4.0f, 0.0f);

    scroll_system(reg, 0.016f);

    float expected_y = constants::SPAWN_Y + (song.song_time - 0.0f) * song.scroll_speed;
    CHECK_THAT(reg.get<WorldTransform>(obs).position.y, Catch::Matchers::WithinAbs(expected_y, 0.1f));
}

TEST_CASE("scroll: rhythm obstacles ignore Vector2 component", "[scroll][rhythm]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 1.0f;

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{100.0f, 0.0f}});
    reg.emplace<Vector2>(obs, Vector2{999.0f, 999.0f});
    reg.emplace<BeatInfo>(obs, 0, 3.0f, 0.0f);

    scroll_system(reg, 1.0f);

    // X should not change (Vector2 is irrelevant for BeatInfo entities in rhythm mode)
    float expected_y = constants::SPAWN_Y + (1.0f - 0.0f) * song.scroll_speed;
    CHECK_THAT(reg.get<WorldTransform>(obs).position.x, Catch::Matchers::WithinAbs(100.0f, 0.01f));
    CHECK_THAT(reg.get<WorldTransform>(obs).position.y, Catch::Matchers::WithinAbs(expected_y, 0.1f));
}

TEST_CASE("motion: non-rhythm entities use dt-based movement", "[motion][rhythm]") {
    auto reg = make_rhythm_registry();

    auto particle = reg.create();
    reg.emplace<WorldTransform>(particle, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<Vector2>(particle, Vector2{10.0f, 20.0f});
    // No BeatInfo → dt-based

    motion_system(reg, 0.5f);

    CHECK_THAT(reg.get<WorldTransform>(particle).position.x, Catch::Matchers::WithinAbs(105.0f, 0.01f));
    CHECK_THAT(reg.get<WorldTransform>(particle).position.y, Catch::Matchers::WithinAbs(210.0f, 0.01f));
}

TEST_CASE("scroll: BeatInfo position tracks song_time progression", "[scroll][rhythm]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, 0.0f}});
    reg.emplace<BeatInfo>(obs, 0, 4.0f, 1.0f);

    // Time 1: song_time=1.0
    song.song_time = 1.0f;
    scroll_system(reg, 0.016f);
    float y1 = reg.get<WorldTransform>(obs).position.y;

    // Time 2: song_time=2.0
    song.song_time = 2.0f;
    scroll_system(reg, 0.016f);
    float y2 = reg.get<WorldTransform>(obs).position.y;

    CHECK(y2 > y1);
    float delta = y2 - y1;
    CHECK_THAT(delta, Catch::Matchers::WithinAbs(song.scroll_speed, 0.5f));
}

TEST_CASE("scroll: invalid scroll_speed preserves finite obstacle position", "[scroll][rhythm][issue929]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 2.0f;
    song.scroll_speed = std::numeric_limits<float>::quiet_NaN();

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, 123.0f}});
    reg.emplace<BeatInfo>(obs, 0, 4.0f, 0.0f);

    {
        // Production scroll_system intentionally TraceLog(LOG_WARNING) on invalid
        // scroll_speed; silence here so the test doesn't pollute stderr.
        ScopedTraceLogSilencer silence_warning;
        scroll_system(reg, 0.016f);
    }

    const auto& transform = reg.get<WorldTransform>(obs);
    CHECK(std::isfinite(transform.position.y));
    CHECK_THAT(transform.position.y, Catch::Matchers::WithinAbs(123.0f, 0.001f));
}

TEST_CASE("scroll: non-positive scroll_speed skips position updates", "[scroll][rhythm][issue929]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 2.0f;
    song.scroll_speed = 0.0f;

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, 234.0f}});
    reg.emplace<BeatInfo>(obs, 0, 4.0f, 0.0f);

    {
        // Production scroll_system intentionally TraceLog(LOG_WARNING) on invalid
        // scroll_speed; silence here so the test doesn't pollute stderr.
        ScopedTraceLogSilencer silence_warning;
        scroll_system(reg, 0.016f);
        CHECK_THAT(reg.get<WorldTransform>(obs).position.y, Catch::Matchers::WithinAbs(234.0f, 0.001f));

        song.scroll_speed = -100.0f;
        scroll_system(reg, 0.016f);
        CHECK_THAT(reg.get<WorldTransform>(obs).position.y, Catch::Matchers::WithinAbs(234.0f, 0.001f));
    }
}
