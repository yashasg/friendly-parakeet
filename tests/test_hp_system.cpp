#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── hp_system ────────────────────────────────────────────────

TEST_CASE("hp: no action when not in Playing phase", "[hp]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto& hp = reg.ctx().get<HPState>();
    hp.current = 0;

    hp_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("hp: no action when HP is positive", "[hp]") {
    auto reg = make_rhythm_registry();
    auto& hp = reg.ctx().get<HPState>();
    hp.current = 3;

    hp_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
    CHECK(reg.ctx().get<SongState>().playing);
}

TEST_CASE("hp: triggers game over when HP reaches zero", "[hp]") {
    auto reg = make_rhythm_registry();
    auto& hp = reg.ctx().get<HPState>();
    hp.current = 0;

    hp_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::GameOver);
}

TEST_CASE("hp: stops song playback when HP depleted", "[hp]") {
    auto reg = make_rhythm_registry();
    auto& hp = reg.ctx().get<HPState>();
    hp.current = 0;

    hp_system(reg, 0.016f);

    auto& song = reg.ctx().get<SongState>();
    CHECK(song.finished);
    CHECK_FALSE(song.playing);
}

TEST_CASE("hp: negative HP also triggers game over", "[hp]") {
    auto reg = make_rhythm_registry();
    auto& hp = reg.ctx().get<HPState>();
    hp.current = -2;

    hp_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::GameOver);
}

TEST_CASE("hp: no action when SongState not present", "[hp]") {
    auto reg = make_registry();
    reg.ctx().emplace<HPState>(HPState{0, 5});

    hp_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("hp: no action when song not playing", "[hp]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<SongState>().playing = false;
    auto& hp = reg.ctx().get<HPState>();
    hp.current = 0;

    hp_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("hp: no action when HPState not present", "[hp]") {
    auto reg = make_registry();
    auto& song = reg.ctx().emplace<SongState>();
    song.playing = true;

    hp_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("hp: HP at exactly 1 does not trigger game over", "[hp]") {
    auto reg = make_rhythm_registry();
    auto& hp = reg.ctx().get<HPState>();
    hp.current = 1;

    hp_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
    CHECK(reg.ctx().get<SongState>().playing);
}
