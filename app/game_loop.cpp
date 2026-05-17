#include "game_loop.h"
#include "version.h"
#include "constants.h"
#include "systems/input.h"
#include "util/app_dir_path.h"
#include "components/game_state.h"
#include "components/scoring.h"
#include "components/audio.h"
#include "systems/sfx_bank.h"
#include "systems/audio_routing.h"
#include "entities/settings.h"
#include "components/rhythm.h"
#include "components/music.h"
#include "components/rendering.h"
#include "systems/test_player.h"
#include "components/obstacle.h"
#include "systems/all_systems.h"
#include "systems/game_phase_transition.h"
#include "systems/runtime_systems.h"
#include "systems/test_player_session.h"
#include "systems/input_routing.h"
#include "components/text_resources.h"
#include "systems/session_logger_system.h"
#include "systems/camera_system.h"
#include "entities/beat_map.h"
#include "platform_display.h"
#include "systems/persistence_policy_system.h"
#include "components/high_score.h"
#include "systems/high_score_system.h"
#include "systems/haptics_backend.h"
#include "systems/screen_lifecycle_system.h"
#include "systems/game_over_scoreboard_bind_system.h"
#include "systems/song_complete_scoreboard_bind_system.h"
#include "systems/settings_screen_bind_system.h"
#include "systems/title_start_tap_system.h"
#include "systems/tutorial_dodge_hint_bind_system.h"
#include "systems/gameplay_hud_bind_system.h"
#include "systems/approach_ring_envelope_system.h"
#include "systems/ui_update_system.h"

#include <raylib.h>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

static constexpr float FIXED_DT = 1.0f / 60.0f;

namespace {

template <typename T, typename... Args>
T& reset_ctx_singleton(entt::registry& reg, Args&&... args) {
    reg.ctx().erase<T>();
    return reg.ctx().emplace<T>(std::forward<Args>(args)...);
}

bool load_text_fonts(TextContext& ctx, const char* font_path) {
    if (!FileExists(font_path)) {
        TraceLog(LOG_WARNING, "Font file not found: %s", font_path);
        return false;
    }

    ctx.font_small  = LoadFontEx(font_path, 16, nullptr, 0);
    ctx.font_medium = LoadFontEx(font_path, 28, nullptr, 0);
    ctx.font_large  = LoadFontEx(font_path, 48, nullptr, 0);

    if (ctx.font_small.baseSize == 0 ||
        ctx.font_medium.baseSize == 0 ||
        ctx.font_large.baseSize == 0) {
        TraceLog(LOG_WARNING, "Failed to load font: %s", font_path);
        if (ctx.font_large.baseSize > 0)  UnloadFont(ctx.font_large);
        if (ctx.font_medium.baseSize > 0) UnloadFont(ctx.font_medium);
        if (ctx.font_small.baseSize > 0)  UnloadFont(ctx.font_small);
        ctx.font_large = {};
        ctx.font_medium = {};
        ctx.font_small = {};
        ctx.loaded = false;
        return false;
    }

    SetTextureFilter(ctx.font_small.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(ctx.font_medium.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(ctx.font_large.texture, TEXTURE_FILTER_BILINEAR);

    ctx.loaded = true;
    return true;
}

bool load_default_text_fonts(TextContext& ctx) {
    std::string exe_font = util::join_app_dir(
        GetApplicationDirectory(), "content/fonts/LiberationMono-Regular.ttf");
    const char* font_paths[] = {
        exe_font.c_str(),
        "content/fonts/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    };
    for (const char* path : font_paths) {
        if (load_text_fonts(ctx, path)) {
            TraceLog(LOG_INFO, "Loaded font: %s", path);
            return true;
        }
    }
    TraceLog(LOG_ERROR, "Could not load any TTF font");
    return false;
}

void unload_text_fonts(TextContext& ctx) {
    ctx.release();
}

// Status → (log_level, message_template) lookup table. Per Fabian's
// existential processing (issue #1277), behavior dispatch on the `Status`
// discriminator is replaced with a value→data lookup — same shape as the
// `to_string(Shape)` / `HapticEvent → TriggerPattern` lookups (issue #1309
// converted these label→value mappings to constexpr table indexing).
struct PersistenceLogRow {
    persistence::Status status;
    int log_level;            // raylib TraceLogLevel (LOG_INFO / LOG_WARNING)
    const char* message_fmt;  // "%s: …" — operation name interpolates as 1st arg
};

constexpr PersistenceLogRow kPersistenceLogTable[] = {
    {persistence::Status::Success,     LOG_INFO,    "%s: success"},
    {persistence::Status::MissingFile, LOG_INFO,    "%s: missing file (defaults in use)"},
    {persistence::Status::CorruptData, LOG_WARNING, "%s: corrupt data (defaults in use)"},
};

const PersistenceLogRow* find_persistence_log_row(persistence::Status status) {
    for (const auto& row : kPersistenceLogTable) {
        if (row.status == status) return &row;
    }
    return nullptr;
}

void log_persistence_result(const char* operation, const persistence::Result& result) {
    if (const auto* row = find_persistence_log_row(result.status)) {
        TraceLog(row->log_level, row->message_fmt, operation);
        return;
    }
    // Fallback for statuses not in the table (any failure status that isn't
    // CorruptData) — stringify via persistence::status_name and include the
    // OS error code message when available.
    if (result.error) {
        TraceLog(LOG_WARNING, "%s: %s (%s)", operation,
                 persistence::status_name(result.status),
                 result.error.message().c_str());
    } else {
        TraceLog(LOG_WARNING, "%s: %s", operation,
                 persistence::status_name(result.status));
    }
}

}  // namespace

// ── Init ────────────────────────────────────────────────────────────────────

bool game_loop_init(entt::registry& reg,
                    bool test_player_mode,
                    SkillConfig test_skill,
                    const char* difficulty,
                    int selected_level) {
    // Platform: window + audio
    std::string window_title = std::string("SHAPESHIFTER v") + SHAPESHIFTER_VERSION;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(constants::SCREEN_W, constants::SCREEN_H, window_title.c_str());
    if (!IsWindowReady()) {
        TraceLog(LOG_WARNING, "Window initialization failed; startup aborted");
        return false;
    }
    const char* smoke_mode = std::getenv("SHAPESHIFTER_STARTUP_SHUTDOWN_SMOKE");
    const bool startup_shutdown_smoke = smoke_mode && smoke_mode[0] != '\0' &&
        !(smoke_mode[0] == '0' && smoke_mode[1] == '\0');
    if (!startup_shutdown_smoke) {
        const int monitor_count = GetMonitorCount();
        const int mon = monitor_count > 0 ? GetCurrentMonitor() : -1;
        if (mon >= 0 && mon < monitor_count) {
            int mon_h = GetMonitorHeight(mon);
            int mon_w = GetMonitorWidth(mon);
            if (mon_w > 0 && mon_h > 0) {
                int win_h = static_cast<int>(mon_h * 0.85f);
                int win_w = win_h * constants::SCREEN_W / constants::SCREEN_H;
                SetWindowSize(win_w, win_h);
                SetWindowPosition(
                    (mon_w - win_w) / 2,
                    (mon_h - win_h) / 2);
            }
        }
    }
    SetTargetFPS(60);
    InitAudioDevice();
    sfx_bank_init(reg);
    TraceLog(LOG_INFO, "SHAPESHIFTER v%s", SHAPESHIFTER_VERSION);

    // Text rendering
    {
        auto& text_ctx = reset_ctx_singleton<TextContext>(reg);
        load_default_text_fonts(text_ctx);
    }

    // Core singletons
    reset_ctx_singleton<InputState>(reg);
    reset_ctx_singleton<entt::dispatcher>(reg);
    wire_input_dispatcher(reg);
    wire_audio_haptic_dispatcher(reg);
    input_system_init(reg);
    reset_ctx_singleton<GameState>(reg, GameState{});
    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    reset_ctx_singleton<ScoreState>(reg);
    reset_ctx_singleton<ScoreDisplay>(reg);
    reset_ctx_singleton<CurrentSongHighScore>(reg);
    reset_ctx_singleton<LevelSelectState>(reg);
    reset_ctx_singleton<EnergyState>(reg);
    reset_ctx_singleton<SongResults>(reg);
    reset_ctx_singleton<TestPlayerState>(reg);
    reset_ctx_singleton<TestPlayerSessionState>(reg);
    reset_ctx_singleton<SessionLog>(reg);
    runtime_system_scratch_init(reg);

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
            platform::haptics::warmup(reg);
        }
        create_settings_entity(reg, settings, settings_persistence);
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
        reset_ctx_singleton<HighScoreState>(reg, hs);
        reset_ctx_singleton<HighScorePersistence>(reg, persistence_state);
        reset_ctx_singleton<HighScoreSession>(reg);
    }

    // Cameras + render targets + GPU meshes
    camera::init(reg);

    // UI + beatmap + music
    create_beat_map_entity(reg);
    reset_ctx_singleton<SongState>(reg);
    reset_ctx_singleton<MusicContext>(reg);

    // Test player (optional)
    if (test_player_mode) {
        test_player_init(reg, test_skill, difficulty, selected_level);
    }
    return true;
}

// ── Run ─────────────────────────────────────────────────────────────────────

// tick_fixed_systems is defined in systems/fixed_tick_runner.cpp (shapeshifter_lib)
// so integration tests can call it without the full render/input graph.

// One frame: input → fixed timestep → render → blit → audio.
// Not in header — called by game_loop_run and platform_run_loop (Emscripten).
void game_loop_frame(entt::registry& reg, float& accumulator) {
    auto* session_log = reg.ctx().find<SessionLog>();
    if (session_log) session_log_begin_frame(*session_log);

    float raw_dt = clamp_frame_dt(GetFrameTime());
    accumulator += raw_dt;
    if (accumulator > kMaxFrameDt) accumulator = kMaxFrameDt;

    compute_screen_transform(reg);
    input_system(reg, raw_dt);
    ui_update_system(reg);
    title_start_tap_system(reg);
    test_player_system(reg, raw_dt);

    while (accumulator >= FIXED_DT) {
        tick_fixed_systems(reg, FIXED_DT);
        accumulator -= FIXED_DT;
    }

    // Screen entity lifecycle: spawn/despawn per-screen UI entities to
    // match the active `GamePhase*Tag` produced by the fixed tick. Must
    // run before render so this frame's UI reflects the post-transition
    // phase. Issue #1287.
    screen_lifecycle_system(reg);

    // Per-screen UI label binding: write platform / runtime-dependent text
    // into the dynamic-text slots spawned by `screen_lifecycle_system`.
    // Must run after spawn and before render (#1291, #1292, #1293, #1297).
    tutorial_dodge_hint_bind_system(reg);
    song_complete_scoreboard_bind_system(reg);
    game_over_scoreboard_bind_system(reg);
    settings_screen_bind_system(reg);
    gameplay_hud_bind_system(reg);
    approach_ring_envelope_system(reg);

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

    if (session_log) session_log_flush(*session_log);
}

bool game_loop_should_quit(entt::registry& reg) {
#ifndef __EMSCRIPTEN__
    if (WindowShouldClose()) return true;
#endif
    auto* input = reg.ctx().find<InputState>();
    return input && input->quit_requested;
}

void game_loop_run(entt::registry& reg) {
    if (!IsWindowReady()) {
        TraceLog(LOG_WARNING, "Run loop skipped because window is not ready");
        return;
    }
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
#ifdef __EMSCRIPTEN__
    platform_disarm_wasm_beforeunload(reg);
#endif

    // Disconnect all destroy/construct listeners before clearing entities
    unwire_input_dispatcher(reg);
    unwire_audio_haptic_dispatcher(reg);
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
        if (music) {
            music->release();
        }
    }
    camera::shutdown(reg);
    if (auto* text_ctx = reg.ctx().find<TextContext>()) {
        unload_text_fonts(*text_ctx);
    }
    sfx_bank_unload(reg);
    platform::haptics::shutdown(reg);
    if (IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    if (IsWindowReady()) {
        CloseWindow();
    }

    // Lifecycle contract: shutdown leaves the registry reusable for a fresh
    // game_loop_init() on the same entt::registry instance.
    reg = entt::registry{};
}
