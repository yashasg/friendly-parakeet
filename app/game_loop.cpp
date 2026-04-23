#include "game_loop.h"
#include "constants.h"
#include "components/input.h"
#include "components/transform.h"
#include "components/rendering.h"
#include "components/game_state.h"
#include "components/session_log.h"
#include "components/camera.h"
#include "systems/all_systems.h"
#include "systems/session_logger.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <rlgl.h>
#endif

#include <algorithm>

static constexpr float FIXED_DT  = 1.0f / 60.0f;
static constexpr float MAX_ACCUM = 0.1f;

// All fixed-timestep systems in execution order.
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

// Recompute letterbox transform so input coords map to virtual resolution.
static void update_screen_transform(entt::registry& reg) {
    float win_w, win_h;
#ifdef __EMSCRIPTEN__
    double css_w = 0.0, css_h = 0.0;
    emscripten_get_element_css_size("#canvas", &css_w, &css_h);
    win_w = static_cast<float>(std::max(1.0, css_w));
    win_h = static_cast<float>(std::max(1.0, css_h));

    int cur_buf_w = 0, cur_buf_h = 0;
    emscripten_get_canvas_element_size("#canvas", &cur_buf_w, &cur_buf_h);
    int target_w = static_cast<int>(win_w);
    int target_h = static_cast<int>(win_h);
    if (cur_buf_w != target_w || cur_buf_h != target_h)
        emscripten_set_canvas_element_size("#canvas", target_w, target_h);
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

// Blit the virtual-resolution render target to the window, letter-boxed.
static void blit_to_window(entt::registry& reg, RenderTexture2D& target) {
    const auto& st = reg.ctx().get<ScreenTransform>();
    float dst_w = constants::SCREEN_W * st.scale;
    float dst_h = constants::SCREEN_H * st.scale;

    Rectangle src = { 0, 0,
        static_cast<float>(constants::SCREEN_W),
        -static_cast<float>(constants::SCREEN_H) };
    Rectangle dst = { st.offset_x, st.offset_y, dst_w, dst_h };

#ifdef __EMSCRIPTEN__
    int buf_w = 0, buf_h = 0;
    emscripten_get_canvas_element_size("#canvas", &buf_w, &buf_h);
    rlViewport(0, 0, buf_w, buf_h);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0.0, static_cast<double>(buf_w),
            static_cast<double>(buf_h), 0.0, 0.0, 1.0);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
#endif

    BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(target.texture, src, dst, {0, 0}, 0.0f, WHITE);
    EndDrawing();
}

void game_loop_frame(entt::registry& reg, float& accumulator,
                     RenderTexture2D& target) {
    float raw_dt = GetFrameTime();
    accumulator += raw_dt;
    if (accumulator > MAX_ACCUM) accumulator = MAX_ACCUM;

    update_screen_transform(reg);
    input_system(reg, raw_dt);
    hit_test_system(reg);
    test_player_system(reg, raw_dt);

    while (accumulator >= FIXED_DT) {
        tick_fixed_systems(reg, FIXED_DT);
        accumulator -= FIXED_DT;
    }

    float alpha = accumulator / FIXED_DT;
    BeginTextureMode(target);
        render_system(reg, alpha);
    EndTextureMode();

    blit_to_window(reg, target);
    audio_system(reg);

    auto* session_log = reg.ctx().find<SessionLog>();
    if (session_log) session_log_flush(*session_log);
}

#ifdef __EMSCRIPTEN__
static struct {
    entt::registry* reg;
    float accumulator;
    RenderTexture2D target;
} g_emscripten_state;

static void emscripten_frame_callback() {
    game_loop_frame(*g_emscripten_state.reg,
                    g_emscripten_state.accumulator,
                    g_emscripten_state.target);
}

void game_loop_init_emscripten(entt::registry& reg, RenderTexture2D& target) {
    g_emscripten_state = { &reg, 0.0f, target };
    emscripten_set_main_loop(emscripten_frame_callback, 0, 1);
}
#endif
