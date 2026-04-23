#include "platform_display.h"
#include "game_loop.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <rlgl.h>
#include <algorithm>

void platform_get_display_size(float& out_w, float& out_h) {
    double css_w = 0.0, css_h = 0.0;
    emscripten_get_element_css_size("#canvas", &css_w, &css_h);
    out_w = static_cast<float>(std::max(1.0, css_w));
    out_h = static_cast<float>(std::max(1.0, css_h));

    // Sync drawing buffer to CSS size so coords align
    int cur_w = 0, cur_h = 0;
    emscripten_get_canvas_element_size("#canvas", &cur_w, &cur_h);
    int target_w = static_cast<int>(out_w);
    int target_h = static_cast<int>(out_h);
    if (cur_w != target_w || cur_h != target_h)
        emscripten_set_canvas_element_size("#canvas", target_w, target_h);
}

void platform_pre_blit() {
    // After canvas buffer resize, raylib's viewport may not reflect the new
    // dimensions.  Explicitly set viewport + ortho to match the buffer.
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

static struct {
    entt::registry* reg;
    float accumulator;
    RenderTexture2D target;
} g_state;

static void frame_callback() {
    game_loop_frame(*g_state.reg, g_state.accumulator, g_state.target);
}

void platform_run_loop(entt::registry& reg, RenderTexture2D& target) {
    g_state = { &reg, 0.0f, target };
    emscripten_set_main_loop(frame_callback, 0, 1);
}

#else // Native

void platform_get_display_size(float& out_w, float& out_h) {
    out_w = static_cast<float>(GetScreenWidth());
    out_h = static_cast<float>(GetScreenHeight());
}

void platform_pre_blit() {
    // No-op on native — raylib manages the viewport.
}

#endif
