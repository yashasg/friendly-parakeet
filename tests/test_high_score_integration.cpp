#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "components/high_score.h"
#include "util/level_content_config.h"
#include "entities/camera_entity.h"
#include "entities/obstacle_entity.h"
#include "entities/obstacle_render_entity.h"
#include "systems/game_phase_transition.h"
#include "systems/play_session.h"
#include "test_helpers.h"
#include "systems/high_score_system.h"

namespace {

std::filesystem::path temp_high_score_path(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

void remove_path(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

struct TempBeatMapFile {
    explicit TempBeatMapFile(const char* name, const std::string& json)
        : path(std::filesystem::temp_directory_path() / name) {
        std::filesystem::remove(path);
        std::ofstream out(path);
        REQUIRE(out.good());
        out << json;
    }

    ~TempBeatMapFile() {
        std::filesystem::remove(path);
    }

    std::filesystem::path path;
};

int count_result_notes(const BeatMap& beatmap) {
    return static_cast<int>(shape_gate_count(beatmap) +
                            split_path_count(beatmap));
}

int count_mesh_children(entt::registry& reg) {
    int count = 0;
    auto view = reg.view<MeshChild>();
    for ([[maybe_unused]] auto entity : view) {
        ++count;
    }
    return count;
}

}  // namespace

TEST_CASE("High score integration: setup_play_session loads selected song difficulty score",
          "[high_score][play_session]") {
    auto reg = make_registry();
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;
    lss.selected_difficulty = 0;

    auto& high_scores = reg.ctx().get<HighScoreState>();
    high_score::set_score(high_scores, "1_stomper|easy", 1234);
    high_score::set_score(high_scores, "1_stomper|hard", 9999);

    setup_play_session(reg);

    CHECK(reg.ctx().get<HighScoreSession>().key_hash
        == high_score::make_key_hash("1_stomper", "easy"));
    CHECK(reg.ctx().get<CurrentSongHighScore>().value == 1234);
    CHECK_FALSE(reg.view<GameCamera>().empty());
    CHECK_FALSE(reg.view<UICamera>().empty());
}

TEST_CASE("Play session: invalid level-select indices fall back before content lookup",
          "[play_session][issue738]") {
    auto reg = make_registry();
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 99;
    lss.selected_difficulty = 99;

    setup_play_session(reg);

    CHECK(lss.selected_level == content_config::DEFAULT_LEVEL_INDEX);
    CHECK(lss.selected_difficulty == content_config::DEFAULT_DIFFICULTY_INDEX);
    CHECK(reg.ctx().get<HighScoreSession>().key_hash
        == high_score::make_key_hash("1_stomper", "medium"));
}

TEST_CASE("Play session: restart clears obstacle mesh children without stale listener state",
          "[play_session][issue957]") {
    auto reg = make_registry();
    auto obstacle = spawn_shape_gate_obstacle(reg, {360.0f, -120.0f, Shape::Circle});
    REQUIRE(reg.all_of<ObstacleChildren>(obstacle));
    const auto children = reg.get<ObstacleChildren>(obstacle);
    REQUIRE(children.count > 0);
    const entt::entity first_child = children.children[0];
    REQUIRE(reg.valid(first_child));

    setup_play_session(reg);

    CHECK_FALSE(reg.valid(obstacle));
    CHECK_FALSE(reg.valid(first_child));
    CHECK(count_mesh_children(reg) == 0);

    auto next_obstacle = spawn_shape_gate_obstacle(reg, {360.0f, -120.0f, Shape::Square});
    REQUIRE(count_mesh_children(reg) > 0);

    destroy_obstacle_with_children(reg, next_obstacle);

    CHECK(count_mesh_children(reg) == 0);
}

TEST_CASE("Play session: missing selected beatmap returns to level select without score session",
          "[play_session][issue835]") {
    auto reg = make_registry();
    auto& lss = reg.ctx().get<LevelSelectState>();
    auto& high_scores = reg.ctx().get<HighScoreState>();
    high_score::set_score(high_scores, "1_stomper|medium", 4321);
    auto& session = reg.ctx().get<HighScoreSession>();
    session.key_hash = high_score::make_key_hash("1_stomper", "medium");
    reg.ctx().insert_or_assign(LevelSelectConfirmedTag{});

    reg.ctx().emplace<PlaySessionContentOverride>(
        PlaySessionContentOverride{"content/beatmaps/does_not_exist_835.json", "medium"});

    setup_play_session(reg);

    CHECK(reg.ctx().contains<GamePhaseLevelSelectTag>());
    CHECK_FALSE(reg.ctx().contains<LevelSelectConfirmedTag>());
    {
        const auto& bm = beat_map(reg);
        CHECK(beat_map_empty(bm));
    }
    CHECK(reg.view<PlayerTag>().empty());
    CHECK(reg.ctx().find<ScoreState>() == nullptr);
    CHECK(session.key_hash == 0);
    CHECK(high_scores.entry_count == 1);

    reg.ctx().erase<PlaySessionContentOverride>();
    lss.selected_level = content_config::DEFAULT_LEVEL_INDEX;
    lss.selected_difficulty = content_config::DEFAULT_DIFFICULTY_INDEX;
    reg.ctx().insert_or_assign(LevelSelectConfirmedTag{});

    REQUIRE_NOTHROW(setup_play_session(reg));
    CHECK(reg.ctx().find<SongState>() != nullptr);
    CHECK(reg.ctx().find<ScoreState>() != nullptr);
    CHECK(reg.ctx().contains<GamePhasePlayingTag>());
    CHECK_FALSE(reg.view<PlayerTag>().empty());
}

TEST_CASE("Play session: high score key uses loaded fallback difficulty",
          "[play_session][high_score][issue847]") {
    const TempBeatMapFile beatmap("shapeshifter_issue847_beatmap.json", R"json({
        "song_id": "issue847",
        "title": "Issue 847",
        "bpm": 120,
        "offset": 0,
        "lead_beats": 4,
        "duration": 8,
        "difficulties": {
            "medium": {
                "beats": [
                    {"beat": 4, "kind": "shape_gate", "shape": "circle", "lane": 1}
                ]
            }
        }
    })json");

    auto reg = make_registry();
    auto& high_scores = reg.ctx().get<HighScoreState>();
    high_score::set_score(high_scores, "shapeshifter_issue847|medium", 4321);
    high_score::set_score(high_scores, "shapeshifter_issue847|hard", 9999);
    reg.ctx().emplace<PlaySessionContentOverride>(
        PlaySessionContentOverride{beatmap.path.string(), "hard"});

    setup_play_session(reg);

    CHECK(beat_map(reg).difficulty == "medium");
    CHECK(reg.ctx().get<HighScoreSession>().key_hash
        == high_score::make_key_hash("shapeshifter_issue847", "medium"));
    CHECK(reg.ctx().get<CurrentSongHighScore>().value == 4321);
}

TEST_CASE("Play session: SongResults total_notes excludes onset marker metadata",
          "[play_session][song_results][issue-773]") {
    bool saw_onset_marker = false;
    for (int level = 0; level < content_config::LEVEL_COUNT; ++level) {
        for (int difficulty = 0; difficulty < content_config::DIFFICULTY_COUNT; ++difficulty) {
            auto reg = make_registry();
            auto& lss = reg.ctx().get<LevelSelectState>();
            lss.selected_level = level;
            lss.selected_difficulty = difficulty;

            setup_play_session(reg);

            const auto& beatmap = beat_map(reg);
            const size_t total_beats = shape_gate_count(beatmap) +
                                       split_path_count(beatmap) +
                                       beatmap.onset_marker_beats.size();
            REQUIRE(total_beats > 0);
            saw_onset_marker = saw_onset_marker || !beatmap.onset_marker_beats.empty();

            CAPTURE(content_config::LEVELS[level].title);
            CAPTURE(content_config::DIFFICULTY_KEYS[difficulty]);
            CHECK(reg.ctx().get<SongResults>().total_notes == count_result_notes(beatmap));
        }
    }
    CHECK(saw_onset_marker);
}

TEST_CASE("High score integration: new song-complete high score persists",
          "[high_score][gamestate]") {
    const auto file = temp_high_score_path("shapeshifter_high_score_song_complete.json");
    remove_path(file);

    auto reg = make_registry();
    reg.ctx().get<HighScorePersistence>().path = file.string();
    auto& high_scores = reg.ctx().get<HighScoreState>();
    high_score::ensure_entry(high_scores, "song_001|easy");
    reg.ctx().get<HighScoreSession>().key_hash = high_score::make_key_hash("song_001", "easy");
    high_score::set_score(high_scores, "song_001|easy", 1000);

    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    current.value = 1000;
    score.score = 2500;

    request_phase_transition<NextPhaseSongCompleteTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(current.value == 2500);
    CHECK(high_score::get_score(high_scores, "song_001|easy") == 2500);

    HighScoreState loaded;
    REQUIRE(high_score::load_high_scores(loaded, file).ok());
    CHECK(high_score::get_score(loaded, "song_001|easy") == 2500);

    remove_path(file);
}

TEST_CASE("High score integration: lower game-over score does not overwrite persisted score",
          "[high_score][gamestate]") {
    const auto file = temp_high_score_path("shapeshifter_high_score_lower_game_over.json");
    remove_path(file);

    auto reg = make_registry();
    reg.ctx().get<HighScorePersistence>().path = file.string();
    auto& high_scores = reg.ctx().get<HighScoreState>();
    high_score::ensure_entry(high_scores, "song_001|easy");
    reg.ctx().get<HighScoreSession>().key_hash = high_score::make_key_hash("song_001", "easy");
    high_score::set_score(high_scores, "song_001|easy", 3000);

    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    current.value = 3000;
    score.score = 1000;

    request_phase_transition<NextPhaseGameOverTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(current.value == 3000);
    CHECK(high_score::get_score(high_scores, "song_001|easy") == 3000);
    CHECK_FALSE(std::filesystem::exists(file));

    remove_path(file);
}

TEST_CASE("High score integration: missing active entry does not report new best",
          "[high_score][gamestate][issue1004]") {
    auto reg = make_registry();
    auto& high_scores = reg.ctx().get<HighScoreState>();
    for (int32_t i = 0; i < HighScoreState::MAX_ENTRIES; ++i) {
        const std::string key = "song_" + std::to_string(i) + "|easy";
        REQUIRE(high_score::set_score(high_scores, key.c_str(), i * 100));
    }
    reg.ctx().get<HighScoreSession>().key_hash = high_score::make_key_hash("overflow", "hard");

    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    current.value = 1000;
    score.score = 2500;

    request_phase_transition<NextPhaseSongCompleteTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(current.value == 1000);
    CHECK_FALSE(reg.ctx().get<TerminalResultState>().new_best);
    CHECK_FALSE(reg.ctx().get<HighScorePersistence>().dirty);
    CHECK(high_score::get_score(high_scores, "overflow|hard") == 0);
}

TEST_CASE("High score integration: failed save keeps dirty state for retry",
          "[high_score][gamestate]") {
    const auto root = std::filesystem::path("test_high_score_retry_state");
    const auto blocked_parent = root / "blocked_parent";
    const auto blocked_file = blocked_parent / "high_scores.json";
    remove_path(root);
    std::filesystem::create_directories(root);
    {
        std::ofstream out(blocked_parent);
        REQUIRE(out.is_open());
        out << "file blocks directory creation";
    }

    auto reg = make_registry();
    reg.ctx().get<HighScorePersistence>().path = blocked_file.string();
    auto& high_scores = reg.ctx().get<HighScoreState>();
    high_score::ensure_entry(high_scores, "song_001|easy");
    reg.ctx().get<HighScoreSession>().key_hash = high_score::make_key_hash("song_001", "easy");
    high_score::set_score(high_scores, "song_001|easy", 1000);

    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    current.value = 1000;
    score.score = 2500;

    request_phase_transition<NextPhaseSongCompleteTag>(reg);

    game_state_system(reg, 0.016f);

    const auto& persistence_state = reg.ctx().get<HighScorePersistence>();
    CHECK(persistence_state.dirty);
    CHECK(persistence_state.last_save.status == persistence::Status::DirectoryCreateFailed);

    remove_path(root);
    const auto retry_file = root / "high_scores.json";
    reg.ctx().get<HighScorePersistence>().path = retry_file.string();
    score.score = 1000;
    request_phase_transition<NextPhaseGameOverTag>(reg);

    game_state_system(reg, 0.016f);

    const auto& retried_persistence = reg.ctx().get<HighScorePersistence>();
    CHECK_FALSE(retried_persistence.dirty);
    CHECK(retried_persistence.last_save.status == persistence::Status::Success);

    HighScoreState loaded;
    REQUIRE(high_score::load_high_scores(loaded, retry_file).ok());
    CHECK(high_score::get_score(loaded, "song_001|easy") == 2500);

    remove_path(root);
}

TEST_CASE("High score bootstrap: persistence path is populated for save call sites",
          "[high_score][gamestate][issue-302]") {
    const auto file = temp_high_score_path("shapeshifter_issue_302_high_scores_bootstrap.json");
    remove_path(file);

    HighScoreState seeded;
    high_score::set_score(seeded, "song_001|easy", 1000);
    REQUIRE(high_score::save_high_scores(seeded, file).ok());

    auto reg = make_registry();
    HighScoreState loaded_at_bootstrap;
    CHECK(high_score::load_high_scores(loaded_at_bootstrap, file).ok());
    reg.ctx().get<HighScoreState>() = loaded_at_bootstrap;
    reg.ctx().get<HighScorePersistence>() = HighScorePersistence{file.string()};
    reg.ctx().get<GameState>() = GameState{ 0.0f };
    sync_game_phase_tags<GamePhasePlayingTag>(reg);
    request_phase_transition<NextPhaseSongCompleteTag>(reg);
    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    current.value = 1000;
    score.score = 2500;

    auto& high_scores = reg.ctx().get<HighScoreState>();
    high_score::ensure_entry(high_scores, "song_001|easy");
    reg.ctx().get<HighScoreSession>().key_hash = high_score::make_key_hash("song_001", "easy");

    const auto& persistence = reg.ctx().get<HighScorePersistence>();
    REQUIRE_FALSE(persistence.path.empty());
    CHECK(persistence.path == file.string());

    game_state_system(reg, 0.016f);

    HighScoreState loaded;
    REQUIRE(high_score::load_high_scores(loaded, file).ok());
    CHECK(high_score::get_score(loaded, "song_001|easy") == 2500);

    remove_path(file);
}
