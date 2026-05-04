#pragma once

#include <cstdint>
#include <raylib.h>

namespace platform::graphics {

class Renderer {
public:
    virtual ~Renderer() = default;

    [[nodiscard]] virtual float frame_time() const noexcept = 0;
    virtual void set_clip_planes(double near_plane, double far_plane) = 0;
    virtual void begin_texture_mode(RenderTexture2D target) = 0;
    virtual void end_texture_mode() = 0;
    virtual void begin_drawing() = 0;
    virtual void end_drawing() = 0;
    virtual void clear_background(Color color) = 0;
    virtual void draw_texture_pro(Texture2D texture,
                                  const Rectangle& source,
                                  const Rectangle& destination,
                                  const Vector2& origin,
                                  float rotation,
                                  Color tint) = 0;
    virtual void begin_mode_3d(const Camera3D& camera) = 0;
    virtual void end_mode_3d() = 0;
    virtual void draw_line_3d(const Vector3& start, const Vector3& end, Color color) = 0;
    virtual void draw_triangle_3d(const Vector3& a,
                                  const Vector3& b,
                                  const Vector3& c,
                                  Color color) = 0;
};

Renderer& renderer();
Renderer& raylib_renderer();
Renderer& sdl2_renderer();
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
