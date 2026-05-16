#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <fstream>
#include <sstream>
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

TEST_CASE("game_over: ENERGY DEPLETED reason renders when EnergyDepletedDeath tag is present",
          "[game_over][ui]") {
    auto reg = make_registry();
    ScopedGameOverHooks hooks;

    // Absent tag: no reason text appears.
    REQUIRE(reg.ctx().find<EnergyDepletedDeath>() == nullptr);
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.0f;

    render_game_over_screen_ui(reg);
    CHECK_FALSE(has_bound_text("ENERGY DEPLETED"));

    // Present tag: ENERGY DEPLETED reason is bound.
    g_captured_values.clear();
    reg.ctx().emplace<EnergyDepletedDeath>();
    render_game_over_screen_ui(reg);
    CHECK(has_bound_text("ENERGY DEPLETED"));
}

TEST_CASE("game_over: score / high score / cause are visible via registry singletons "
          "(value-binding contract mirrors song_complete)",
          "[game_over][ui]") {
    auto reg = make_registry();

    // Simulate a finished run.
    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 12345;
    current.value = 99999;

    reg.ctx().insert_or_assign(EnergyDepletedDeath{});

    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 1.0f;

    // Controller-bound values — these are exactly the fields the
    // game_over_screen_controller reads to populate the layout slots.
    const auto& s = reg.ctx().get<ScoreState>();
    CHECK(s.score      == 12345);
    CHECK(reg.ctx().get<CurrentSongHighScore>().value == 99999);

    CHECK(reg.ctx().find<EnergyDepletedDeath>() != nullptr);
}

TEST_CASE("game_over: render binds score/high-score/reason into scoreboard draw path",
          "[game_over][ui]") {
    auto reg = make_registry();
    ScopedGameOverHooks hooks;

    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 31415;
    current.value = 92653;

    reg.ctx().insert_or_assign(EnergyDepletedDeath{});

    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.0f;

    render_game_over_screen_ui(reg);

    CHECK(has_bound_text("31415"));
    CHECK(has_bound_text("92653"));
    CHECK(has_bound_text("ENERGY DEPLETED"));
}

TEST_CASE("game_over: render binds new-best badge and previous score", "[game_over][ui]") {
    auto reg = make_registry();
    ScopedGameOverHooks hooks;

    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 5000;
    current.value = 5000;
    reg.ctx().insert_or_assign(TerminalResultState{true, 3000});

    reg.ctx().insert_or_assign(EnergyDepletedDeath{});

    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.0f;

    render_game_over_screen_ui(reg);

    CHECK(has_bound_text("NEW BEST!"));
    CHECK(has_bound_text("PREV 3000"));
}

TEST_CASE("game_over: render omits empty reason binding when no death-cause tag present", "[game_over][ui]") {
    auto reg = make_registry();
    ScopedGameOverHooks hooks;

    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 7;
    current.value = 11;

    REQUIRE(reg.ctx().find<EnergyDepletedDeath>() == nullptr);

    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.0f;

    render_game_over_screen_ui(reg);

    CHECK(has_bound_text("7"));
    CHECK(has_bound_text("11"));
    CHECK_FALSE(has_bound_text("MISSED A BEAT"));
    CHECK_FALSE(has_bound_text("ENERGY DEPLETED"));
}

namespace {

// Slurp helper for the rgl/header snapshot tests below.
std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool rgl_has_label(const std::string& path, const std::string& name, const std::string& text) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line.front() != 'c') continue;
        std::istringstream ss(line);
        char kind = '\0';
        int id = 0, type = 0, x = 0, y = 0, w = 0, h = 0, anchor = 0;
        std::string ctrl_name;
        if (!(ss >> kind >> id >> type >> ctrl_name >> x >> y >> w >> h >> anchor)) continue;
        if (ctrl_name != name) continue;
        // Type 4 == GuiLabel in rguilayout. Trailing tokens are the label text.
        if (type != 4) return false;
        std::string token, joined;
        while (ss >> token) {
            if (!joined.empty()) joined += ' ';
            joined += token;
        }
        return joined == text;
    }
    return false;
}

}  // namespace

// Issue #533: Game Over screen must label the score / high-score numbers
// the same way Song Complete does. The labels are authored in the .rgl
// (so they survive a layout regeneration) and surfaced through the
// generated layout header (so they render at runtime, not only via the
// C++ controller).
TEST_CASE("game_over: rgl declares SCORE and HIGH SCORE labels alongside slots (#533)",
          "[game_over][ui][issue533]") {
    const std::string path = "content/ui/screens/game_over.rgl";
    REQUIRE(rgl_has_label(path, "ScoreLabel", "SCORE"));
    REQUIRE(rgl_has_label(path, "HighScoreLabel", "HIGH SCORE"));
}

TEST_CASE("game_over: generated layout renders SCORE and HIGH SCORE labels (#533)",
          "[game_over][ui][issue533]") {
    const std::string header = read_file("app/ui/generated/game_over_layout.h");
    REQUIRE_FALSE(header.empty());
    // The labels must be drawn by the generated layout itself, not only
    // by the screen controller — Song Complete's parallel labels are
    // emitted from its layout header.
    CHECK(header.find("\"SCORE\"") != std::string::npos);
    CHECK(header.find("\"HIGH SCORE\"") != std::string::npos);
    // Sanity: still uses the centered-label helper (consistent typography
    // with Song Complete) rather than a bare GuiLabel for the new text.
    const auto score_pos = header.find("\"SCORE\"");
    REQUIRE(score_pos != std::string::npos);
    const auto window_start = score_pos > 96 ? score_pos - 96 : 0;
    CHECK(header.substr(window_start, score_pos - window_start)
              .find("DrawCenteredLabel") != std::string::npos);
}
