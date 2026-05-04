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
#include "entities/obstacle_render_entity.h"
#include "platform_display.h"
#include "platform/graphics/renderer.h"
#include "platform/window/window_manager.h"
#include "util/persistence_policy.h"
#include "util/settings_persistence.h"
#include "components/high_score.h"
#include "util/high_score_persistence.h"
#include "platform/haptics_backend.h"

#include <raylib.h>
#include <algorithm>
#include <string>

static constexpr float FIXED_DT  = 1.0f / 60.0f;
static constexpr float MAX_ACCUM = 0.1f;

namespace {

void log_persistence_result(const char* operation, const persistence::Result& result) {
    switch (result.status) {
        case persistence::Status::Success:
            TraceLog(LOG_INFO, "%s: success", operation);
            break;
        case persistence::Status::MissingFile:
            TraceLog(LOG_INFO, "%s: missing file (defaults in use)", operation);
            break;
        case persistence::Status::CorruptData:
            TraceLog(LOG_WARNING, "%s: corrupt data (defaults in use)", operation);
            break;
        default:
            if (result.error) {
                TraceLog(LOG_WARNING, "%s: %s (%s)", operation,
                         persistence::status_name(result.status),
                         result.error.message().c_str());
            } else {
                TraceLog(LOG_WARNING, "%s: %s", operation,
                         persistence::status_name(result.status));
            }
            break;
    }
}

}  // namespace

// ── Init ────────────────────────────────────────────────────────────────────

void game_loop_init(entt::registry& reg,
                    bool test_player_mode,
                    TestPlayerSkill test_skill,
                    const char* difficulty) {
    // Platform: window + audio
    std::string window_title = std::string("SHAPESHIFTER v") + SHAPESHIFTER_VERSION;
    auto& window = platform::window::window_manager();
    window.set_resizable_flag();
    window.init_window(constants::SCREEN_W, constants::SCREEN_H, window_title.c_str());
    {
        int mon = window.current_monitor();
        int mon_h = window.monitor_height(mon);
        int win_h = static_cast<int>(mon_h * 0.85f);
        int win_w = win_h * constants::SCREEN_W / constants::SCREEN_H;
        window.set_window_size(win_w, win_h);
        window.set_window_position(
            (window.monitor_width(mon) - win_w) / 2,
            (mon_h - win_h) / 2);
    }
    window.set_target_fps(60);

#if defined(SHAPESHIFTER_BACKEND_SDL2)
    (void)test_player_mode;
    (void)test_skill;
    (void)difficulty;
    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<entt::dispatcher>();
    wire_input_dispatcher(reg);
    input_system_init(reg);
    reg.ctx().emplace<ScreenTransform>();
    reg.ctx().emplace<GameState>(GameState{
        .phase = GamePhase::Playing, .previous_phase = GamePhase::Playing,
        .phase_timer = 0.0f, .transition_pending = false,
        .next_phase = GamePhase::Playing, .transition_alpha = 0.0f
    });
    spawn_game_camera(reg);
    spawn_ui_camera(reg);
    reg.ctx().emplace<RenderTargets>();
    FloorParams floor{};
    floor.size = constants::FLOOR_SHAPE_SIZE * constants::FLOOR_SCALE_REST;
    floor.half = floor.size * 0.5f;
    floor.thick = constants::FLOOR_OUTLINE_THICK;
    floor.alpha = static_cast<uint8_t>(constants::FLOOR_ALPHA_REST);
    reg.ctx().emplace<FloorParams>(floor);
    TraceLog(LOG_INFO, "SHAPESHIFTER v%s (SDL2 backend phase-4 input slice)",
             SHAPESHIFTER_VERSION);
    return;
#endif

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
    input_system_init(reg);
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

    persistence::Paths persistence_paths;
    const auto path_result = persistence::resolve_paths(persistence_paths);

    // Settings — load from disk; defaults remain if file is missing/corrupt/path-invalid.
    {
        SettingsState settings;
        SettingsPersistence settings_persistence;
        if (path_result.ok()) {
            settings_persistence.path = persistence_paths.settings_file.string();
            settings_persistence.last_load = settings::load_settings(settings, persistence_paths.settings_file);
        } else {
            settings_persistence.last_load = path_result;
        }
        log_persistence_result("settings load", settings_persistence.last_load);
        if (settings.haptics_enabled) {
            platform::haptics::warmup();
        }
        reg.ctx().emplace<SettingsState>(settings);
        reg.ctx().emplace<SettingsPersistence>(settings_persistence);
    }

    // High scores — load from disk; defaults remain if file is missing/corrupt/path-invalid.
    {
        HighScoreState hs;
        HighScorePersistence persistence_state;
        if (path_result.ok()) {
            persistence_state.path = persistence_paths.high_scores_file.string();
            persistence_state.last_load = high_score::load_high_scores(hs, persistence_paths.high_scores_file);
        } else {
            persistence_state.last_load = path_result;
        }
        log_persistence_result("high score load", persistence_state.last_load);
        reg.ctx().emplace<HighScoreState>(hs);
        reg.ctx().emplace<HighScorePersistence>(persistence_state);
    }

    // Cameras + render targets + GPU meshes
    camera::init(reg);

    // MeshChild auto-cleanup: destroy children when parent ownership is removed.
    wire_obstacle_mesh_lifetime(reg);
    wire_obstacle_model_lifecycle(reg);

    // UI + beatmap + music
    reg.ctx().emplace<BeatMap>();
    reg.ctx().emplace<SongState>();
    reg.ctx().emplace<MusicContext>();

    // Test player (optional)
    if (test_player_mode) {
        test_player_init(reg, test_skill, difficulty);
    }
}

// ── Run ─────────────────────────────────────────────────────────────────────

// tick_fixed_systems is defined in systems/fixed_tick_runner.cpp (shapeshifter_lib)
// so integration tests can call it without the full render/input graph.

// One frame: input → fixed timestep → render → blit → audio.
// Not in header — called by game_loop_run and platform_run_loop (Emscripten).
void game_loop_frame(entt::registry& reg, float& accumulator) {
#if defined(SHAPESHIFTER_BACKEND_SDL2)
    (void)accumulator;
    compute_screen_transform(reg);
    input_system(reg, 0.0f);
    reg.ctx().get<entt::dispatcher>().update<InputEvent>();
    auto& renderer = platform::graphics::renderer();
    auto& targets = reg.ctx().get<RenderTargets>();
    renderer.begin_texture_mode(targets.world);
    game_render_system(reg, 0.0f);
    renderer.end_texture_mode();
    renderer.begin_texture_mode(targets.ui);
    ui_render_system(reg, 0.0f);
    renderer.end_texture_mode();

    const auto& st = reg.ctx().get<ScreenTransform>();
    float dst_w = constants::SCREEN_W * st.scale;
    float dst_h = constants::SCREEN_H * st.scale;
    Rectangle src = {0, 0, static_cast<float>(constants::SCREEN_W), -static_cast<float>(constants::SCREEN_H)};
    Rectangle dst = {st.offset_x, st.offset_y, dst_w, dst_h};

    platform_pre_blit();
    renderer.begin_drawing();
    renderer.clear_background(BLACK);
    renderer.draw_texture_pro(targets.world.texture, src, dst, {0, 0}, 0.0f, WHITE);
    renderer.draw_texture_pro(targets.ui.texture, src, dst, {0, 0}, 0.0f, WHITE);
    renderer.end_drawing();
    return;
#else
    auto& renderer = platform::graphics::renderer();
    float raw_dt = renderer.frame_time();
    accumulator += raw_dt;
    if (accumulator > MAX_ACCUM) accumulator = MAX_ACCUM;

    compute_screen_transform(reg);
    input_system(reg, raw_dt);
    // Deliver Tier-1 InputEvents: fires gesture_routing_handle_input and
    // enqueues GoEvent into the Tier-2 queue for fixed-step delivery.
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
    renderer.begin_texture_mode(targets.world);
        game_render_system(reg, 0.0f);
    renderer.end_texture_mode();

    // Pass 2: UI (2D)
    renderer.begin_texture_mode(targets.ui);
        ui_render_system(reg, 0.0f);
    renderer.end_texture_mode();

    // Composite: blit world, then UI (alpha-blended) onto window
    const auto& st = reg.ctx().get<ScreenTransform>();
    float dst_w = constants::SCREEN_W * st.scale;
    float dst_h = constants::SCREEN_H * st.scale;
    Rectangle src = { 0, 0,
        static_cast<float>(constants::SCREEN_W),
        -static_cast<float>(constants::SCREEN_H) };
    Rectangle dst = { st.offset_x, st.offset_y, dst_w, dst_h };

    platform_pre_blit();
    renderer.begin_drawing();
        renderer.clear_background(BLACK);
        renderer.draw_texture_pro(targets.world.texture, src, dst, {0, 0}, 0.0f, WHITE);
        renderer.draw_texture_pro(targets.ui.texture, src, dst, {0, 0}, 0.0f, WHITE);
    renderer.end_drawing();

    audio_system(reg);
    haptic_system(reg);

    auto* session_log = reg.ctx().find<SessionLog>();
    if (session_log) session_log_flush(*session_log);
#endif
}

bool game_loop_should_quit(entt::registry& reg) {
#ifndef __EMSCRIPTEN__
    if (platform::window::window_manager().should_close()) return true;
#endif
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
#if defined(SHAPESHIFTER_BACKEND_SDL2)
    (void)reg;
    platform::window::window_manager().close_window();
    return;
#else
    // Disconnect all destroy/construct listeners before clearing entities
    unwire_input_dispatcher(reg);
    unwire_obstacle_mesh_lifetime(reg);
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
    platform::window::window_manager().close_window();
#endif
}
