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
#include "systems/all_systems.h"
#include "text_renderer.h"
#include "file_logger.h"

#include <string>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// ── Emscripten main-loop state ───────────────────────────────
// The browser event loop is non-blocking, so we extract the loop body
// into a free function and hand it to emscripten_set_main_loop.
static constexpr float FIXED_DT  = 1.0f / 60.0f;
static constexpr float MAX_ACCUM = 0.1f;

#ifdef __EMSCRIPTEN__
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

    input_system(reg, raw_dt);

    while (g_loop.accumulator >= FIXED_DT) {
        gesture_system(reg, FIXED_DT);
        game_state_system(reg, FIXED_DT);
        player_action_system(reg, FIXED_DT);
        player_movement_system(reg, FIXED_DT);
        difficulty_system(reg, FIXED_DT);
        obstacle_spawn_system(reg, FIXED_DT);
        scroll_system(reg, FIXED_DT);
        burnout_system(reg, FIXED_DT);
        collision_system(reg, FIXED_DT);
        scoring_system(reg, FIXED_DT);
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

int main(int /*argc*/, char* /*argv*/[]) {

    // ── FILE LOGGING (must be before InitWindow) ──────────────
    file_logger_init("shapeshifter.log");

    // ── RAYLIB INIT ──────────────────────────────────────────
    std::string window_title = std::string("SHAPESHIFTER v") + SHAPESHIFTER_VERSION;
    InitWindow(
        constants::SCREEN_W, constants::SCREEN_H,
        window_title.c_str()
    );
    SetTargetFPS(60);

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

        // Phase 0: Input (polls raylib input state)
        input_system(reg, raw_dt);
        if (reg.ctx().get<InputState>().quit_requested) break;

        // Fixed timestep loop — all systems self-guard on GamePhase
        while (accumulator >= FIXED_DT) {
            gesture_system(reg, FIXED_DT);
            game_state_system(reg, FIXED_DT);
            player_action_system(reg, FIXED_DT);
            player_movement_system(reg, FIXED_DT);
            difficulty_system(reg, FIXED_DT);
            obstacle_spawn_system(reg, FIXED_DT);
            scroll_system(reg, FIXED_DT);
            burnout_system(reg, FIXED_DT);
            collision_system(reg, FIXED_DT);
            scoring_system(reg, FIXED_DT);
            lifetime_system(reg, FIXED_DT);
            particle_system(reg, FIXED_DT);
            cleanup_system(reg, FIXED_DT);
            accumulator -= FIXED_DT;
        }

        // Render
        float alpha = accumulator / FIXED_DT;
        BeginDrawing();
            render_system(reg, alpha);
        EndDrawing();

        // Audio
        audio_system(reg);
    }
#endif

    // ── SHUTDOWN ─────────────────────────────────────────────
    text_shutdown(reg.ctx().get<TextContext>());
    CloseWindow();
    file_logger_shutdown();
    return 0;
}
