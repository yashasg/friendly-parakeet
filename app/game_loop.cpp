#include "game_loop.h"
#include "version.h"
#include "constants.h"
#include "components/input.h"
#include "components/input_events.h"
#include "components/game_state.h"
#include "components/scoring.h"
#include "components/burnout.h"
#include "components/difficulty.h"
#include "components/audio.h"
#include "components/haptics.h"
#include "components/settings.h"
#include "components/rhythm.h"
#include "components/music.h"
#include "components/session_log.h"
#include "components/rendering.h"
#include "components/camera.h"
#include "components/test_player.h"
#include "components/obstacle.h"
#include "systems/all_systems.h"
#include "systems/text_renderer.h"
#include "systems/session_logger.h"
#include "systems/camera_system.h"
#include "systems/ui_loader.h"
#include "systems/ui_button_spawner.h"
#include "gameobjects/shape_obstacle.h"
#include "platform_display.h"
#include "util/settings_persistence.h"

#include <raylib.h>
#include <algorithm>
#include <string>

static constexpr float FIXED_DT  = 1.0f / 60.0f;
static constexpr float MAX_ACCUM = 0.1f;

// ── Init ────────────────────────────────────────────────────────────────────

void game_loop_init(entt::registry& reg,
                    bool test_player_mode,
                    TestPlayerSkill test_skill,
                    const char* difficulty) {
    // Platform: window + audio
    std::string window_title = std::string("SHAPESHIFTER v") + SHAPESHIFTER_VERSION;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
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

    // Text rendering
    {
        auto& text_ctx = reg.ctx().emplace<TextContext>();
        text_init_default(text_ctx);
    }

    // Core singletons
    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<EventQueue>();
    reg.ctx().emplace<GameState>(GameState{
        .phase = GamePhase::Title, .previous_phase = GamePhase::Title,
        .phase_timer = 0.0f, .transition_pending = false,
        .next_phase = GamePhase::Title, .transition_alpha = 0.0f
    });
    reg.ctx().emplace<ScoreState>();
    reg.ctx().emplace<BurnoutState>();
    reg.ctx().emplace<DifficultyConfig>();
    reg.ctx().emplace<AudioQueue>();
    reg.ctx().emplace<HapticQueue>();
    reg.ctx().emplace<LevelSelectState>();
    reg.ctx().emplace<EnergyState>();
    reg.ctx().emplace<SongResults>();

    // Settings — load from disk; default values apply when no file exists.
    {
        SettingsState settings;
        settings::load_settings(settings, settings::get_settings_file_path());
        reg.ctx().emplace<SettingsState>(settings);
    }

    // Cameras + render targets + GPU meshes
    camera::init(reg);

    // MeshChild auto-cleanup: destroy children when parent obstacle is destroyed
    reg.on_destroy<ObstacleTag>().connect<&on_obstacle_destroy>();

    // UI + beatmap + music
    reg.ctx().emplace<UIState>(load_ui());
    reg.ctx().emplace<BeatMap>();
    reg.ctx().emplace<SongState>();
    reg.ctx().emplace<MusicContext>();
    spawn_title_buttons(reg);

    // Test player (optional)
    if (test_player_mode) {
        test_player_init(reg, test_skill, difficulty);
    }
}

// ── Run ─────────────────────────────────────────────────────────────────────

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
    popup_display_system(reg, dt);
    ui_navigation_system(reg, dt);
}

// One frame: input → fixed timestep → render → blit → audio.
// Not in header — called by game_loop_run and platform_run_loop (Emscripten).
void game_loop_frame(entt::registry& reg, float& accumulator) {
    float raw_dt = GetFrameTime();
    accumulator += raw_dt;
    if (accumulator > MAX_ACCUM) accumulator = MAX_ACCUM;

    input_system(reg, raw_dt);
    hit_test_system(reg);
    test_player_system(reg, raw_dt);

    while (accumulator >= FIXED_DT) {
        tick_fixed_systems(reg, FIXED_DT);
        accumulator -= FIXED_DT;
    }

    // Camera runs after gameplay systems so transforms reflect current frame
    game_camera_system(reg, raw_dt);
    ui_camera_system(reg, raw_dt);

    auto& targets = reg.ctx().get<RenderTargets>();

    // Pass 1: World (3D)
    BeginTextureMode(targets.world);
        game_render_system(reg, 0.0f);
    EndTextureMode();

    // Pass 2: UI (2D)
    BeginTextureMode(targets.ui);
        ui_render_system(reg, 0.0f);
    EndTextureMode();

    // Composite: blit world, then UI (alpha-blended) onto window
    const auto& st = reg.ctx().get<ScreenTransform>();
    float dst_w = constants::SCREEN_W * st.scale;
    float dst_h = constants::SCREEN_H * st.scale;
    Rectangle src = { 0, 0,
        static_cast<float>(constants::SCREEN_W),
        -static_cast<float>(constants::SCREEN_H) };
    Rectangle dst = { st.offset_x, st.offset_y, dst_w, dst_h };

    platform_pre_blit();
    BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(targets.world.texture, src, dst, {0, 0}, 0.0f, WHITE);
        DrawTexturePro(targets.ui.texture, src, dst, {0, 0}, 0.0f, WHITE);
    EndDrawing();

    audio_system(reg);
    haptic_system(reg);

    auto* session_log = reg.ctx().find<SessionLog>();
    if (session_log) session_log_flush(*session_log);
}

void game_loop_run(entt::registry& reg) {
#ifdef __EMSCRIPTEN__
    platform_run_loop(reg);
#else
    float accumulator = 0.0f;
    while (!WindowShouldClose()) {
        game_loop_frame(reg, accumulator);
        if (reg.ctx().get<InputState>().quit_requested) break;
    }
#endif
}

// ── Shutdown ────────────────────────────────────────────────────────────────

void game_loop_shutdown(entt::registry& reg) {
    // Disconnect all destroy/construct listeners before clearing entities
    reg.on_destroy<ObstacleTag>().disconnect<&on_obstacle_destroy>();
    reg.on_construct<ObstacleTag>().disconnect<&session_log_on_obstacle_spawn>();
    reg.on_construct<ScoredTag>().disconnect<&session_log_on_scored>();

    // Destroy all entities while GPU context is still alive
    reg.clear();

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
    camera::shutdown(reg);
    text_shutdown(reg.ctx().get<TextContext>());
    CloseAudioDevice();
    CloseWindow();
}
