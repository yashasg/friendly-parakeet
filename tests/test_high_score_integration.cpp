#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "components/high_score.h"
#include "systems/play_session.h"
#include "systems/ui_source_resolver.h"
#include "test_helpers.h"
#include "util/high_score_persistence.h"

namespace {

std::filesystem::path temp_high_score_path(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

void remove_path(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

}  // namespace

TEST_CASE("High score integration: setup_play_session loads selected song difficulty score",
          "[high_score][play_session]") {
    auto reg = make_registry();
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;
    lss.selected_difficulty = 0;

    auto& high_scores = reg.ctx().get<HighScoreState>();
    high_scores.scores["1_stomper|easy"] = 1234;
    high_scores.scores["1_stomper|hard"] = 9999;

    setup_play_session(reg);

    CHECK(reg.ctx().get<HighScoreState>().current_key == "1_stomper|easy");
    CHECK(reg.ctx().get<ScoreState>().high_score == 1234);
}

TEST_CASE("Play session: SongResults total_notes matches every shipped song difficulty",
          "[play_session][song_results][issue-114]") {
    for (int level = 0; level < LevelSelectState::LEVEL_COUNT; ++level) {
        for (int difficulty = 0; difficulty < LevelSelectState::DIFFICULTY_COUNT; ++difficulty) {
            auto reg = make_registry();
            auto& lss = reg.ctx().get<LevelSelectState>();
            lss.selected_level = level;
            lss.selected_difficulty = difficulty;

            setup_play_session(reg);

            const auto& beatmap = reg.ctx().get<BeatMap>();
            REQUIRE_FALSE(beatmap.beats.empty());
            const int expected_total = static_cast<int>(beatmap.beats.size());

            CAPTURE(LevelSelectState::LEVELS[level].title);
            CAPTURE(LevelSelectState::DIFFICULTY_KEYS[difficulty]);
            CHECK(reg.ctx().get<SongResults>().total_notes == expected_total);
            CHECK(resolve_ui_int_source(reg, "SongResults.total_notes").value() == expected_total);
        }
    }
}

TEST_CASE("High score integration: new song-complete high score persists",
          "[high_score][gamestate]") {
    const auto file = temp_high_score_path("shapeshifter_high_score_song_complete.json");
    remove_path(file);

    auto reg = make_registry();
    reg.ctx().get<HighScorePersistence>().path = file.string();
    auto& high_scores = reg.ctx().get<HighScoreState>();
    high_scores.set_current_key("song_001", "easy");
    high_scores.scores["song_001|easy"] = 1000;

    auto& score = reg.ctx().get<ScoreState>();
    score.high_score = 1000;
    score.score = 2500;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::SongComplete;

    game_state_system(reg, 0.016f);

    CHECK(score.high_score == 2500);
    CHECK(high_scores.scores.at("song_001|easy") == 2500);

    HighScoreState loaded;
    REQUIRE(high_score::load_high_scores(loaded, file));
    CHECK(loaded.scores.at("song_001|easy") == 2500);

    remove_path(file);
}

TEST_CASE("High score integration: lower game-over score does not overwrite persisted score",
          "[high_score][gamestate]") {
    const auto file = temp_high_score_path("shapeshifter_high_score_lower_game_over.json");
    remove_path(file);

    auto reg = make_registry();
    reg.ctx().get<HighScorePersistence>().path = file.string();
    auto& high_scores = reg.ctx().get<HighScoreState>();
    high_scores.set_current_key("song_001", "easy");
    high_scores.scores["song_001|easy"] = 3000;

    auto& score = reg.ctx().get<ScoreState>();
    score.high_score = 3000;
    score.score = 1000;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    CHECK(score.high_score == 3000);
    CHECK(high_scores.scores.at("song_001|easy") == 3000);
    CHECK_FALSE(std::filesystem::exists(file));

    remove_path(file);
}
