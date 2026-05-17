// Game Over screen entity-driven UI tests (issue #1293, refs #1287, #1193 OoS-A).
//
// The legacy `app/ui/screen_controllers/game_over_screen_controller.cpp`
// render path was removed from `app/systems/ui_render_system.cpp` this
// cycle. The hook-based scoreboard tests that mocked the layout-render +
// value-draw hooks have been replaced with bind-system + lifecycle
// assertions that operate on the entity-driven UI (`UiLabelTag` entities
// spawned by `spawn_game_over_screen()`).
//
// The button-action / debounce / lifecycle tests for Game Over live in
// `tests/test_ui_update_system.cpp` alongside the parallel Song Complete
// and Paused coverage; this file keeps the .rgl source-of-truth snapshot
// assertions (issue #533) and the registry-singleton smoke test.

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>

#include "test_helpers.h"
#include "components/scoring.h"
#include "components/song_state.h"
#include "tags/tags.h"

namespace {

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

// Issue #1293 + #533: the .rgl is the source of truth — a layout edit
// must preserve the SCORE / HIGH SCORE labels alongside their dynamic-text
// slots so the entity-driven render keeps surfacing them after every
// regeneration.
TEST_CASE("game_over: rgl declares SCORE and HIGH SCORE labels alongside slots (#533)",
          "[game_over][ui][issue533]") {
    const std::string path = "content/ui/screens/game_over.rgl";
    REQUIRE(rgl_has_label(path, "ScoreLabel", "SCORE"));
    REQUIRE(rgl_has_label(path, "HighScoreLabel", "HIGH SCORE"));
}

// Issue #1293 + #533: codegen must materialize the SCORE / HIGH SCORE
// labels into the generated spawner so the runtime renders them.
TEST_CASE("game_over: generated spawner emits SCORE and HIGH SCORE labels (#533)",
          "[game_over][ui][issue533]") {
    const std::string spawner = read_file("app/systems/generated/game_over_screen.cpp");
    REQUIRE_FALSE(spawner.empty());
    CHECK(spawner.find("\"SCORE\"") != std::string::npos);
    CHECK(spawner.find("\"HIGH SCORE\"") != std::string::npos);
    CHECK(spawner.find("GameOverScreenTag") != std::string::npos);
}

// Registry-singleton smoke test: the components the game_over scoreboard
// bind system reads (ScoreState, CurrentSongHighScore, EnergyDepletedDeath
// ctx tag) must be discoverable from a make_registry() snapshot for a
// finished run. This pins the contract without depending on any render or
// bind code path.
TEST_CASE("game_over: scoreboard inputs visible via registry singletons",
          "[game_over][ui]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 12345;
    current.value = 99999;
    reg.ctx().insert_or_assign(EnergyDepletedDeath{});

    CHECK(reg.ctx().get<ScoreState>().score             == 12345);
    CHECK(reg.ctx().get<CurrentSongHighScore>().value   == 99999);
    CHECK(reg.ctx().find<EnergyDepletedDeath>() != nullptr);
}
