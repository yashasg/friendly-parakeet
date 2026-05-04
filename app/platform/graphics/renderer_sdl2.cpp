#include "renderer.h"
#include "../sdl2/sdl2_graphics_context.h"

#if !defined(__EMSCRIPTEN__)
#include "../sdl2/sdl2_headers.h"
#endif

namespace platform::graphics {

namespace {

class Sdl2Renderer final : public Renderer {
public:
    [[nodiscard]] float frame_time() const noexcept override {
        return platform::sdl2::consume_frame_time();
    }

    void begin_texture_mode(RenderTexture2D /*target*/) override {}
    void end_texture_mode() override {}

    void begin_drawing() override {
#if !defined(__EMSCRIPTEN__)
        if (!platform::sdl2::is_ready()) return;
        glViewport(0, 0, platform::sdl2::screen_width(), platform::sdl2::screen_height());
#endif
    }

    void end_drawing() override {
        platform::sdl2::swap_window();
    }

    void clear_background(Color color) override {
#if !defined(__EMSCRIPTEN__)
        if (!platform::sdl2::is_ready()) return;
        constexpr float kColorScale = 1.0f / 255.0f;
        glClearColor(static_cast<float>(color.r) * kColorScale,
                     static_cast<float>(color.g) * kColorScale,
                     static_cast<float>(color.b) * kColorScale,
                     static_cast<float>(color.a) * kColorScale);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#else
        (void)color;
#endif
    }

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

Renderer& sdl2_renderer() {
    static Sdl2Renderer instance;
    return instance;
}

}  // namespace platform::graphics
