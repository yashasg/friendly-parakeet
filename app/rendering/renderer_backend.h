#pragma once

#include <cstdint>
#include "runtime/runtime_api.h"

namespace platform::graphics {

[[nodiscard]] float frame_time() noexcept;
void set_clip_planes(double near_plane, double far_plane);
void begin_texture_mode(RenderTexture2D target);
void end_texture_mode();
void begin_drawing();
void end_drawing();
void clear_background(Color color);
void draw_texture_pro(Texture2D texture,
                      const Rectangle& source,
                      const Rectangle& destination,
                      const Vector2& origin,
                      float rotation,
                      Color tint);
void begin_mode_3d(const Camera3D& camera);
void end_mode_3d();
void draw_line_3d(const Vector3& start, const Vector3& end, Color color);
void draw_triangle_3d(const Vector3& a,
                      const Vector3& b,
                      const Vector3& c,
                      Color color);
struct RendererValidationCounters {
    uint32_t frame_time_calls = 0;
    uint32_t begin_texture_mode_calls = 0;
    uint32_t end_texture_mode_calls = 0;
    uint32_t begin_drawing_calls = 0;
    uint32_t begin_drawing_skipped_not_ready = 0;
    uint32_t end_drawing_calls = 0;
    uint32_t swap_window_calls = 0;
    uint32_t clear_background_calls = 0;
    uint32_t clear_background_skipped_not_ready = 0;
    uint32_t draw_texture_pro_calls = 0;
    uint32_t begin_mode_3d_calls = 0;
    uint32_t end_mode_3d_calls = 0;
    uint32_t set_clip_planes_calls = 0;
    uint32_t draw_line_3d_calls = 0;
    uint32_t draw_triangle_3d_calls = 0;
    Color last_clear_background = Color{0, 0, 0, 0};
};
void reset_renderer_validation_counters() noexcept;
[[nodiscard]] RendererValidationCounters renderer_validation_counters() noexcept;
void set_renderer_frame_time_override(float frame_time_seconds) noexcept;
void clear_renderer_frame_time_override() noexcept;

}  // namespace platform::graphics
