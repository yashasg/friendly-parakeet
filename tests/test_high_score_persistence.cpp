#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>

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
    // make_key_hash() produces a stable FNV-1a hash: same inputs → same hash, different inputs → different hash.
    // Collision risk across our 9 shipped keys ("1_stomper|easy" … "3_mental|hard") is negligible.
    const auto hash_easy = HighScoreState::make_key_hash("song_001", "easy");
    const auto hash_hard = HighScoreState::make_key_hash("song_001", "hard");
    CHECK(hash_easy == HighScoreState::make_key_hash("song_001", "easy"));
    CHECK(hash_easy != hash_hard);
    CHECK(hash_easy != HighScoreState::make_key_hash("song_002", "easy"));
}

TEST_CASE("High score: make_key_str produces correct key string", "[high_score]") {
    char buf[HighScoreState::KEY_CAP]{};
    int32_t n = HighScoreState::make_key_str(buf, HighScoreState::KEY_CAP, "song_001", "easy");
    CHECK(n > 0);
    CHECK(std::string_view{buf} == "song_001|easy");
}

TEST_CASE("High score: no hash collisions across all 9 shipped song+difficulty keys", "[high_score]") {
    // FNV-1a 32-bit collisions across exactly 9 short ASCII keys are negligible —
    // this test confirms the shipped set is collision-free and will catch any future
    // key additions that happen to collide.
    const char* songs[]       = {"1_stomper", "2_drama", "3_mental"};
    const char* diffs[]       = {"easy", "medium", "hard"};

    entt::hashed_string::hash_type hashes[9]{};
    int32_t idx = 0;
    for (const char* song : songs) {
        for (const char* diff : diffs) {
            hashes[idx++] = HighScoreState::make_key_hash(song, diff);
        }
    }

    // All 9 hashes must be distinct.
    for (int32_t i = 0; i < 9; ++i) {
        for (int32_t j = i + 1; j < 9; ++j) {
            CHECK(hashes[i] != hashes[j]);
        }
    }
}

TEST_CASE("High score: current score lookup and update never lowers stored score", "[high_score]") {
    HighScoreState state;
    CHECK(state.get_current_high_score() == 0);

    // ensure_entry + current_key_hash mirrors the production setup_play_session path.
    state.ensure_entry("song_001|easy");
    state.current_key_hash = HighScoreState::make_key_hash("song_001", "easy");
    CHECK(state.get_current_high_score() == 0);

    high_score::update_if_higher(state, 5000);
    CHECK(state.get_current_high_score() == 5000);

    high_score::update_if_higher(state, 1000);
    CHECK(state.get_current_high_score() == 5000);
}

TEST_CASE("High score: tracks songs and difficulties independently", "[high_score]") {
    HighScoreState state;

    state.ensure_entry("song_001|easy");
    state.current_key_hash = HighScoreState::make_key_hash("song_001", "easy");
    high_score::update_if_higher(state, 1000);

    state.ensure_entry("song_001|hard");
    state.current_key_hash = HighScoreState::make_key_hash("song_001", "hard");
    high_score::update_if_higher(state, 2000);

    state.ensure_entry("song_002|easy");
    state.current_key_hash = HighScoreState::make_key_hash("song_002", "easy");
    high_score::update_if_higher(state, 3000);

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
    // Simulate a session already active on song_002|hard before the load.
    const auto pre_load_hash = HighScoreState::make_key_hash("song_002", "hard");
    loaded.current_key_hash = pre_load_hash;
    REQUIRE(high_score::load_high_scores(loaded, file));

    // Load must not clobber the in-progress session key.
    CHECK(loaded.current_key_hash == pre_load_hash);
    CHECK(loaded.get_score("song_001|easy") == 1000);

    remove_path(dir);
}
