#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

#include "components/high_score.h"
#include "content/level_content_config.h"
#include "systems/high_score_system.h"

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
    // Collision risk across our 9 shipped keys is negligible.
    const auto hash_easy = high_score::make_key_hash("song_001", "easy");
    const auto hash_hard = high_score::make_key_hash("song_001", "hard");
    CHECK(hash_easy == high_score::make_key_hash("song_001", "easy"));
    CHECK(hash_easy != hash_hard);
    CHECK(hash_easy != high_score::make_key_hash("song_002", "easy"));
}

TEST_CASE("High score: make_key_str produces correct key string", "[high_score]") {
    char buf[HighScoreState::KEY_CAP]{};
    int32_t n = high_score::make_key_str(buf, HighScoreState::KEY_CAP, "song_001", "easy");
    CHECK(n > 0);
    CHECK(std::string_view{buf} == "song_001|easy");
}

TEST_CASE("High score: make_key_str rejects invalid capacity", "[high_score]") {
    char buf[HighScoreState::KEY_CAP]{};
    buf[0] = 'x';

    CHECK(high_score::make_key_str(buf, 0, "song_001", "easy") == -1);
    CHECK(buf[0] == 'x');

    CHECK(high_score::make_key_str(buf, -1, "song_001", "easy") == -1);
    CHECK(buf[0] == 'x');
}

TEST_CASE("High score: no hash collisions across all 9 shipped song+difficulty keys", "[high_score]") {
    // FNV-1a 32-bit collisions across exactly 9 short ASCII keys are negligible —
    // this test confirms the shipped set is collision-free and will catch any future
    // key additions that happen to collide.
    constexpr int32_t key_count = content_config::LEVEL_COUNT * content_config::DIFFICULTY_COUNT;

    entt::hashed_string::hash_type hashes[key_count]{};
    int32_t idx = 0;
    for (const char* song : content_config::LEVEL_KEYS) {
        for (const char* diff : content_config::DIFFICULTY_KEYS) {
            hashes[idx++] = high_score::make_key_hash(song, diff);
        }
    }
    REQUIRE(idx == key_count);

    for (int32_t i = 0; i < key_count; ++i) {
        for (int32_t j = i + 1; j < key_count; ++j) {
            CHECK(hashes[i] != hashes[j]);
        }
    }
}

TEST_CASE("High score: current score lookup and update never lowers stored score", "[high_score]") {
    HighScoreState state;
    CHECK(high_score::get_current_high_score(state) == 0);

    // ensure_entry + current_key_hash mirrors the production setup_play_session path.
    REQUIRE(high_score::ensure_entry(state, "song_001|easy"));
    state.current_key_hash = high_score::make_key_hash("song_001", "easy");
    CHECK(high_score::get_current_high_score(state) == 0);

    CHECK(high_score::update_if_higher(state, 5000));
    CHECK(high_score::get_current_high_score(state) == 5000);

    CHECK(high_score::update_if_higher(state, 1000));
    CHECK(high_score::get_current_high_score(state) == 5000);
}

TEST_CASE("High score: saturated table reports insert and hash update failures", "[high_score][issue1004]") {
    HighScoreState state;
    for (int32_t i = 0; i < HighScoreState::MAX_ENTRIES; ++i) {
        const std::string key = "song_" + std::to_string(i) + "|easy";
        REQUIRE(high_score::set_score(state, key.c_str(), i * 100));
    }

    CHECK(state.entry_count == HighScoreState::MAX_ENTRIES);
    CHECK_FALSE(high_score::set_score(state, "overflow|easy", 9000));
    CHECK_FALSE(high_score::ensure_entry(state, "overflow|medium"));

    state.current_key_hash = high_score::make_key_hash("overflow", "hard");
    CHECK_FALSE(high_score::update_if_higher(state, 9000));
    CHECK(high_score::get_score(state, "overflow|hard") == 0);

    state.current_key_hash = high_score::make_key_hash("song_0", "easy");
    CHECK(high_score::update_if_higher(state, 9000));
    CHECK(high_score::get_score(state, "song_0|easy") == 9000);
}

TEST_CASE("High score: tracks songs and difficulties independently", "[high_score]") {
    HighScoreState state;

    REQUIRE(high_score::ensure_entry(state, "song_001|easy"));
    state.current_key_hash = high_score::make_key_hash("song_001", "easy");
    CHECK(high_score::update_if_higher(state, 1000));

    REQUIRE(high_score::ensure_entry(state, "song_001|hard"));
    state.current_key_hash = high_score::make_key_hash("song_001", "hard");
    CHECK(high_score::update_if_higher(state, 2000));

    REQUIRE(high_score::ensure_entry(state, "song_002|easy"));
    state.current_key_hash = high_score::make_key_hash("song_002", "easy");
    CHECK(high_score::update_if_higher(state, 3000));

    CHECK(high_score::get_score(state, "song_001|easy") == 1000);
    CHECK(high_score::get_score(state, "song_001|hard") == 2000);
    CHECK(high_score::get_score(state, "song_002|easy") == 3000);
}

TEST_CASE("High score persistence: round-trips score map", "[high_score]") {
    const auto dir = temp_high_score_path("shapeshifter_high_score_roundtrip");
    const auto file = dir / "high_scores.json";
    remove_path(dir);

    HighScoreState original;
    high_score::set_score(original, "song_001|easy", 1000);
    high_score::set_score(original, "song_001|hard", 2000);
    high_score::set_score(original, "song_002|easy", 1500);

    REQUIRE(high_score::save_high_scores(original, file).ok());

    HighScoreState loaded;
    REQUIRE(high_score::load_high_scores(loaded, file).ok());
    CHECK(high_score::get_score(loaded, "song_001|easy") == high_score::get_score(original, "song_001|easy"));
    CHECK(high_score::get_score(loaded, "song_001|hard") == high_score::get_score(original, "song_001|hard"));
    CHECK(high_score::get_score(loaded, "song_002|easy") == high_score::get_score(original, "song_002|easy"));

    remove_path(dir);
}

TEST_CASE("High score persistence: supports current-directory files", "[high_score]") {
    const auto file = std::filesystem::path("high_scores_current_dir_tmp.json");
    remove_path(file);

    HighScoreState original;
    high_score::set_score(original, "song_001|easy", 2000);

    REQUIRE(high_score::save_high_scores(original, file).ok());

    HighScoreState loaded;
    REQUIRE(high_score::load_high_scores(loaded, file).ok());
    CHECK(high_score::get_score(loaded, "song_001|easy") == 2000);

    remove_path(file);
}

TEST_CASE("High score persistence: missing file returns false and preserves state", "[high_score]") {
    const auto file = temp_high_score_path("missing_high_scores_xyz.json");
    remove_path(file);

    HighScoreState state;
    high_score::set_score(state, "song_001|easy", 500);

    const auto result = high_score::load_high_scores(state, file);
    CHECK(result.status == persistence::Status::MissingFile);
    CHECK(high_score::get_score(state, "song_001|easy") == 500);
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
    high_score::set_score(state, "song_001|easy", 500);

    const auto result = high_score::load_high_scores(state, file);
    CHECK(result.status == persistence::Status::CorruptData);
    CHECK(high_score::get_score(state, "song_001|easy") == 500);

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
    high_score::set_score(state, "song_001|easy", 500);

    CHECK(high_score::load_high_scores(state, file).status == persistence::Status::CorruptData);
    CHECK(high_score::get_score(state, "song_001|easy") == 500);

    {
        std::ofstream out(file);
        out << R"({"scores":{"song_001|easy":"not_a_number"}})";
    }

    CHECK(high_score::load_high_scores(state, file).status == persistence::Status::CorruptData);
    CHECK(high_score::get_score(state, "song_001|easy") == 500);

    remove_path(dir);
}

TEST_CASE("High score persistence: oversized score map is corrupt", "[high_score][issue1004]") {
    const auto dir = temp_high_score_path("shapeshifter_high_score_too_many_entries");
    const auto file = dir / "too_many_scores.json";
    remove_path(dir);
    std::filesystem::create_directories(dir);

    {
        std::ofstream out(file);
        out << R"({"scores":{)";
        for (int32_t i = 0; i <= HighScoreState::MAX_ENTRIES; ++i) {
            if (i > 0) out << ',';
            out << R"("song_)" << i << R"(|easy":)" << i;
        }
        out << "}}";
    }

    HighScoreState state;
    REQUIRE(high_score::set_score(state, "song_001|easy", 500));

    CHECK(high_score::load_high_scores(state, file).status == persistence::Status::CorruptData);
    CHECK(high_score::get_score(state, "song_001|easy") == 500);

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
    REQUIRE(high_score::load_high_scores(state, file).ok());
    CHECK(high_score::get_score(state, "negative|easy") == 0);
    CHECK(high_score::get_score(state, "huge|hard") == std::numeric_limits<int32_t>::max());

    remove_path(dir);
}

TEST_CASE("High score persistence: load preserves current active key", "[high_score]") {
    const auto dir = temp_high_score_path("shapeshifter_high_score_current_key");
    const auto file = dir / "high_scores.json";
    remove_path(dir);

    HighScoreState original;
    high_score::set_score(original, "song_001|easy", 1000);
    REQUIRE(high_score::save_high_scores(original, file).ok());

    HighScoreState loaded;
    // Simulate a session already active on song_002|hard before the load.
    const auto pre_load_hash = high_score::make_key_hash("song_002", "hard");
    loaded.current_key_hash = pre_load_hash;
    REQUIRE(high_score::load_high_scores(loaded, file).ok());

    // Load must not clobber the in-progress session key.
    CHECK(loaded.current_key_hash == pre_load_hash);
    CHECK(high_score::get_score(loaded, "song_001|easy") == 1000);

    remove_path(dir);
}

TEST_CASE("High score persistence: save reports unwritable parent path", "[high_score]") {
    const auto root = std::filesystem::path("test_high_score_unwritable_parent");
    const auto not_a_dir = root / "not_a_directory";
    const auto file = not_a_dir / "high_scores.json";

    remove_path(root);
    std::filesystem::create_directories(root);
    {
        std::ofstream out(not_a_dir);
        REQUIRE(out.is_open());
        out << "file blocks directory creation";
    }

    HighScoreState state;
    high_score::set_score(state, "song_001|easy", 1000);
    const auto result = high_score::save_high_scores(state, file);
    CHECK(result.status == persistence::Status::DirectoryCreateFailed);

    remove_path(root);
}

TEST_CASE("High score persistence: save creates nested parent directories",
          "[high_score][persistence][issue-1025]") {
    const auto root = std::filesystem::path("test_high_score_nested_parent");
    const auto file = root / "missing" / "parent" / "high_scores.json";
    remove_path(root);

    HighScoreState original;
    high_score::set_score(original, "song_001|easy", 1000);
    REQUIRE(high_score::save_high_scores(original, file).ok());
    CHECK(std::filesystem::exists(file));

    HighScoreState loaded;
    REQUIRE(high_score::load_high_scores(loaded, file).ok());
    CHECK(high_score::get_score(loaded, "song_001|easy") == 1000);

    remove_path(root);
}

TEST_CASE("High score helper: file path resolution reports failure without CWD fallback", "[high_score]") {
    const auto root = std::filesystem::path("test_high_score_path_resolution_failure");
    const auto blocked_root = root / "blocked_root";
    remove_path(root);
    std::filesystem::create_directories(root);
    {
        std::ofstream out(blocked_root);
        REQUIRE(out.is_open());
        out << "file blocks directory creation";
    }

    std::filesystem::path file_path = "seed_should_clear.json";
    const auto result = high_score::get_high_scores_file_path(file_path, blocked_root);
    CHECK(result.status == persistence::Status::DirectoryCreateFailed);
    CHECK(file_path.empty());

    remove_path(root);
}
