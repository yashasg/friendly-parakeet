#include "play_session.h"
#include "tags/tags.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/scoring.h"
#include "../entities/beat_map.h"
#include "../components/rhythm.h"
#include "../components/song_state.h"
#include "../components/energy_bar.h"
#include "audio_resources.h"
#include "../components/high_score.h"
#include "high_score_system.h"
#include "../entities/settings.h"
#include "../util/rhythm_math.h"
#include "../util/app_dir_path.h"
#include "../entities/camera_entity.h"
#include "../entities/energy_bar_entity.h"
#include "../entities/player_entity.h"
#include "../util/level_content_config.h"
#include "all_systems.h"
#include "game_phase_transition.h"
#include "../constants.h"
#include <filesystem>
#include <raylib.h>
#include <utility>
#include <vector>

namespace {

template <typename T>
T& assign_or_emplace_ctx(entt::registry& reg, T value = T{}) {
    if (auto* existing = reg.ctx().find<T>()) {
        *existing = std::move(value);
        return *existing;
    }
    return reg.ctx().emplace<T>(std::move(value));
}

template <typename T>
void erase_ctx_if_exists(entt::registry& reg) {
    if (reg.ctx().find<T>()) {
        reg.ctx().erase<T>();
    }
}

} // namespace

void setup_play_session(entt::registry& reg) {
    const auto* previous_settings = find_settings_state(reg);
    const auto* previous_settings_persistence = find_settings_persistence(reg);
    const bool restore_settings = previous_settings != nullptr && previous_settings_persistence != nullptr;
    SettingsState settings = previous_settings ? *previous_settings : SettingsState{};
    SettingsPersistence settings_persistence =
        previous_settings_persistence ? *previous_settings_persistence : SettingsPersistence{};
    bool settings_was_dirty = false;
    if (restore_settings) {
        auto dirty_view = reg.view<SettingsTag, SettingsDirtyTag>();
        settings_was_dirty = dirty_view.begin() != dirty_view.end();
    }

    // Snapshot the HighScoreEntry row table across the upcoming `reg.clear()`
    // so durable scores survive the per-session entity wipe (Fabian Principle 3 /
    // issue #1560: the row table replaces the prior ctx-singleton array column,
    // and `reg.clear()` wipes entities but leaves ctx untouched).
    std::vector<HighScoreEntry> saved_high_score_entries;
    {
        auto entry_view = reg.view<HighScoreEntry>();
        saved_high_score_entries.reserve(entry_view.size());
        for (auto entity : entry_view) {
            saved_high_score_entries.push_back(entry_view.get<HighScoreEntry>(entity));
        }
    }

    reg.clear();

    for (const auto& entry : saved_high_score_entries) {
        const auto entity = reg.create();
        reg.emplace<HighScoreEntry>(entity, entry);
    }

    spawn_game_camera(reg);
    spawn_ui_camera(reg);
    create_energy_bar_entity(reg);
    create_beat_map_entity(reg);
    if (restore_settings) {
        const auto settings_entity =
            create_settings_entity(reg, settings, settings_persistence);
        if (settings_was_dirty) {
            reg.emplace<SettingsDirtyTag>(settings_entity);
        }
    }

    // Load beatmap from level selection. BeatMap is an entity singleton whose
    // cold asset data remains immutable for the duration of the play session.
    auto& lss = reg.ctx().get<LevelSelectState>();
    auto& beatmap = beat_map(reg);
    beatmap = BeatMap{};
    lss.selected_level = content_config::level_index_or_default(lss.selected_level);
    lss.selected_difficulty = content_config::difficulty_index_or_default(lss.selected_difficulty);
    std::string override_beatmap_path;
    std::string override_difficulty_key;
    const char* beatmap_path = content_config::LEVELS[lss.selected_level].beatmap_path;
    const char* difficulty_key = content_config::DIFFICULTY_KEYS[lss.selected_difficulty];
    if (const auto* override_content = reg.ctx().find<PlaySessionContentOverride>()) {
        override_beatmap_path = override_content->beatmap_path;
        override_difficulty_key = override_content->difficulty_key;
        if (!override_beatmap_path.empty()) {
            beatmap_path = override_beatmap_path.c_str();
        }
        if (!override_difficulty_key.empty()) {
            difficulty_key = override_difficulty_key.c_str();
        }
    }

    std::vector<BeatMapError> load_errors;
    std::vector<BeatMapError> reported_load_errors;
    std::string exe_path = util::join_app_dir(GetApplicationDirectory(), beatmap_path);
    const char* paths[] = { exe_path.c_str(), beatmap_path };

    bool loaded = false;
    for (const char* path : paths) {
        load_errors.clear();
        bool loaded_this_path = false;
        BeatMap candidate;
        if (load_beat_map(path, candidate, load_errors, difficulty_key)) {
            std::vector<BeatMapError> validation_errors;
            if (validate_beat_map(candidate, validation_errors)) {
                beatmap = std::move(candidate);
                loaded_this_path = true;
            } else {
                // Runtime tolerates the "Different-shape gates must be >= 3
                // beats apart" validation error and treats it as a warning;
                // any other validation error fails the load.
                bool only_allowed_errors = !validation_errors.empty();
                for (const BeatMapError& error : validation_errors) {
                    if (error.message != "Different-shape gates must be >= 3 beats apart") {
                        only_allowed_errors = false;
                    }
                    load_errors.push_back(error);
                }
                if (only_allowed_errors) {
                    for (const BeatMapError& error : validation_errors) {
                        TraceLog(LOG_WARNING, "Beatmap validation warning at beat %d: %s",
                                 error.beat_index, error.message.c_str());
                    }
                    beatmap = std::move(candidate);
                    loaded_this_path = true;
                }
            }
        }
        if (loaded_this_path) {
            const size_t total_beats = beat_map_total_count(beatmap);
            TraceLog(LOG_INFO, "Loaded beatmap: %s (%zu beats, difficulty=%s)",
                     path, total_beats, beatmap.difficulty.c_str());
            loaded = true;
            break;
        }
        reported_load_errors.insert(reported_load_errors.end(), load_errors.begin(), load_errors.end());
    }
    if (!loaded) {
        TraceLog(LOG_WARNING, "Failed to load beatmap: %s", beatmap_path);
        for (const BeatMapError& error : reported_load_errors) {
            TraceLog(LOG_WARNING, "Beatmap load error at beat %d: %s",
                     error.beat_index, error.message.c_str());
        }
        runtime_system_scratch_init(reg);
        if (auto* music = reg.ctx().find<MusicContext>()) {
            music->release();
        }
        if (auto* session = reg.ctx().find<HighScoreSession>()) {
            session->key_hash = 0;
        }
        erase_ctx_if_exists<ScoreState>(reg);
        erase_ctx_if_exists<ScoreDisplay>(reg);
        erase_ctx_if_exists<CurrentSongHighScore>(reg);
        erase_ctx_if_exists<SongState>(reg);
        erase_ctx_if_exists<BeatCursor>(reg);
        erase_ctx_if_exists<EnergyState>(reg);
        erase_ctx_if_exists<SongResults>(reg);
        erase_ctx_if_exists<EnergyDepletedDeath>(reg);
        erase_ctx_if_exists<EndChoiceRestart>(reg);
        erase_ctx_if_exists<EndChoiceLevelSelect>(reg);
        erase_ctx_if_exists<EndChoiceMainMenu>(reg);
        erase_ctx_if_exists<LevelSelectConfirmedTag>(reg);
        enter_phase<GamePhaseLevelSelectTag>(reg);
        return;
    }
    runtime_system_scratch_init(reg);
    runtime_system_scratch_reserve(reg, beat_map_total_count(beatmap));

    assign_or_emplace_ctx(reg, ScoreState{});
    assign_or_emplace_ctx(reg, ScoreDisplay{});
    assign_or_emplace_ctx(reg, CurrentSongHighScore{});

    // Wire high score: derive song_id from beatmap filename and set HighScoreSession::key_hash.
    // The key string is built once in a stack buffer (no heap); ensure_entry pre-registers
    // the entry so update_if_higher can later update by hash without the key string.
    // HighScoreEntry rows live in the registry (issue #1560), so the operations don't
    // need a HighScoreState ctx-singleton gate any more.
    {
        std::string stem = std::filesystem::path(beatmap_path).stem().string();
        static const std::string BEATMAP_SUFFIX = "_beatmap";
        if (stem.size() > BEATMAP_SUFFIX.size() &&
            stem.compare(stem.size() - BEATMAP_SUFFIX.size(), BEATMAP_SUFFIX.size(), BEATMAP_SUFFIX) == 0) {
            stem.erase(stem.size() - BEATMAP_SUFFIX.size());
        }
        char key_buf[HighScoreState::KEY_CAP]{};
        high_score::make_key_str(key_buf, HighScoreState::KEY_CAP,
                                 stem.c_str(), beatmap.difficulty.c_str());
        if (auto* session = reg.ctx().find<HighScoreSession>()) {
            session->key_hash = entt::hashed_string::value(static_cast<const char*>(key_buf));
        }
        high_score::ensure_entry(reg, key_buf);
    }

    // Init song state
    auto& song = assign_or_emplace_ctx(reg, SongState{});
    // Reset the BeatCursor row table (Fabian Principle 3 / issue #1545):
    // membership IS "at least one beat has been crossed", so erase any
    // carry-over from a prior session before the new session begins.
    erase_ctx_if_exists<BeatCursor>(reg);
    if (!beat_map_empty(beatmap)) {
        init_song_state(song, beatmap);
    } else {
        song.bpm = 120.0f;
        song_state_compute_derived(song);
    }
    song.playing = true;
    song.restart_music = true;

    // Load music (only if MusicContext exists — not in test mode)
    auto* music = reg.ctx().find<MusicContext>();
    if (music) {
        music->release();
    }
    if (music && !beatmap.song_path.empty()) {
        std::string exe_audio = util::join_app_dir(GetApplicationDirectory(), beatmap.song_path);
        const char* audio_paths[] = { exe_audio.c_str(), beatmap.song_path.c_str() };
        for (const char* path : audio_paths) {
            Music stream = LoadMusicStream(path);
            stream.looping = false;
            if (music_stream_is_playable(stream)) {
                music->stream  = stream;
                music->loaded  = true;
                music->started = false;
                music->paused = false;
                SetMusicVolume(music->stream, music->volume);
                TraceLog(LOG_INFO, "Loaded music: %s", path);
                break;
            }
            unload_music_stream_resources(stream);
        }
    }

    // Reset energy and results
    assign_or_emplace_ctx(reg, EnergyState{});
    {
        SongResults results{};
        // Per-(kind, shape) tables (#1202/#1204): result-counted notes are
        // the scoring-eligible kinds — ShapeGate + SplitPath. OnsetMarker
        // entries are non-scorable cues and don't contribute.
        results.total_notes = static_cast<int>(beat_map_required_count(beatmap));
        assign_or_emplace_ctx(reg, results);
    }
    // Clear any death-cause and end-screen-choice tags carried over from a
    // previous run; absence of all *Death and EndChoice* ctx tags is the
    // "no terminal cause / no choice recorded yet" state.
    erase_ctx_if_exists<EnergyDepletedDeath>(reg);
    erase_ctx_if_exists<EndChoiceRestart>(reg);
    erase_ctx_if_exists<EndChoiceLevelSelect>(reg);
    erase_ctx_if_exists<EndChoiceMainMenu>(reg);

    // Load stored high score for this song+difficulty into CurrentSongHighScore
    {
        auto& current = reg.ctx().get<CurrentSongHighScore>();
        const auto* session = reg.ctx().find<HighScoreSession>();
        current.value = session ? high_score::get_current_high_score(reg, *session) : 0;
    }

    // play_session owns when a player is spawned; player_entity owns how.
    create_player_entity(reg);
    // Transition game state
    enter_phase<GamePhasePlayingTag>(reg);
}
