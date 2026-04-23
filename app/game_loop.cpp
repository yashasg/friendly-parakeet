#include "game_loop.h"
#include "constants.h"
#include "components/input.h"
#include "components/game_state.h"
#include "components/session_log.h"
#include "components/rendering.h"
#include "systems/all_systems.h"
#include "systems/session_logger.h"
#include "platform_display.h"

#include <algorithm>

static constexpr float FIXED_DT  = 1.0f / 60.0f;
static constexpr float MAX_ACCUM = 0.1f;

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

void game_loop_frame(entt::registry& reg, float& accumulator,
                     RenderTexture2D& target) {
    float raw_dt = GetFrameTime();
    accumulator += raw_dt;
    if (accumulator > MAX_ACCUM) accumulator = MAX_ACCUM;

    camera_system(reg, raw_dt);
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

    // Blit render target to window, letterboxed
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
        DrawTexturePro(target.texture, src, dst, {0, 0}, 0.0f, WHITE);
    EndDrawing();

    audio_system(reg);

    auto* session_log = reg.ctx().find<SessionLog>();
    if (session_log) session_log_flush(*session_log);
}
