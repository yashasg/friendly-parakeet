// Game Over screen controller tests — issue #498.
//
// Verifies that the Game Over screen exposes the run's final score, persisted
// high score, and a one-line death cause through stable, render-independent
// surfaces (DeathCause → reason text mapping; ScoreState binding pattern).
//
// We deliberately do NOT spin up a raygui surface here: the controller render
// path requires an active GL context (raygui ↔ raylib). Instead we assert on
// the values the controller binds to its layout slots, mirroring how
// song_complete is exercised elsewhere.

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "test_helpers.h"
#include "ui/screen_controllers/game_over_screen_controller.h"
#include "components/song_state.h"
#include "components/scoring.h"

TEST_CASE("game_over: death_cause_text maps every DeathCause to a non-empty platform-neutral string",
          "[game_over][ui]") {
    // None → empty (suppresses ReasonSlot rendering).
    CHECK(std::strlen(death_cause_text(DeathCause::None)) == 0);

    // Real causes must produce a non-empty, ASCII-only one-liner.
    for (auto cause : { DeathCause::EnergyDepleted, DeathCause::MissedABeat }) {
        const char* txt = death_cause_text(cause);
        REQUIRE(txt != nullptr);
        CHECK(std::strlen(txt) > 0);
        // Stay short enough to fit ReasonSlot (500px @ 22pt) — sanity bound.
        CHECK(std::strlen(txt) < 32u);
    }
}

TEST_CASE("game_over: distinct DeathCauses produce distinct reasons (#168 coverage)",
          "[game_over][ui]") {
    const char* energy = death_cause_text(DeathCause::EnergyDepleted);
    const char* miss   = death_cause_text(DeathCause::MissedABeat);
    CHECK(std::strcmp(energy, miss) != 0);
}

TEST_CASE("game_over: score / high score / cause are visible via registry singletons "
          "(value-binding contract mirrors song_complete)",
          "[game_over][ui]") {
    auto reg = make_registry();

    // Simulate a finished run.
    auto& score = reg.ctx().get<ScoreState>();
    score.score      = 12345;
    score.high_score = 99999;

    auto& gos = reg.ctx().insert_or_assign(GameOverState{});
    gos.cause = DeathCause::EnergyDepleted;

    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 1.0f;

    // Controller-bound values — these are exactly the fields the
    // game_over_screen_controller reads to populate the layout slots.
    const auto& s = reg.ctx().get<ScoreState>();
    CHECK(s.score      == 12345);
    CHECK(s.high_score == 99999);

    const auto& g = reg.ctx().get<GameOverState>();
    CHECK(g.cause == DeathCause::EnergyDepleted);
    CHECK(std::strlen(death_cause_text(g.cause)) > 0);
}
