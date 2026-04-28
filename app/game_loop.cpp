#include "game_loop.h"
#include "version.h"
#include "constants.h"
#include "components/input.h"
#include "components/input_events.h"
#include "components/game_state.h"
#include "components/scoring.h"
#include "audio/audio_types.h"
#include "audio/sfx_bank.h"
#include "components/haptics.h"
#include "util/settings.h"
#include "components/rhythm.h"
#include "audio/music_context.h"
#include "components/rendering.h"
#include "components/test_player.h"
#include "components/obstacle.h"
#include "components/rng.h"
#include "systems/all_systems.h"
#include "session/test_player_session.h"
#include "input/input_routing.h"
#include "ui/text_renderer.h"
#include "util/session_logger.h"
#include "systems/camera_system.h"
#include "ui/ui_loader.h"
#include "ui/ui_button_spawner.h"
#include "gameobjects/shape_obstacle.h"
#include "platform_display.h"
#include "util/settings_persistence.h"
#include "components/high_score.h"
#include "util/high_score_persistence.h"

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
    sfx_bank_init(reg);
    sfx_playback_backend_init(reg);
    TraceLog(LOG_INFO, "SHAPESHIFTER v%s", SHAPESHIFTER_VERSION);

    // Text rendering
    {
        auto& text_ctx = reg.ctx().emplace<TextContext>();
        text_init_default(text_ctx);
    }

    // Core singletons
    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<entt::dispatcher>();
    wire_input_dispatcher(reg);
    reg.ctx().emplace<GameState>(GameState{
        .phase = GamePhase::Title, .previous_phase = GamePhase::Title,
        .phase_timer = 0.0f, .transition_pending = false,
        .next_phase = GamePhase::Title, .transition_alpha = 0.0f
    });
    reg.ctx().emplace<ScoreState>();
    reg.ctx().emplace<AudioQueue>();
    reg.ctx().emplace<HapticQueue>();
    reg.ctx().emplace<LevelSelectState>();
    reg.ctx().emplace<EnergyState>();
    reg.ctx().emplace<GameOverState>();
    reg.ctx().emplace<SongResults>();
    reg.ctx().emplace<RNGState>();

    // Settings — load from disk; default values apply when no file exists.
    {
        SettingsState settings;
        settings::load_settings(settings, settings::get_settings_file_path());
        reg.ctx().emplace<SettingsState>(settings);
    }

    // High scores — load from disk; default values apply when no file exists.
    {
        const auto high_scores_path = high_score::get_high_scores_file_path();
        HighScoreState hs;
        high_score::load_high_scores(hs, high_scores_path);
        reg.ctx().emplace<HighScoreState>(hs);
        reg.ctx().emplace<HighScorePersistence>(HighScorePersistence{high_scores_path.string()});
    }

    // Cameras + render targets + GPU meshes
    camera::init(reg);

    // MeshChild auto-cleanup: destroy children when parent obstacle is destroyed.
    // Prime ObstacleChildren pool BEFORE connecting on_destroy<ObstacleTag>.
    // EnTT destroy() iterates pools in reverse insertion order; priming first
    // ensures ObstacleChildren has a lower index so it's removed last — i.e.,
    // it's still readable when on_obstacle_destroy fires for ObstacleTag.
    reg.storage<ObstacleChildren>();
    reg.on_destroy<ObstacleTag>().connect<&on_obstacle_destroy>();
    wire_obstacle_model_lifecycle(reg);

    // UI + beatmap + music
    reg.ctx().emplace<UIActiveCache>();
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
    // game_state_system runs FIRST and owns the authoritative GoEvent /
    // ButtonPressEvent drain for this tick (calls disp.update<GoEvent>() and
    // disp.update<ButtonPressEvent>() at its top).  All pre-tick enqueues from
    // input_system, gesture_routing, and hit_test are delivered
    // here to listeners in registration order (see wire_input_dispatcher).
    // Systems later in this list that also call disp.update<T>() (e.g.,
    // player_input_system) will find an empty queue and execute as no-ops.
    game_state_system(reg, dt);
    song_playback_system(reg, dt);
    beat_log_system(reg, dt);
    beat_scheduler_system(reg, dt);
    player_input_system(reg, dt);
    shape_window_system(reg, dt);
    player_movement_system(reg, dt);
    scroll_system(reg, dt);
    collision_system(reg, dt);
    miss_detection_system(reg, dt);
    scoring_system(reg, dt);
    energy_system(reg, dt);
    particle_system(reg, dt);
    obstacle_despawn_system(reg, dt);
    popup_display_system(reg, dt);
    ui_navigation_system(reg, dt);
}

// One frame: input → fixed timestep → render → blit → audio.
// Not in header — called by game_loop_run and platform_run_loop (Emscripten).
void game_loop_frame(entt::registry& reg, float& accumulator) {
    float raw_dt = GetFrameTime();
    accumulator += raw_dt;
    if (accumulator > MAX_ACCUM) accumulator = MAX_ACCUM;

    compute_screen_transform(reg);
    input_system(reg, raw_dt);
    // Deliver Tier-1 InputEvents: fires gesture_routing_handle_input and
    // hit_test_handle_input in registration order, enqueuing GoEvent /
    // ButtonPressEvent into the Tier-2 queues for fixed-step delivery.
    reg.ctx().get<entt::dispatcher>().update<InputEvent>();
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

bool game_loop_should_quit(entt::registry& reg) {
    if (WindowShouldClose()) return true;
    auto* input = reg.ctx().find<InputState>();
    return input && input->quit_requested;
}

void game_loop_run(entt::registry& reg) {
#ifdef __EMSCRIPTEN__
    platform_run_loop(reg);
#else
    float accumulator = 0.0f;
    while (!game_loop_should_quit(reg)) {
        game_loop_frame(reg, accumulator);
    }
#endif
}

// ── Shutdown ────────────────────────────────────────────────────────────────

void game_loop_shutdown(entt::registry& reg) {
    // Disconnect all destroy/construct listeners before clearing entities
    unwire_input_dispatcher(reg);
    reg.on_destroy<ObstacleTag>().disconnect<&on_obstacle_destroy>();
    reg.on_construct<ObstacleTag>().disconnect<&session_log_on_obstacle_spawn>();
    reg.on_construct<ScoredTag>().disconnect<&session_log_on_scored>();

    // Destroy all entities while GPU context is still alive
    reg.clear();
    unwire_obstacle_model_lifecycle(reg);

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
    sfx_bank_unload(reg);
    CloseAudioDevice();
    CloseWindow();
}
