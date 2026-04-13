#include "play_session.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/scoring.h"
#include "../components/burnout.h"
#include "../components/difficulty.h"
#include "../components/audio.h"
#include "../components/rhythm.h"
#include "../components/music.h"
#include "../beat_map_loader.h"
#include "../constants.h"
#include <raylib.h>

void setup_play_session(entt::registry& reg) {
    reg.clear();

    // Reset singletons
    reg.ctx().insert_or_assign(ScoreState{});
    reg.ctx().insert_or_assign(DifficultyConfig{
        .speed_multiplier     = 1.0f,
        .scroll_speed         = constants::BASE_SCROLL_SPEED,
        .spawn_interval       = constants::INITIAL_SPAWN_INT,
        .spawn_timer          = 1.0f,
        .burnout_window_scale = 1.0f,
        .elapsed              = 0.0f
    });
    reg.ctx().insert_or_assign(BurnoutState{});
    reg.ctx().insert_or_assign(AudioQueue{});

    // Load beatmap from level selection
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
    if (music) {
        if (music->loaded) {
            StopMusicStream(music->stream);
            UnloadMusicStream(music->stream);
            music->loaded = false;
            music->started = false;
        }
        if (!beatmap.song_path.empty()) {
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
    }

    // Reset HP and results
    auto& hp = reg.ctx().get<HPState>();
    hp.current = hp.max_hp;
    reg.ctx().insert_or_assign(SongResults{});

    // Create player entity
    auto player = reg.create();
    reg.emplace<PlayerTag>(player);
    reg.emplace<Position>(player, constants::LANE_X[1], constants::PLAYER_Y);
    {
        PlayerShape ps;
        ps.current  = Shape::Hexagon;
        ps.previous = Shape::Hexagon;
        reg.emplace<PlayerShape>(player, ps);
    }
    {
        ShapeWindow sw;
        sw.target_shape = Shape::Hexagon;
        sw.phase_raw    = static_cast<uint8_t>(WindowPhase::Idle);
        reg.emplace<ShapeWindow>(player, sw);
    }
    reg.emplace<Lane>(player);
    reg.emplace<VerticalState>(player);
    reg.emplace<DrawColor>(player, uint8_t{80}, uint8_t{180}, uint8_t{255}, uint8_t{255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<DrawLayer>(player, Layer::Game);

    // Transition game state
    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::Playing;
    gs.phase_timer = 0.0f;
}
