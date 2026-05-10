#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <string>
#include <vector>

#include "test_helpers.h"
#include "ui/screen_controllers/game_over_screen_controller.h"
#include "components/song_state.h"
#include "components/scoring.h"

namespace {

struct CapturedValue {
    float x = 0.0f;
    float y = 0.0f;
    std::string text;
};

std::vector<CapturedValue> g_captured_values;

void no_op_layout_render(GameOverLayoutState* state) {
    (void)state;
}

void capture_scoreboard_value(Vector2 anchor, float x, float y, float, float,
                              const char* text, int) {
    g_captured_values.push_back(CapturedValue{anchor.x + x, anchor.y + y, text ? text : ""});
}

bool has_bound_text(const char* expected) {
    for (const auto& bound : g_captured_values) {
        if (bound.text == expected) return true;
    }
    return false;
}

struct ScopedGameOverHooks {
    ScopedGameOverHooks() {
        g_captured_values.clear();
        set_game_over_screen_test_hooks(&no_op_layout_render, &capture_scoreboard_value);
    }
    ~ScopedGameOverHooks() { reset_game_over_screen_test_hooks(); }
};

} // namespace

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

TEST_CASE("game_over: render binds score/high-score/reason into scoreboard draw path",
          "[game_over][ui]") {
    auto reg = make_registry();
    ScopedGameOverHooks hooks;

    auto& score = reg.ctx().get<ScoreState>();
    score.score = 31415;
    score.high_score = 92653;

    auto& gos = reg.ctx().get<GameOverState>();
    gos.cause = DeathCause::MissedABeat;

    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.0f;

    render_game_over_screen_ui(reg);

    CHECK(has_bound_text("31415"));
    CHECK(has_bound_text("92653"));
    CHECK(has_bound_text("MISSED A BEAT"));
}

TEST_CASE("game_over: render omits empty reason binding for DeathCause::None", "[game_over][ui]") {
    auto reg = make_registry();
    ScopedGameOverHooks hooks;

    auto& score = reg.ctx().get<ScoreState>();
    score.score = 7;
    score.high_score = 11;

    auto& gos = reg.ctx().get<GameOverState>();
    gos.cause = DeathCause::None;

    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.0f;

    render_game_over_screen_ui(reg);

    CHECK(has_bound_text("7"));
    CHECK(has_bound_text("11"));
    CHECK_FALSE(has_bound_text("MISSED A BEAT"));
    CHECK_FALSE(has_bound_text("ENERGY DEPLETED"));
}
