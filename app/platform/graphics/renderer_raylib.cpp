#include "renderer.h"

namespace platform::graphics {

namespace {

class RaylibRenderer final : public Renderer {
public:
    [[nodiscard]] float frame_time() const noexcept override {
        return GetFrameTime();
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
};

}  // namespace

Renderer& raylib_renderer() {
    static RaylibRenderer instance;
    return instance;
}

Renderer& renderer() {
    return raylib_renderer();
}

}  // namespace platform::graphics
