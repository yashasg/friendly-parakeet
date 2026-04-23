#include <raylib.h>
#include <entt/entt.hpp>

#include "version.h"
#include "constants.h"
#include "components/input.h"
#include "components/input_events.h"
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
#include "systems/beat_map_loader.h"
#include "systems/text_renderer.h"
#include "systems/session_logger.h"
#include "systems/camera_system.h"
#include "systems/ui_loader.h"
#include "systems/ui_button_spawner.h"

#include <string>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <rlgl.h>
#endif

static constexpr float FIXED_DT  = 1.0f / 60.0f;
static constexpr float MAX_ACCUM = 0.1f;

// Runs all fixed-timestep systems once. Called from both the native
// and Emscripten main loops so the system list is defined in one place.
static void tick_fixed_systems(entt::registry& reg, float dt) {
    game_state_system(reg, dt);
    level_select_system(reg, dt);
    song_playback_system(reg, dt);
    beat_scheduler_system(reg, dt);
    player_input_system(reg, dt);
    shape_window_system(reg, dt);
    player_movement_system(reg, dt);
    difficulty_system(reg, dt);
    obstacle_spawn_system(reg, dt);
    scroll_system(reg, dt);
    ring_zone_log_system(reg, dt);
    burnout_system(reg, dt);
    collision_system(reg, dt);
    scoring_system(reg, dt);
    energy_system(reg, dt);
    lifetime_system(reg, dt);
    particle_system(reg, dt);
    cleanup_system(reg, dt);
}

// Recomputes the letterbox transform and stores it in the registry context so
// input_system can normalise raw window coordinates to virtual world space.
static void update_screen_transform(entt::registry& reg) {
    float win_w, win_h;
#ifdef __EMSCRIPTEN__
    // On web, the canvas CSS display size may differ from the drawing buffer.
    // Read the CSS size directly and resize the buffer to match, so that
    // mouse/touch coordinates (which are in CSS pixels) align with the
    // framebuffer pixels — eliminating any hidden CSS scaling layer.
    double css_w = 0.0, css_h = 0.0;
    emscripten_get_element_css_size("#canvas", &css_w, &css_h);
    win_w = static_cast<float>(std::max(1.0, css_w));
    win_h = static_cast<float>(std::max(1.0, css_h));

    int cur_buf_w = 0, cur_buf_h = 0;
    emscripten_get_canvas_element_size("#canvas", &cur_buf_w, &cur_buf_h);
    int target_w = static_cast<int>(win_w);
    int target_h = static_cast<int>(win_h);
    if (cur_buf_w != target_w || cur_buf_h != target_h) {
        emscripten_set_canvas_element_size("#canvas", target_w, target_h);
    }
#else
    win_w = static_cast<float>(GetScreenWidth());
    win_h = static_cast<float>(GetScreenHeight());
#endif
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
    RenderTexture2D target;
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
    hit_test_system(reg);
    test_player_system(reg, raw_dt);

    while (g_loop.accumulator >= FIXED_DT) {
        tick_fixed_systems(reg, FIXED_DT);
        g_loop.accumulator -= FIXED_DT;
    }

    // Render to virtual-resolution texture, then blit letterboxed
    float alpha = g_loop.accumulator / FIXED_DT;
    BeginTextureMode(g_loop.target);
        render_system(reg, alpha);
    EndTextureMode();

    const auto& st = reg.ctx().get<ScreenTransform>();
    float dst_w = constants::SCREEN_W * st.scale;
    float dst_h = constants::SCREEN_H * st.scale;

    BeginDrawing();
        // After canvas buffer resize, raylib's viewport may not yet reflect the
        // new dimensions.  Explicitly set viewport + ortho to match the buffer
        // so that ClearBackground and DrawTexturePro use the correct space.
        {
            int buf_w = 0, buf_h = 0;
            emscripten_get_canvas_element_size("#canvas", &buf_w, &buf_h);
            rlViewport(0, 0, buf_w, buf_h);
            rlMatrixMode(RL_PROJECTION);
            rlLoadIdentity();
            rlOrtho(0.0, static_cast<double>(buf_w),
                    static_cast<double>(buf_h), 0.0, 0.0, 1.0);
            rlMatrixMode(RL_MODELVIEW);
            rlLoadIdentity();
        }
        ClearBackground(BLACK);
        Rectangle src = { 0, 0,
            static_cast<float>(constants::SCREEN_W),
            -static_cast<float>(constants::SCREEN_H) };
        Rectangle dst = { st.offset_x, st.offset_y, dst_w, dst_h };
        DrawTexturePro(g_loop.target.texture, src, dst, {0, 0}, 0.0f, WHITE);
    EndDrawing();

    audio_system(reg);

    // Flush session log buffer (if active)
    auto* session_log = reg.ctx().find<SessionLog>();
    if (session_log) session_log_flush(*session_log);
}
#endif

int main(int argc, char* argv[]) {

    // ── Parse CLI args ───────────────────────────────────────
    TestPlayerSkill test_skill = TestPlayerSkill::Pro;
    bool test_player_mode = false;
    const char* difficulty = "medium";
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
        if (std::strcmp(argv[i], "--difficulty") == 0 && i + 1 < argc) {
            ++i;
            if (std::strcmp(argv[i], "easy") == 0 ||
                std::strcmp(argv[i], "medium") == 0 ||
                std::strcmp(argv[i], "hard") == 0) {
                difficulty = argv[i];
            } else {
                std::fprintf(stderr, "Unknown difficulty: %s (use easy|medium|hard)\n", argv[i]);
                return 1;
            }
        }
    }

    // ── RAYLIB INIT ──────────────────────────────────────────
    std::string window_title = std::string("SHAPESHIFTER v") + SHAPESHIFTER_VERSION;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    // Start with a window that fits the monitor, maintaining 9:16 aspect ratio
    InitWindow(constants::SCREEN_W, constants::SCREEN_H, window_title.c_str());
    {
        int mon = GetCurrentMonitor();
        int mon_h = GetMonitorHeight(mon);
        int win_h = static_cast<int>(mon_h * 0.85f);
        int win_w = win_h * constants::SCREEN_W / constants::SCREEN_H;
        SetWindowSize(win_w, win_h);
        SetWindowPosition(
            (GetMonitorWidth(mon) - win_w) / 2,
            (mon_h - win_h) / 2);
    }
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
    reg.ctx().emplace<EventQueue>();
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
    reg.ctx().emplace<LevelSelectState>();

    // Rhythm singletons (active even without a beat map loaded)
    reg.ctx().emplace<EnergyState>();
    reg.ctx().emplace<SongResults>();

    // ── 3-D perspective camera ─────────────────────────────────────────────────
    // Maps 2D game positions (x, y) to 3D world space as (x, 0, y) on the
    // XZ plane.  Camera3D perspective projection naturally creates lane
    // convergence — no manual trapezoid warping needed.
    {
        Camera3D cam3d = {};
        cam3d.position   = {to_world(360.0f), to_world(460.0f), to_world(2390.0f)};
        cam3d.target     = {to_world(360.0f), to_world(0.0f),   to_world(400.0f)};
        cam3d.up         = {0.0f, 1.0f, 0.0f};
        cam3d.fovy       = 45.0f;
        cam3d.projection = CAMERA_PERSPECTIVE;
        reg.ctx().emplace<Camera3D>(cam3d);
    }
    reg.ctx().emplace<ScreenTransform>();  // updated each frame before input_system
    reg.ctx().emplace<camera::ShapeMeshes>(camera::build_shape_meshes());

    // ── UI layouts (data-driven screens from JSON) ──────────────
    reg.ctx().emplace<UIState>(load_ui());

    // ── Beatmap + song singletons (populated by play_session on level start) ─
    reg.ctx().emplace<BeatMap>();
    reg.ctx().emplace<SongState>();

    // ── Spawn initial UI button entities for the Title screen ─
    spawn_title_buttons(reg);

    // ── Music context (stream loaded per-level in play_session) ─
    reg.ctx().emplace<MusicContext>();

    // ── Test Player Setup ──────────────────────────────────────
    if (test_player_mode) {
        // Auto-select level 1 (2_drama) at chosen difficulty for test player
        auto& lss = reg.ctx().get<LevelSelectState>();
        lss.selected_level = 1;
        for (int d = 0; d < 3; ++d) {
            if (std::strcmp(LevelSelectState::DIFFICULTY_KEYS[d], difficulty) == 0) {
                lss.selected_difficulty = d;
                break;
            }
        }

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
        localtime_r(&now, &tm);
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
    g_loop = { &reg, 0.0f, target };
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
        hit_test_system(reg);
        if (reg.ctx().get<InputState>().quit_requested) break;

        // Phase 0.5: Test player AI (injects into InputState)
        test_player_system(reg, raw_dt);

        // Fixed timestep loop — all systems self-guard on GamePhase
        while (accumulator >= FIXED_DT) {
            tick_fixed_systems(reg, FIXED_DT);
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

        // Flush session log buffer (if active)
        auto* session_log = reg.ctx().find<SessionLog>();
        if (session_log) session_log_flush(*session_log);
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
    camera::unload_shape_meshes(reg.ctx().get<camera::ShapeMeshes>());
    text_shutdown(reg.ctx().get<TextContext>());
    CloseWindow();
    return 0;
}
