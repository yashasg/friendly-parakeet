#include "play_session.h"
#include "../components/game_state.h"
#include "../util/obstacle_counter.h"
#include "../components/scoring.h"
#include "../components/gameplay_intents.h"
#include "../components/audio.h"
#include "../components/beat_map.h"
#include "../components/song_state.h"
#include "../components/high_score.h"
#include "../systems/audio_runtime.h"
#include "../util/high_score_persistence.h"
#include "../util/rhythm_math.h"
#include "../components/registry_context.h"
#include "../util/beat_map_loader.h"
#include "../entities/player_entity.h"
#include "../entities/camera_entity.h"
#include <SDL.h>
#include <filesystem>
#include <utility>
#include <vector>

namespace {

template <typename Loader>
bool load_from_any_resource_path(const std::string& relative_path, Loader&& loader) {
    if (relative_path.empty()) {
        return false;
    }
    const std::string app_relative_path =
        (std::filesystem::current_path() / relative_path).string();
    if (loader(app_relative_path)) {
        return true;
    }
    return app_relative_path != relative_path && loader(relative_path);
}

std::string song_id_from_beatmap_path(const char* beatmap_path) {
    std::string stem = std::filesystem::path(beatmap_path ? beatmap_path : "").stem().string();
    static constexpr const char* BEATMAP_SUFFIX = "_beatmap";
    static constexpr std::size_t BEATMAP_SUFFIX_LEN = 8;
    if (stem.size() > BEATMAP_SUFFIX_LEN &&
        stem.compare(stem.size() - BEATMAP_SUFFIX_LEN, BEATMAP_SUFFIX_LEN, BEATMAP_SUFFIX) == 0) {
        stem.erase(stem.size() - BEATMAP_SUFFIX_LEN);
    }
    return stem;
}

std::string selected_song_id(const BeatMap& beatmap, const char* beatmap_path) {
    if (!beatmap.song_id.empty()) {
        return beatmap.song_id;
    }
    return song_id_from_beatmap_path(beatmap_path);
}

void reset_play_runtime_context(entt::registry& reg, int total_notes) {
    registry_ctx_insert_defaults<ScoreState,
                                 AudioQueue,
                                 EnergyState,
                                 GameOverState,
                                 PendingEnergyEffects,
                                 ScorePopupRequestQueue>(reg);

    SongResults results{};
    results.total_notes = total_notes > 0 ? total_notes : 0;
    registry_ctx_insert_or_assign(reg, std::move(results));
}

void clear_session_entities_except(entt::registry& reg, entt::entity keep) {
    auto& entities = reg.storage<entt::entity>();
    std::vector<entt::entity> stale_entities;
    stale_entities.reserve(entities.size());
    for (const entt::entity entity : entities) {
        if (entity != keep) {
            stale_entities.push_back(entity);
        }
    }
    if (!stale_entities.empty()) {
        reg.destroy(stale_entities.begin(), stale_entities.end());
    }
}

}  // namespace

void setup_play_session(entt::registry& reg) {
    const entt::entity camera_entity = try_game_camera_entity(reg);
    clear_session_entities_except(reg, camera_entity);
    ensure_game_camera(reg);

    // Initialise (first session) or reset (subsequent sessions) the obstacle counter.
    // Session entity teardown above destroys leftover obstacles and decrements the
    // counter; reset to 0 here unconditionally so it is always clean.
    // Signals are wired once and survive across session resets.
    auto& obstacle_counter = registry_ctx_get_or_emplace<ObstacleCounter>(reg);
    wire_obstacle_counter(reg);
    obstacle_counter.count = 0;

    // Load beatmap from level selection.
    // BeatMap is a context singleton (cold asset). It is reset here via move
    // assignment and populated by load_beat_map(). It remains immutable for
    // the duration of the play session. On song unload (next setup_play_session
    // call) the beat array is replaced in-place via another move assignment.
    auto& lss = registry_ctx_get<LevelSelectState>(reg);
    auto& beatmap = registry_ctx_get<BeatMap>(reg);
    beatmap = BeatMap{};
    const LevelInfo& level_info = selected_level_info(lss);
    const char* beatmap_path = level_info.beatmap_path;
    const char* difficulty_key = selected_difficulty_key(lss);

    const bool loaded = load_from_any_resource_path(beatmap_path, [&](const std::string& path) {
        std::vector<BeatMapError> load_errors;
        if (!load_beat_map(path.c_str(), beatmap, load_errors, difficulty_key)) {
            return false;
        }
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loaded beatmap: %s (%zu beats, difficulty=%s)",
                    path.c_str(), beatmap.beats.size(), beatmap.difficulty.c_str());
        return true;
    });
    if (!loaded) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load beatmap: %s", beatmap_path);
    }

    // Wire high score: derive song_id from beatmap filename and set current_key_hash.
    // The key string is built once in a stack buffer (no heap); ensure_entry pre-registers
    // the entry so update_if_higher can later update by hash without the key string.
    registry_ctx_if<HighScoreState>(reg, [&](HighScoreState& hs) {
        const std::string song_id = selected_song_id(beatmap, beatmap_path);
        char key_buf[HighScoreState::KEY_CAP]{};
        ::high_score::make_key_str(key_buf, HighScoreState::KEY_CAP, song_id.c_str(), difficulty_key);
        ::high_score::ensure_entry(hs, key_buf);
        hs.current_key_hash = ::high_score::make_key_hash(song_id.c_str(), difficulty_key);
    });

    // Init song state
    auto& song = registry_ctx_get<SongState>(reg);
    song = SongState{};
    if (!beatmap.beats.empty()) {
        init_song_state(song, beatmap);
    } else {
        song.bpm = 120.0f;
        song_state_compute_derived(song);
    }
    song.playing = true;
    song.restart_music = true;

    reset_play_runtime_context(reg, static_cast<int>(beatmap.beats.size()));

    // Load music only when MusicContext is present (tests may intentionally omit it).
    if (auto* music = registry_ctx_find<MusicContext>(reg)) {
        if (music->loaded) {
            audio_runtime::unload_music_stream(*music);
        }
        if (!beatmap.song_path.empty()) {
            const bool music_loaded = load_from_any_resource_path(beatmap.song_path, [&](const std::string& path) {
                if (!audio_runtime::load_music_stream(*music, path.c_str(), false)) {
                    return false;
                }
                audio_runtime::set_music_volume(*music, music->volume);
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loaded music: %s", path.c_str());
                return true;
            });
            if (!music_loaded) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load music: %s", beatmap.song_path.c_str());
            }
        }
    }

    // Load stored high score for this song+difficulty into ScoreState
    registry_ctx_if<HighScoreState>(reg, [&](const HighScoreState& hs) {
        auto& score = registry_ctx_get<ScoreState>(reg);
        score.high_score = ::high_score::get_current_high_score(hs);
    });

    create_player_entity(reg);
    // Transition game state
    enter_phase(registry_ctx_get<GameState>(reg), GamePhase::Playing);
}
