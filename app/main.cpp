#include <raylib.h>
#include <entt/entt.hpp>

#include "version.h"
#include "constants.h"
#include "game_loop.h"
#include "platform_display.h"
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
#include "components/camera.h"
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

    // ── ENTT REGISTRY + SINGLETONS ──────────────────────────
    entt::registry reg;

    // Text rendering
    {
        auto& text_ctx = reg.ctx().emplace<TextContext>();
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
        if (!font_loaded) TraceLog(LOG_ERROR, "Could not load any TTF font");
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
    reg.ctx().emplace<LevelSelectState>();
    reg.ctx().emplace<EnergyState>();
    reg.ctx().emplace<SongResults>();

    // Camera + GPU meshes
    {
        Camera3D cam3d = {};
        cam3d.position   = {to_world(360.0f), to_world(460.0f), to_world(2390.0f)};
        cam3d.target     = {to_world(360.0f), to_world(0.0f),   to_world(400.0f)};
        cam3d.up         = {0.0f, 1.0f, 0.0f};
        cam3d.fovy       = 45.0f;
        cam3d.projection = CAMERA_PERSPECTIVE;
        reg.ctx().emplace<Camera3D>(cam3d);
    }
    reg.ctx().emplace<ScreenTransform>();
    reg.ctx().emplace<camera::ShapeMeshes>(camera::build_shape_meshes());

    // UI + beatmap + music
    reg.ctx().emplace<UIState>(load_ui());
    reg.ctx().emplace<BeatMap>();
    reg.ctx().emplace<SongState>();
    reg.ctx().emplace<MusicContext>();
    spawn_title_buttons(reg);

    // ── Test player setup ────────────────────────────────────
    if (test_player_mode) {
        auto& lss = reg.ctx().get<LevelSelectState>();
        lss.selected_level = 1;
        for (int d = 0; d < 3; ++d)
            if (std::strcmp(LevelSelectState::DIFFICULTY_KEYS[d], difficulty) == 0)
                { lss.selected_difficulty = d; break; }

        auto& tp_state = reg.ctx().emplace<TestPlayerState>();
        tp_state.skill  = test_skill;
        tp_state.active = true;
        tp_state.rng.seed(static_cast<unsigned>(std::time(nullptr)));

        const char* skill_names[] = { "pro", "good", "bad" };
        TraceLog(LOG_INFO, "TEST PLAYER: skill=%s",
                 skill_names[static_cast<int>(test_skill)]);

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

        reg.on_construct<ObstacleTag>().connect<&session_log_on_obstacle_spawn>();
        reg.on_construct<ScoredTag>().connect<&session_log_on_scored>();
    }

    // ── Render target + game loop ────────────────────────────
    RenderTexture2D target = LoadRenderTexture(
        constants::SCREEN_W, constants::SCREEN_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

#ifdef __EMSCRIPTEN__
    platform_run_loop(reg, target);
#else
    float accumulator = 0.0f;
    while (!WindowShouldClose()) {
        game_loop_frame(reg, accumulator, target);
        if (reg.ctx().get<InputState>().quit_requested) break;
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
