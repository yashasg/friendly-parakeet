#include "renderer.h"
#include <rlgl.h>

namespace platform::graphics {

namespace {

class RaylibRenderer final : public Renderer {
public:
    [[nodiscard]] float frame_time() const noexcept override {
        return GetFrameTime();
    }

    void set_clip_planes(double near_plane, double far_plane) override {
        rlSetClipPlanes(near_plane, far_plane);
    }

    void begin_texture_mode(RenderTexture2D target) override {
        BeginTextureMode(target);
    }

    void end_texture_mode() override {
        EndTextureMode();
    }

    void begin_drawing() override {
        BeginDrawing();
    }

    void end_drawing() override {
        EndDrawing();
    }

    void clear_background(Color color) override {
        ClearBackground(color);
    }

    void draw_texture_pro(Texture2D texture,
                          const Rectangle& source,
                          const Rectangle& destination,
                          const Vector2& origin,
                          float rotation,
                          Color tint) override {
        DrawTexturePro(texture, source, destination, origin, rotation, tint);
    }

    void begin_mode_3d(const Camera3D& camera) override {
        BeginMode3D(camera);
    }

    void end_mode_3d() override {
        EndMode3D();
    }

    void draw_line_3d(const Vector3& start, const Vector3& end, Color color) override {
        DrawLine3D(start, end, color);
    }

    void draw_triangle_3d(const Vector3& a,
                          const Vector3& b,
                          const Vector3& c,
                          Color color) override {
        DrawTriangle3D(a, b, c, color);
    }

};

}  // namespace

Renderer& raylib_renderer() {
    static RaylibRenderer instance;
    return instance;
}

Renderer& renderer() {
#if defined(SHAPESHIFTER_BACKEND_SDL2)
    return sdl2_renderer();
#else
    return raylib_renderer();
#endif
}

}  // namespace platform::graphics
