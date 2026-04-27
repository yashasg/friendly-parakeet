#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <limits>

#include "components/high_score.h"
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

TEST_CASE("High score: keys are stable per song and difficulty", "[high_score]") {
    CHECK(HighScoreState::make_key("song_001", "easy") == "song_001|easy");
    CHECK(HighScoreState::make_key("song_001", "easy")
        == HighScoreState::make_key("song_001", "easy"));
    CHECK(HighScoreState::make_key("song_001", "easy")
        != HighScoreState::make_key("song_001", "hard"));
}

TEST_CASE("High score: current score lookup and update never lowers stored score", "[high_score]") {
    HighScoreState state;
    CHECK(state.get_current_high_score() == 0);

    state.set_current_key("song_001", "easy");
    CHECK(state.get_current_high_score() == 0);

    state.update_current_high_score(5000);
    CHECK(state.get_current_high_score() == 5000);

    state.update_current_high_score(1000);
    CHECK(state.get_current_high_score() == 5000);
}

TEST_CASE("High score: tracks songs and difficulties independently", "[high_score]") {
    HighScoreState state;

    state.set_current_key("song_001", "easy");
    state.update_current_high_score(1000);

    state.set_current_key("song_001", "hard");
    state.update_current_high_score(2000);

    state.set_current_key("song_002", "easy");
    state.update_current_high_score(3000);

    CHECK(state.get_score("song_001|easy") == 1000);
    CHECK(state.get_score("song_001|hard") == 2000);
    CHECK(state.get_score("song_002|easy") == 3000);
}

TEST_CASE("High score persistence: round-trips score map", "[high_score]") {
    const auto dir = temp_high_score_path("shapeshifter_high_score_roundtrip");
    const auto file = dir / "high_scores.json";
    remove_path(dir);

    HighScoreState original;
    original.set_score("song_001|easy", 1000);
    original.set_score("song_001|hard", 2000);
    original.set_score("song_002|easy", 1500);

    REQUIRE(high_score::save_high_scores(original, file));

    HighScoreState loaded;
    REQUIRE(high_score::load_high_scores(loaded, file));
    CHECK(loaded.get_score("song_001|easy") == original.get_score("song_001|easy"));
    CHECK(loaded.get_score("song_001|hard") == original.get_score("song_001|hard"));
    CHECK(loaded.get_score("song_002|easy") == original.get_score("song_002|easy"));

    remove_path(dir);
}

TEST_CASE("High score persistence: supports current-directory files", "[high_score]") {
    const auto file = std::filesystem::path("high_scores_current_dir_tmp.json");
    remove_path(file);

    HighScoreState original;
    original.set_score("song_001|easy", 2000);

    REQUIRE(high_score::save_high_scores(original, file));

    HighScoreState loaded;
    REQUIRE(high_score::load_high_scores(loaded, file));
    CHECK(loaded.get_score("song_001|easy") == 2000);

    remove_path(file);
}

TEST_CASE("High score persistence: missing file returns false and preserves state", "[high_score]") {
    const auto file = temp_high_score_path("missing_high_scores_xyz.json");
    remove_path(file);

    HighScoreState state;
    state.set_score("song_001|easy", 500);

    CHECK_FALSE(high_score::load_high_scores(state, file));
    CHECK(state.get_score("song_001|easy") == 500);
}

TEST_CASE("High score persistence: malformed JSON preserves state", "[high_score]") {
    const auto dir = temp_high_score_path("shapeshifter_high_score_malformed");
    const auto file = dir / "bad_high_scores.json";
    remove_path(dir);
    std::filesystem::create_directories(dir);

    {
        std::ofstream out(file);
        out << "{ invalid json }";
    }

    HighScoreState state;
    state.set_score("song_001|easy", 500);

    CHECK_FALSE(high_score::load_high_scores(state, file));
    CHECK(state.get_score("song_001|easy") == 500);

    remove_path(dir);
}

TEST_CASE("High score persistence: invalid schema preserves state", "[high_score]") {
    const auto dir = temp_high_score_path("shapeshifter_high_score_invalid_schema");
    const auto file = dir / "bad_schema.json";
    remove_path(dir);
    std::filesystem::create_directories(dir);

    {
        std::ofstream out(file);
        out << R"({"scores":["not","an","object"]})";
    }

    HighScoreState state;
    state.set_score("song_001|easy", 500);

    CHECK_FALSE(high_score::load_high_scores(state, file));
    CHECK(state.get_score("song_001|easy") == 500);

    {
        std::ofstream out(file);
        out << R"({"scores":{"song_001|easy":"not_a_number"}})";
    }

    CHECK_FALSE(high_score::load_high_scores(state, file));
    CHECK(state.get_score("song_001|easy") == 500);

    remove_path(dir);
}

TEST_CASE("High score persistence: clamps negative and oversized scores", "[high_score]") {
    const auto dir = temp_high_score_path("shapeshifter_high_score_clamp");
    const auto file = dir / "clamped_scores.json";
    remove_path(dir);
    std::filesystem::create_directories(dir);

    {
        std::ofstream out(file);
        out << R"({"scores":{"negative|easy":-100,"huge|hard":999999999999}})";
    }

    HighScoreState state;
    REQUIRE(high_score::load_high_scores(state, file));
    CHECK(state.get_score("negative|easy") == 0);
    CHECK(state.get_score("huge|hard") == std::numeric_limits<int32_t>::max());

    remove_path(dir);
}

TEST_CASE("High score persistence: load preserves current active key", "[high_score]") {
    const auto dir = temp_high_score_path("shapeshifter_high_score_current_key");
    const auto file = dir / "high_scores.json";
    remove_path(dir);

    HighScoreState original;
    original.set_score("song_001|easy", 1000);
    REQUIRE(high_score::save_high_scores(original, file));

    HighScoreState loaded;
    loaded.set_current_key("song_002", "hard");
    REQUIRE(high_score::load_high_scores(loaded, file));

    CHECK(loaded.current_key == "song_002|hard");
    CHECK(loaded.get_score("song_001|easy") == 1000);

    remove_path(dir);
}
