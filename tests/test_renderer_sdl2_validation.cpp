#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "rendering/renderer_backend.h"

TEST_CASE("sdl2 renderer validation counters capture render command stream",
          "[render][sdl2][validation]") {
    using namespace platform::graphics;

    reset_renderer_validation_counters();
    clear_renderer_frame_time_override();

    begin_texture_mode(RenderTexture2D{});  // world pass
    end_texture_mode();
    begin_texture_mode(RenderTexture2D{});  // ui pass
    end_texture_mode();
    begin_drawing();
    clear_background(Color{10, 20, 30, 255});
    draw_texture_pro(Texture2D{}, Rectangle{}, Rectangle{}, Vector2{}, 0.0f, WHITE);  // world composite
    draw_texture_pro(Texture2D{}, Rectangle{}, Rectangle{}, Vector2{}, 0.0f, WHITE);  // ui composite
    begin_mode_3d(Camera3D{});
    draw_triangle_3d({0.0f, 0.0f, 0.0f},
                     {1.0f, 0.0f, 0.0f},
                     {0.0f, 0.0f, 1.0f},
                     RED);  // obstacle
    draw_triangle_3d({2.0f, 0.0f, 2.0f},
                     {2.5f, 0.0f, 2.0f},
                     {2.0f, 0.0f, 2.5f},
                     SKYBLUE);  // particle
    end_mode_3d();
    end_drawing();

    const auto counters = renderer_validation_counters();
    CHECK(counters.begin_texture_mode_calls == 2);
    CHECK(counters.end_texture_mode_calls == 2);
    CHECK(counters.begin_drawing_calls == 1);
    CHECK(counters.clear_background_calls == 1);
    CHECK(counters.draw_texture_pro_calls == 2);
    CHECK(counters.begin_mode_3d_calls == 1);
    CHECK(counters.draw_triangle_3d_calls == 2);
    CHECK(counters.end_mode_3d_calls == 1);
    CHECK(counters.end_drawing_calls == 1);
    CHECK(counters.swap_window_calls == 1);
    CHECK(counters.begin_drawing_skipped_not_ready <= counters.begin_drawing_calls);
    CHECK(counters.clear_background_skipped_not_ready <= counters.clear_background_calls);
    CHECK(counters.last_clear_background.r == 10);
    CHECK(counters.last_clear_background.g == 20);
    CHECK(counters.last_clear_background.b == 30);
    CHECK(counters.last_clear_background.a == 255);
}

TEST_CASE("sdl2 renderer frame-time override is deterministic",
          "[render][sdl2][validation][timing]") {
    using namespace platform::graphics;

    reset_renderer_validation_counters();
    set_renderer_frame_time_override(1.0f / 120.0f);

    CHECK(std::fabs(frame_time() - (1.0f / 120.0f)) < 0.00001f);

    clear_renderer_frame_time_override();
    const float dt = frame_time();
    CHECK(std::isfinite(dt));
    CHECK(dt >= 0.0f);

    const auto counters = renderer_validation_counters();
    CHECK(counters.frame_time_calls == 2);
}
