#include "play_session.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/scoring.h"
#include "../entities/beat_map.h"
#include "../components/rhythm.h"
#include "../components/song_state.h"
#include "../components/energy_bar.h"
#include "../components/music.h"
#include "../components/high_score.h"
#include "high_score_system.h"
#include "../entities/settings.h"
#include "../util/rhythm_math.h"
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

namespace {

// Session setup owns when a player is spawned; player_entity owns how.
void spawn_session_player(entt::registry& reg) {
    create_player_entity(reg);
}

bool is_runtime_allowed_validation_error(const BeatMapError& error) {
    return error.message == "Different-shape gates must be >= 3 beats apart";
}

int count_result_notes(const BeatMap& beatmap) {
    int total = 0;
    for (const BeatEntry& beat : beatmap.beats) {
        if (beat.kind != ObstacleKind::OnsetMarker) {
            ++total;
        }
    }
    return total;
}

bool load_runtime_beat_map(const char* path,
                           BeatMap& out,
                           std::vector<BeatMapError>& errors,
                           const std::string& difficulty_key) {
    BeatMap candidate;
    if (!load_beat_map(path, candidate, errors, difficulty_key)) {
        return false;
    }

    std::vector<BeatMapError> validation_errors;
    if (validate_beat_map(candidate, validation_errors)) {
        out = std::move(candidate);
        return true;
    }

    bool only_allowed_errors = !validation_errors.empty();
    for (const BeatMapError& error : validation_errors) {
        if (!is_runtime_allowed_validation_error(error)) {
            only_allowed_errors = false;
        }
        errors.push_back(error);
    }

    if (!only_allowed_errors) {
        return false;
    }

    for (const BeatMapError& error : validation_errors) {
        TraceLog(LOG_WARNING, "Beatmap validation warning at beat %d: %s",
                 error.beat_index, error.message.c_str());
    }
    out = std::move(candidate);
    return true;
}

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

    reg.clear();

    spawn_game_camera(reg);
    spawn_ui_camera(reg);
    create_energy_bar_entity(reg);
    create_beat_map_entity(reg);
    if (restore_settings) {
        create_settings_entity(reg, settings, settings_persistence);
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
    std::string exe_path = std::string(GetApplicationDirectory()) + beatmap_path;
    const char* paths[] = { exe_path.c_str(), beatmap_path };

    bool loaded = false;
    for (const char* path : paths) {
        load_errors.clear();
        if (load_runtime_beat_map(path, beatmap, load_errors, difficulty_key)) {
            TraceLog(LOG_INFO, "Loaded beatmap: %s (%zu beats, difficulty=%s)",
                     path, beatmap.beats.size(), beatmap.difficulty.c_str());
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
        erase_ctx_if_exists<EnergyState>(reg);
        erase_ctx_if_exists<SongResults>(reg);
        erase_ctx_if_exists<EnergyDepletedDeath>(reg);
        erase_ctx_if_exists<EndChoiceRestart>(reg);
        erase_ctx_if_exists<EndChoiceLevelSelect>(reg);
        erase_ctx_if_exists<EndChoiceMainMenu>(reg);
        lss.confirmed = false;
        auto& gs = reg.ctx().get<GameState>();
        enter_phase(gs, GamePhase::LevelSelect);
        return;
    }
    runtime_system_scratch_init(reg);
    runtime_system_scratch_reserve(reg, beatmap.beats.size());

    assign_or_emplace_ctx(reg, ScoreState{});
    assign_or_emplace_ctx(reg, ScoreDisplay{});
    assign_or_emplace_ctx(reg, CurrentSongHighScore{});

    // Wire high score: derive song_id from beatmap filename and set HighScoreSession::key_hash.
    // The key string is built once in a stack buffer (no heap); ensure_entry pre-registers
    // the entry so update_if_higher can later update by hash without the key string.
    if (auto* hs = reg.ctx().find<HighScoreState>()) {
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
        high_score::ensure_entry(*hs, key_buf);
    }

    // Init song state
    auto& song = assign_or_emplace_ctx(reg, SongState{});
    if (!beatmap.beats.empty()) {
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
        std::string exe_audio = std::string(GetApplicationDirectory()) + beatmap.song_path;
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
        results.total_notes = count_result_notes(beatmap);
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
    if (auto* hs = reg.ctx().find<HighScoreState>()) {
        auto& current = reg.ctx().get<CurrentSongHighScore>();
        const auto* session = reg.ctx().find<HighScoreSession>();
        current.value = session ? high_score::get_current_high_score(*hs, *session) : 0;
    }

    spawn_session_player(reg);
    // Transition game state
    auto& gs = reg.ctx().get<GameState>();
    enter_phase(gs, GamePhase::Playing);
}
