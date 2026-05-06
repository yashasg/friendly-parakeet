#include "game_loop.h"
#include "version.h"
#include "constants.h"
#include "components/input.h"
#include "components/input_events.h"
#include "components/game_state.h"
#include "components/beat_map.h"
#include "components/song_state.h"
#include "components/audio.h"
#include "components/haptics.h"
#include "util/settings.h"
#include "components/transform.h"
#include "components/registry_context.h"
#include "systems/all_systems.h"
#include "systems/sfx_bank_system.h"
#include "session/test_player_session.h"
#include "systems/input_routing.h"
#include "ui/text_renderer.h"
#include "util/session_logger.h"
#include "systems/camera_system.h"
#include "entities/obstacle_render_entity.h"
#include "systems/audio_runtime.h"
#include "systems/platform_state.h"
#include "util/persistence_policy.h"
#include "util/settings_persistence.h"
#include "components/high_score.h"
#include "util/high_score_persistence.h"
#include "systems/haptics_runtime.h"
#include <SDL.h>
#include <cstdio>
#include <filesystem>
#include <string>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

static constexpr float FIXED_DT  = 1.0f / 60.0f;
static constexpr float MAX_ACCUM = 0.1f;

namespace {

void init_core_context_singletons(entt::registry& reg) {
    registry_ctx_insert_defaults<InputState,
                                 GameState,
                                 HapticQueue,
                                 LevelSelectState,
                                 BeatMap,
                                 SongState,
                                 MusicContext>(reg);
}

void log_persistence_result(const char* operation, const persistence::Result& result) {
    switch (result.status) {
        case persistence::Status::Success:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s: success", operation);
            break;
        case persistence::Status::MissingFile:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s: missing file (defaults in use)", operation);
            break;
        case persistence::Status::CorruptData:
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s: corrupt data (defaults in use)", operation);
            break;
        default:
            if (result.error) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s: %s (%s)", operation,
                            persistence::status_name(result.status),
                            result.error.message().c_str());
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", operation,
                            persistence::status_name(result.status));
            }
            break;
    }
}

template <typename State, typename PersistenceState, typename LoadFn>
void load_persistence_context(entt::registry& reg,
                              const char* operation,
                              const persistence::Result& path_result,
                              const std::filesystem::path& file_path,
                              LoadFn&& load_fn) {
    State state{};
    PersistenceState persistence_state{};
    if (path_result.ok()) {
        persistence_state.path = file_path;
        persistence_state.last_load = load_fn(state, file_path);
    } else {
        persistence_state.last_load = path_result;
    }
    log_persistence_result(operation, persistence_state.last_load);
    registry_ctx_insert_or_assign(reg, std::move(state));
    registry_ctx_insert_or_assign(reg, std::move(persistence_state));
}

void shutdown_init_failure(AudioDeviceRuntimeState& audio_device) {
    audio_runtime::shutdown_audio_device(audio_device);
    platform_state::shutdown();
    platform_state::request_close();
}

void draw_frame(entt::registry& reg) {
    frame_render_system(reg);
}

void drain_raw_input_events(entt::registry& reg) {
    registry_ctx_if<entt::dispatcher>(reg, [](entt::dispatcher& dispatcher) {
        dispatcher.update<::InputEvent>();
    });
}

#if defined(__EMSCRIPTEN__)
struct WebLoopState {
    entt::registry* reg = nullptr;
    float accumulator = 0.0f;
};

WebLoopState& web_loop_state() {
    static WebLoopState state;
    return state;
}

void web_shutdown_once() {
    auto& state = web_loop_state();
    if (!state.reg) return;
    entt::registry* reg = state.reg;
    state.reg = nullptr;
    state.accumulator = 0.0f;
    game_loop_shutdown(*reg);
}

void web_frame_callback() {
    auto& state = web_loop_state();
    if (!state.reg) return;
    game_loop_frame(*state.reg, state.accumulator);
    if (game_loop_should_quit(*state.reg)) {
        emscripten_cancel_main_loop();
        web_shutdown_once();
    }
}

const char* web_before_unload(int /*event_type*/, const void* /*reserved*/, void* /*user_data*/) {
    web_shutdown_once();
    return nullptr;
}
#endif

}  // namespace

// ── Init ────────────────────────────────────────────────────────────────────

bool game_loop_init(entt::registry& reg,
                    bool test_player_mode,
                    TestPlayerSkill test_skill,
                    const char* difficulty) {
    // Platform: window + audio
    std::string window_title = std::string("SHAPESHIFTER v") + SHAPESHIFTER_VERSION;
    if (!platform_state::initialize()) {
        std::fprintf(stderr, "game_loop_init failed: SDL init failed (%s)\n",
                     platform_state::last_error());
        platform_state::request_close();
        return false;
    }
    const platform_state::WindowConfig window_config{
        .width = constants::SCREEN_W,
        .height = constants::SCREEN_H,
        .title = window_title.c_str(),
        .resizable = true,
    };
    if (!platform_state::create_window_and_gl_context(window_config)) {
        std::fprintf(stderr, "game_loop_init failed: window/context creation failed\n");
        platform_state::shutdown();
        platform_state::request_close();
        return false;
    }
    {
        int mon = platform_state::current_monitor();
        int mon_h = platform_state::monitor_height(mon);
        int win_h = static_cast<int>(mon_h * 0.85f);
        int win_w = win_h * constants::SCREEN_W / constants::SCREEN_H;
        platform_state::set_window_size(win_w, win_h);
        platform_state::set_window_position(
            (platform_state::monitor_width(mon) - win_w) / 2,
            (mon_h - win_h) / 2);
    }
    platform_state::set_target_fps(60);

    auto& audio_device = registry_ctx_insert_or_assign(reg, AudioDeviceRuntimeState{});
    if (!audio_runtime::init_audio_device(audio_device)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Audio initialization failed; continuing with audio disabled");
    }
    sfx_bank_init(reg);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SHAPESHIFTER v%s", SHAPESHIFTER_VERSION);

    // Text rendering
    {
        auto& text_ctx = registry_ctx_get_or_emplace<TextContext>(reg);
        if (text_ctx.loaded) {
            text_shutdown(text_ctx);
        }
        if (!text_init_default(text_ctx)) {
#if defined(__EMSCRIPTEN__)
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "text_init_default failed on web build; continuing without text rendering");
#else
            std::fprintf(stderr, "game_loop_init failed: text_init_default failed\n");
            shutdown_init_failure(audio_device);
            return false;
#endif
        }
    }

    // Core singletons
    init_core_context_singletons(reg);
    wire_input_dispatcher(reg);
    input_system_init(reg);

    persistence::Paths persistence_paths;
    const auto path_result = persistence::resolve_paths(persistence_paths);

    // Settings + high scores — load from disk; defaults remain if file is
    // missing/corrupt/path-invalid.
    load_persistence_context<SettingsState, SettingsPersistence>(
        reg,
        "settings load",
        path_result,
        persistence_paths.settings_file,
        [](SettingsState& state, const std::filesystem::path& path) {
            return settings::load_settings(state, path);
        });
    registry_ctx_if<SettingsState>(reg, [](const SettingsState& settings) {
        if (settings.haptics_enabled) {
            haptics_runtime::warmup();
        }
    });
    load_persistence_context<HighScoreState, HighScorePersistence>(
        reg,
        "high score load",
        path_result,
        persistence_paths.high_scores_file,
        [](HighScoreState& state, const std::filesystem::path& path) {
            return high_score::load_high_scores(state, path);
        });

    // Cameras + render targets + GPU meshes
    camera::init(reg);

    // Test player (optional)
    if (test_player_mode) {
        test_player_init(reg, test_skill, difficulty);
    }
    return true;
}

// ── Run ─────────────────────────────────────────────────────────────────────

// tick_fixed_systems is defined in systems/fixed_tick_runner.cpp (shapeshifter_lib)
// so integration tests can call it without the full render/input graph.

// One frame: input → fixed timestep → render → blit → audio.
// Not in header — called by game_loop_run and the Emscripten loop callback.
void game_loop_frame(entt::registry& reg, float& accumulator) {
    float raw_dt = platform_state::consume_frame_time();
    accumulator += raw_dt;
    if (accumulator > MAX_ACCUM) accumulator = MAX_ACCUM;

    compute_screen_transform(reg);
    input_system(reg, raw_dt);
    // Deliver Tier-1 InputEvents: fires swipe-to-go routing and
    // enqueues GoEvent into the Tier-2 queue for fixed-step delivery.
    drain_raw_input_events(reg);
    test_player_system(reg, raw_dt);

    while (accumulator >= FIXED_DT) {
        tick_fixed_systems(reg, FIXED_DT);
        accumulator -= FIXED_DT;
    }

    // Camera runs after gameplay systems so transforms reflect current frame
    game_camera_system(reg, raw_dt);
    ui_camera_system(reg, raw_dt);

    draw_frame(reg);

    audio_system(reg);
    haptic_system(reg);

    registry_ctx_if<SessionLog>(reg, [](SessionLog& session_log) {
        session_log_flush(session_log);
    });
}

bool game_loop_should_quit(const entt::registry& reg) {
#ifndef __EMSCRIPTEN__
    if (platform_state::should_close()) return true;
#endif
    const auto* input = reg.ctx().find<InputState>();
    return input != nullptr && input->quit_requested;
}

void game_loop_run(entt::registry& reg) {
#ifdef __EMSCRIPTEN__
    auto& state = web_loop_state();
    state.reg = &reg;
    state.accumulator = 0.0f;
    emscripten_set_beforeunload_callback(nullptr, web_before_unload);
    emscripten_set_main_loop(web_frame_callback, 0, 1);
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
    unwire_obstacle_mesh_lifetime(reg);
    test_player_shutdown(reg);

    // Destroy all entities while GPU context is still alive
    reg.clear();
    unwire_obstacle_model_lifecycle(reg);

    registry_ctx_if<SessionLog>(reg, [](SessionLog& slog) {
        session_log_close(slog);
    });
    registry_ctx_if<MusicContext>(reg, [](MusicContext& music) {
        if (music.loaded) {
            audio_runtime::unload_music_stream(music);
        }
    });
    camera::shutdown(reg);
    registry_ctx_if<TextContext>(reg, [](TextContext& text) {
        text_shutdown(text);
    });
    sfx_bank_unload(reg);
    registry_ctx_if<AudioDeviceRuntimeState>(reg, [](AudioDeviceRuntimeState& audio_device) {
        audio_runtime::shutdown_audio_device(audio_device);
    });
    platform_state::shutdown();
}
