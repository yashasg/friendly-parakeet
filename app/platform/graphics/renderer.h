#pragma once

#include <raylib.h>

namespace platform::graphics {

class Renderer {
public:
    virtual ~Renderer() = default;

    [[nodiscard]] virtual float frame_time() const noexcept = 0;
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
};

Renderer& renderer();
Renderer& raylib_renderer();
Renderer& sdl2_renderer();

}  // namespace platform::graphics
