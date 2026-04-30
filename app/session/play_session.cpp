#include "play_session.h"
#include "../components/game_state.h"
#include "../util/obstacle_counter.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/scoring.h"
#include "../audio/audio_types.h"
#include "../components/beat_map.h"
#include "../components/rhythm.h"
#include "../components/song_state.h"
#include "../audio/music_context.h"
#include "../components/high_score.h"
#include "../util/high_score_persistence.h"
#include "../components/rng.h"
#include "../util/beat_map_loader.h"
#include "../util/rhythm_math.h"
#include "../entities/camera_entity.h"
#include "../entities/player_entity.h"
#include "../constants.h"
#include <filesystem>
#include <raylib.h>

namespace {

// Session setup owns when a player is spawned; player_entity owns how.
void spawn_session_player(entt::registry& reg) {
    create_player_entity(reg);
}

void enter_playing_phase(GameState& gs) {
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::Playing;
    gs.phase_timer = 0.0f;
}

} // namespace

void setup_play_session(entt::registry& reg) {
    reg.clear();
    spawn_game_camera(reg);
    spawn_ui_camera(reg);

    // Initialise (first session) or reset (subsequent sessions) the obstacle counter.
    // reg.clear() above fires on_destroy<ObstacleTag> for any leftover entities, which
    // decrements the counter; reset to 0 here unconditionally so it is always clean.
    // Signals are wired once and survive reg.clear().
    if (!reg.ctx().find<ObstacleCounter>()) {
        reg.ctx().emplace<ObstacleCounter>();
        wire_obstacle_counter(reg);
    } else {
        reg.ctx().get<ObstacleCounter>().count = 0;
    }

    // Reset singletons
    reg.ctx().insert_or_assign(RNGState{});
    reg.ctx().insert_or_assign(ScoreState{});
    reg.ctx().insert_or_assign(AudioQueue{});

    // Load beatmap from level selection.
    // BeatMap is a context singleton (cold asset). It is reset here via move
    // assignment and populated by load_beat_map(). It remains immutable for
    // the duration of the play session. On song unload (next setup_play_session
    // call) the beat array is replaced in-place via another move assignment.
    auto& lss = reg.ctx().get<LevelSelectState>();
    auto& beatmap = reg.ctx().get<BeatMap>();
    beatmap = BeatMap{};
    const char* beatmap_path = LevelSelectState::LEVELS[lss.selected_level].beatmap_path;
    const char* difficulty_key = LevelSelectState::DIFFICULTY_KEYS[lss.selected_difficulty];

    std::vector<BeatMapError> load_errors;
    std::string exe_path = std::string(GetApplicationDirectory()) + beatmap_path;
    const char* paths[] = { exe_path.c_str(), beatmap_path };

    bool loaded = false;
    for (const char* path : paths) {
        load_errors.clear();
        if (load_beat_map(path, beatmap, load_errors, difficulty_key)) {
            TraceLog(LOG_INFO, "Loaded beatmap: %s (%zu beats, difficulty=%s)",
                     path, beatmap.beats.size(), beatmap.difficulty.c_str());
            loaded = true;
            break;
        }
    }
    if (!loaded) {
        TraceLog(LOG_WARNING, "Failed to load beatmap: %s", beatmap_path);
    }

    // Wire high score: derive song_id from beatmap filename and set current_key_hash.
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
        high_score::make_key_str(key_buf, HighScoreState::KEY_CAP, stem.c_str(), difficulty_key);
        high_score::ensure_entry(*hs, key_buf);
        hs->current_key_hash = entt::hashed_string::value(static_cast<const char*>(key_buf));
    }

    // Init song state
    auto& song = reg.ctx().get<SongState>();
    song = SongState{};
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
    if (music && music->loaded) {
        StopMusicStream(music->stream);
        UnloadMusicStream(music->stream);
        music->loaded = false;
        music->started = false;
    }
    if (music && !beatmap.song_path.empty()) {
        std::string exe_audio = std::string(GetApplicationDirectory()) + beatmap.song_path;
        const char* audio_paths[] = { exe_audio.c_str(), beatmap.song_path.c_str() };
        for (const char* path : audio_paths) {
            Music stream = LoadMusicStream(path);
            if (stream.frameCount > 0) {
                music->stream  = stream;
                music->loaded  = true;
                music->started = false;
                SetMusicVolume(music->stream, music->volume);
                TraceLog(LOG_INFO, "Loaded music: %s", path);
                break;
            }
        }
    }

    // Reset energy and results
    reg.ctx().insert_or_assign(EnergyState{});
    {
        SongResults results{};
        results.total_notes = static_cast<int>(beatmap.beats.size());
        reg.ctx().insert_or_assign(results);
    }
    reg.ctx().insert_or_assign(GameOverState{});

    // Load stored high score for this song+difficulty into ScoreState
    if (auto* hs = reg.ctx().find<HighScoreState>()) {
        auto& score = reg.ctx().get<ScoreState>();
        score.high_score = high_score::get_current_high_score(*hs);
    }

    spawn_session_player(reg);
    // Transition game state
    auto& gs = reg.ctx().get<GameState>();
    enter_playing_phase(gs);
}
