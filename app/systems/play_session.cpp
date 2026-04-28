#include "play_session.h"
#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/obstacle_counter.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/scoring.h"
#include "../components/difficulty.h"
#include "../components/audio.h"
#include "../components/rhythm.h"
#include "../components/music.h"
#include "../components/high_score.h"
#include "../util/high_score_persistence.h"
#include "../components/rng.h"
#include "beat_map_loader.h"
#include "../components/input_events.h"
#include "../constants.h"
#include <filesystem>
#include <raylib.h>

void setup_play_session(entt::registry& reg) {
    reg.clear();

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
    reg.ctx().insert_or_assign(DifficultyConfig{
        .speed_multiplier     = 1.0f,
        .scroll_speed         = constants::BASE_SCROLL_SPEED,
        .spawn_interval       = constants::INITIAL_SPAWN_INT,
        .spawn_timer          = 1.0f,
        .elapsed              = 0.0f
    });
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
        sw.phase = WindowPhase::Idle;
        reg.emplace<ShapeWindow>(player, sw);
    }
    reg.emplace<Lane>(player);
    reg.emplace<VerticalState>(player);
    reg.emplace<Color>(player, Color{80, 180, 255, 255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<DrawLayer>(player, Layer::Game);

    // ── Spawn shape button UI entities ──────────────────────────
    {
        float btn_w       = constants::BUTTON_W_N  * constants::SCREEN_W;
        float btn_h       = constants::BUTTON_H_N  * constants::SCREEN_H;
        float btn_spacing = constants::BUTTON_SPACING_N * constants::SCREEN_W;
        float btn_y       = constants::BUTTON_Y_N  * constants::SCREEN_H;
        float btn_area_x  = (constants::SCREEN_W - 3.0f * btn_w - 2.0f * btn_spacing) / 2.0f;
        float btn_cy      = btn_y + btn_h / 2.0f;
        float btn_radius  = btn_w / 2.8f;
        float hit_radius  = btn_radius * 1.4f;

        Shape shapes[3] = { Shape::Circle, Shape::Square, Shape::Triangle };
        for (int i = 0; i < 3; ++i) {
            float btn_cx = btn_area_x + static_cast<float>(i) * (btn_w + btn_spacing) + btn_w / 2.0f;
            auto btn = reg.create();
            reg.emplace<ShapeButtonTag>(btn);
            reg.emplace<Position>(btn, btn_cx, btn_cy);
            reg.emplace<HitCircle>(btn, hit_radius);
            reg.emplace<ShapeButtonData>(btn, shapes[i]);
            reg.emplace<ActiveInPhase>(btn, GamePhaseBit::Playing);
        }
    }

    // Transition game state
    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::Playing;
    gs.phase_timer = 0.0f;
}
