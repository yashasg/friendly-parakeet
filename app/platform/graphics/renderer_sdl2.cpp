#include "renderer.h"

namespace platform::graphics {

namespace {

class Sdl2RendererStub final : public Renderer {
public:
    [[nodiscard]] float frame_time() const noexcept override {
        return 0.0f;
    }

    void begin_texture_mode(RenderTexture2D /*target*/) override {}
    void end_texture_mode() override {}
    void begin_drawing() override {}
    void end_drawing() override {}
    void clear_background(Color /*color*/) override {}

    void draw_texture_pro(Texture2D /*texture*/,
                          const Rectangle& /*source*/,
                          const Rectangle& /*destination*/,
                          const Vector2& /*origin*/,
                          float /*rotation*/,
                          Color /*tint*/) override {}

    void begin_mode_3d(const Camera3D& /*camera*/) override {}
    void end_mode_3d() override {}
};

}  // namespace

Renderer& sdl2_renderer_stub() {
    static Sdl2RendererStub instance;
    return instance;
}

}  // namespace platform::graphics
