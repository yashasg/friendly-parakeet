#include <raylib.h>
#include <entt/entt.hpp>

#include "version.h"
#include "constants.h"
#include "components/input.h"
#include "components/game_state.h"
#include "components/scoring.h"
#include "components/burnout.h"
#include "components/difficulty.h"
#include "components/audio.h"
#include "components/rhythm.h"
#include "components/music.h"
#include "components/rendering.h"
#include "components/test_player.h"
#include "systems/all_systems.h"
#include "beat_map_loader.h"
#include "text_renderer.h"
#include "session_logger.h"

#include <string>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static constexpr float FIXED_DT  = 1.0f / 60.0f;
static constexpr float MAX_ACCUM = 0.1f;

// Recomputes the letterbox transform and stores it in the registry context so
// input_system can normalise raw window coordinates to virtual world space.
static void update_screen_transform(entt::registry& reg) {
    float win_w = static_cast<float>(GetScreenWidth());
    float win_h = static_cast<float>(GetScreenHeight());
    float scale = std::min(
        win_w / static_cast<float>(constants::SCREEN_W),
        win_h / static_cast<float>(constants::SCREEN_H));
    float dst_w = constants::SCREEN_W * scale;
    float dst_h = constants::SCREEN_H * scale;
    auto& st     = reg.ctx().get<ScreenTransform>();
    st.offset_x  = (win_w - dst_w) * 0.5f;
    st.offset_y  = (win_h - dst_h) * 0.5f;
    st.scale     = scale;
}

#ifdef __EMSCRIPTEN__
// ── Emscripten main-loop state ───────────────────────────────
// The browser event loop is non-blocking, so we extract the loop body
// into a free function and hand it to emscripten_set_main_loop.
struct LoopState {
    entt::registry* reg;
    float accumulator;
};
static LoopState g_loop;

static void update_draw_frame() {
    auto& reg = *g_loop.reg;
    float raw_dt = GetFrameTime();
    g_loop.accumulator += raw_dt;
    if (g_loop.accumulator > MAX_ACCUM) {
        g_loop.accumulator = MAX_ACCUM;
    }

    update_screen_transform(reg);
    input_system(reg, raw_dt);
    test_player_system(reg, raw_dt);

    while (g_loop.accumulator >= FIXED_DT) {
        gesture_system(reg, FIXED_DT);
        game_state_system(reg, FIXED_DT);
        song_playback_system(reg, FIXED_DT);
        beat_scheduler_system(reg, FIXED_DT);
        player_action_system(reg, FIXED_DT);
        shape_window_system(reg, FIXED_DT);
        player_movement_system(reg, FIXED_DT);
        difficulty_system(reg, FIXED_DT);
        obstacle_spawn_system(reg, FIXED_DT);
        scroll_system(reg, FIXED_DT);
        ring_zone_log_system(reg, FIXED_DT);
        burnout_system(reg, FIXED_DT);
        collision_system(reg, FIXED_DT);
        scoring_system(reg, FIXED_DT);
        hp_system(reg, FIXED_DT);
        lifetime_system(reg, FIXED_DT);
        particle_system(reg, FIXED_DT);
        cleanup_system(reg, FIXED_DT);
        g_loop.accumulator -= FIXED_DT;
    }

    float alpha = g_loop.accumulator / FIXED_DT;
    BeginDrawing();
        render_system(reg, alpha);
    EndDrawing();

    audio_system(reg);
}
#endif

int main(int argc, char* argv[]) {

    // ── Parse CLI args ───────────────────────────────────────
    TestPlayerSkill test_skill = TestPlayerSkill::Pro;
    bool test_player_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--test-player") == 0 && i + 1 < argc) {
            test_player_mode = true;
            ++i;
            if (std::strcmp(argv[i], "pro") == 0)       test_skill = TestPlayerSkill::Pro;
            else if (std::strcmp(argv[i], "good") == 0)  test_skill = TestPlayerSkill::Good;
            else if (std::strcmp(argv[i], "bad") == 0)   test_skill = TestPlayerSkill::Bad;
            else {
                std::fprintf(stderr, "Unknown skill: %s (use pro|good|bad)\n", argv[i]);
                return 1;
            }
        }
    }

    // ── RAYLIB INIT ──────────────────────────────────────────
    std::string window_title = std::string("SHAPESHIFTER v") + SHAPESHIFTER_VERSION;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(constants::SCREEN_W, constants::SCREEN_H, window_title.c_str());
    SetTargetFPS(60);
    InitAudioDevice();

    TraceLog(LOG_INFO, "SHAPESHIFTER v%s", SHAPESHIFTER_VERSION);

    // ── ENTT REGISTRY ────────────────────────────────────────
    entt::registry reg;

    // ── TEXT RENDERING (raylib fonts) ────────────────────────
    {
        auto& text_ctx = reg.ctx().emplace<TextContext>();

        // Try font paths: next to executable, CWD assets, then system fonts
        std::string exe_font = std::string(GetApplicationDirectory())
                             + "assets/fonts/LiberationMono-Regular.ttf";
        const char* font_paths[] = {
            exe_font.c_str(),
            "assets/fonts/LiberationMono-Regular.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        };

        bool font_loaded = false;
        for (const char* path : font_paths) {
            if (text_init(text_ctx, path)) {
                TraceLog(LOG_INFO, "Loaded font: %s", path);
                font_loaded = true;
                break;
            }
        }
        if (!font_loaded) {
            TraceLog(LOG_ERROR, "Could not load any TTF font");
        }
    }

    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<GestureResult>();
    reg.ctx().emplace<ShapeButtonEvent>();
    reg.ctx().emplace<GameState>(GameState{
        .phase          = GamePhase::Title,
        .previous_phase = GamePhase::Title,
        .phase_timer    = 0.0f,
        .transition_pending = false,
        .next_phase     = GamePhase::Title,
        .transition_alpha = 0.0f
    });
    reg.ctx().emplace<ScoreState>();
    reg.ctx().emplace<BurnoutState>();
    reg.ctx().emplace<DifficultyConfig>();
    reg.ctx().emplace<AudioQueue>();

    // Rhythm singletons (active even without a beat map loaded)
    reg.ctx().emplace<HPState>();
    reg.ctx().emplace<SongResults>();

    // ── 2-D world camera (identity; allows zoom/shake without touching game logic) ─
    // target=(0,0), offset=(0,0): world-space origin maps 1:1 to the virtual
    // 720×1280 render-texture.  Systems update these fields for camera effects.
    reg.ctx().emplace<Camera2D>(Camera2D{
        {0.0f, 0.0f},   // offset:   screen point that maps to target
        {0.0f, 0.0f},   // target:   world-space origin (0,0)
        0.0f,           // rotation: degrees
        1.0f            // zoom:     1.0 = no scale
    });
    reg.ctx().emplace<ScreenTransform>();  // updated each frame before input_system

    // ── Load beatmap from disk ────────────────────────────────
    {
        auto& beatmap = reg.ctx().emplace<BeatMap>();
        std::vector<BeatMapError> load_errors;

        std::string exe_beatmap = std::string(GetApplicationDirectory())
                                + "content/beatmaps/1_stomper_beatmap.json";
        const char* beatmap_paths[] = {
            exe_beatmap.c_str(),
            "content/beatmaps/1_stomper_beatmap.json",
        };

        bool loaded = false;
        for (const char* path : beatmap_paths) {
            load_errors.clear();
            if (load_beat_map(path, beatmap, load_errors, "medium")) {
                TraceLog(LOG_INFO, "Loaded beatmap: %s (%zu beats, difficulty=%s)",
                         path, beatmap.beats.size(), beatmap.difficulty.c_str());
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            TraceLog(LOG_WARNING, "No beatmap loaded — running in freeplay mode");
            for (const auto& err : load_errors) {
                TraceLog(LOG_WARNING, "  beatmap error: %s", err.message.c_str());
            }
        }

        if (loaded) {
            std::vector<BeatMapError> val_errors;
            if (!validate_beat_map(beatmap, val_errors)) {
                TraceLog(LOG_WARNING, "Beatmap validation warnings:");
                for (const auto& err : val_errors) {
                    TraceLog(LOG_WARNING, "  beat %d: %s", err.beat_index, err.message.c_str());
                }
            }
        }

        auto& song = reg.ctx().emplace<SongState>();
        if (!beatmap.beats.empty()) {
            init_song_state(song, beatmap);
        } else {
            song.bpm = 120.0f;
            song_state_compute_derived(song);
        }
    }

    // ── Load music stream ─────────────────────────────────────
    {
        auto& music = reg.ctx().emplace<MusicContext>();
        auto* beatmap = reg.ctx().find<BeatMap>();

        if (beatmap && !beatmap->song_path.empty()) {
            std::string exe_audio = std::string(GetApplicationDirectory())
                                  + beatmap->song_path;
            const char* audio_paths[] = {
                exe_audio.c_str(),
                beatmap->song_path.c_str(),
            };

            for (const char* path : audio_paths) {
                Music stream = LoadMusicStream(path);
                if (stream.frameCount > 0) {
                    music.stream  = stream;
                    music.loaded  = true;
                    music.started = false;
                    SetMusicVolume(music.stream, music.volume);
                    TraceLog(LOG_INFO, "Loaded music: %s (%u frames)",
                             path, stream.frameCount);
                    break;
                }
            }

            if (!music.loaded) {
                TraceLog(LOG_WARNING, "Could not load music: %s",
                         beatmap->song_path.c_str());
            }
        }
    }

    // ── Test Player Setup ──────────────────────────────────────
    if (test_player_mode) {
        auto& tp_state = reg.ctx().emplace<TestPlayerState>();
        tp_state.skill  = test_skill;
        tp_state.active = true;
        tp_state.rng.seed(static_cast<unsigned>(std::time(nullptr)));

        const char* skill_names[] = { "pro", "good", "bad" };
        TraceLog(LOG_INFO, "TEST PLAYER: skill=%s vision=%.0f react=%.3f-%.3f",
                 skill_names[static_cast<int>(test_skill)],
                 tp_state.config().vision_range,
                 tp_state.config().reaction_min,
                 tp_state.config().reaction_max);

        // Session logger
        auto& slog = reg.ctx().emplace<SessionLog>();
        std::time_t now = std::time(nullptr);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &now);
#else
        localtime_r(&now, &tm);
#endif
        char log_filename[256];
        std::snprintf(log_filename, sizeof(log_filename),
            "%ssession_%s_%04d%02d%02d_%02d%02d%02d.log",
            GetApplicationDirectory(),
            skill_names[static_cast<int>(test_skill)],
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
        session_log_open(slog, log_filename);
        TraceLog(LOG_INFO, "SESSION LOG: %s", log_filename);

        // Register EnTT signals for game-side logging
        reg.on_construct<ObstacleTag>().connect<&session_log_on_obstacle_spawn>();
        reg.on_construct<ScoredTag>().connect<&session_log_on_scored>();
    }

    // ── Virtual-resolution render target ─────────────────────
    // The game logic and rendering all target 720×1280.  We draw into
    // this texture every frame and then blit it, letter-boxed, to the
    // actual window — which may be a different (smaller) size.
    RenderTexture2D target = LoadRenderTexture(
        constants::SCREEN_W, constants::SCREEN_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    // ── MAIN LOOP ────────────────────────────────────────────
#ifdef __EMSCRIPTEN__
    g_loop = { &reg, 0.0f };
    emscripten_set_main_loop(update_draw_frame, 0, 1);
#else
    float accumulator = 0.0f;
    while (!WindowShouldClose()) {

        // Delta time
        float raw_dt = GetFrameTime();
        accumulator += raw_dt;
        if (accumulator > MAX_ACCUM) {
            accumulator = MAX_ACCUM;
        }

        // Phase 0: Resolve letterbox scale for this frame, then poll input
        update_screen_transform(reg);
        input_system(reg, raw_dt);
        if (reg.ctx().get<InputState>().quit_requested) break;

        // Phase 0.5: Test player AI (injects into InputState)
        test_player_system(reg, raw_dt);

        // Fixed timestep loop — all systems self-guard on GamePhase
        while (accumulator >= FIXED_DT) {
            gesture_system(reg, FIXED_DT);
            game_state_system(reg, FIXED_DT);
            song_playback_system(reg, FIXED_DT);
            beat_scheduler_system(reg, FIXED_DT);
            player_action_system(reg, FIXED_DT);
            shape_window_system(reg, FIXED_DT);
            player_movement_system(reg, FIXED_DT);
            difficulty_system(reg, FIXED_DT);
            obstacle_spawn_system(reg, FIXED_DT);
            scroll_system(reg, FIXED_DT);
            ring_zone_log_system(reg, FIXED_DT);
            burnout_system(reg, FIXED_DT);
            collision_system(reg, FIXED_DT);
            scoring_system(reg, FIXED_DT);
            hp_system(reg, FIXED_DT);
            lifetime_system(reg, FIXED_DT);
            particle_system(reg, FIXED_DT);
            cleanup_system(reg, FIXED_DT);
            accumulator -= FIXED_DT;
        }

        // Render to virtual-resolution texture
        float alpha = accumulator / FIXED_DT;
        BeginTextureMode(target);
            render_system(reg, alpha);
        EndTextureMode();

        // Blit virtual framebuffer to window (letter-boxed)
        // Reuse the ScreenTransform already computed this frame — avoids two
        // code paths diverging if the scale/offset formula ever changes.
        const auto& st = reg.ctx().get<ScreenTransform>();
        float dst_w  = constants::SCREEN_W * st.scale;
        float dst_h  = constants::SCREEN_H * st.scale;

        BeginDrawing();
            ClearBackground(BLACK);
            // Source rect: full texture, flipped vertically (OpenGL convention)
            Rectangle src = { 0, 0,
                static_cast<float>(constants::SCREEN_W),
                -static_cast<float>(constants::SCREEN_H) };
            Rectangle dst = { st.offset_x, st.offset_y, dst_w, dst_h };
            DrawTexturePro(target.texture, src, dst, {0, 0}, 0.0f, WHITE);
        EndDrawing();

        // Audio
        audio_system(reg);
    }
#endif

    // ── SHUTDOWN ─────────────────────────────────────────────
    {
        auto* slog = reg.ctx().find<SessionLog>();
        if (slog) session_log_close(*slog);
    }
    {
        auto* music = reg.ctx().find<MusicContext>();
        if (music && music->loaded) {
            StopMusicStream(music->stream);
            UnloadMusicStream(music->stream);
            music->loaded = false;
        }
    }
    CloseAudioDevice();
    UnloadRenderTexture(target);
    text_shutdown(reg.ctx().get<TextContext>());
    CloseWindow();
    return 0;
}
