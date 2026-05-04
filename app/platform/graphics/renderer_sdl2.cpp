#include "renderer.h"
#include "../sdl2/sdl2_graphics_context.h"

#if !defined(__EMSCRIPTEN__)
#include "../sdl2/sdl2_headers.h"
#include "platform/runtime_api.h"
#endif

namespace platform::graphics {

namespace {

struct ValidationState {
    RendererValidationCounters counters{};
    bool frame_time_override_enabled = false;
    float frame_time_override_seconds = 0.0f;
};

ValidationState& validation_state() {
    static ValidationState state;
    return state;
}

class Sdl2Renderer final : public Renderer {
public:
    [[nodiscard]] float frame_time() const noexcept override {
        auto& state = validation_state();
        ++state.counters.frame_time_calls;
        if (state.frame_time_override_enabled) {
            return state.frame_time_override_seconds;
        }
        return platform::sdl2::consume_frame_time();
    }

    void set_clip_planes(double near_plane, double far_plane) override {
        auto& counters = validation_state().counters;
        ++counters.set_clip_planes_calls;
        near_plane_ = static_cast<float>(near_plane);
        far_plane_ = static_cast<float>(far_plane);
    }

    void begin_texture_mode(RenderTexture2D /*target*/) override {
        ++validation_state().counters.begin_texture_mode_calls;
    }
    void end_texture_mode() override {
        ++validation_state().counters.end_texture_mode_calls;
    }

    void begin_drawing() override {
        auto& counters = validation_state().counters;
        ++counters.begin_drawing_calls;
#if !defined(__EMSCRIPTEN__)
        if (!platform::sdl2::is_ready()) {
            ++counters.begin_drawing_skipped_not_ready;
            return;
        }
        glViewport(0, 0, platform::sdl2::screen_width(), platform::sdl2::screen_height());
#endif
    }

    void end_drawing() override {
        auto& counters = validation_state().counters;
        ++counters.end_drawing_calls;
        ++counters.swap_window_calls;
        platform::sdl2::swap_window();
    }

    void clear_background(Color color) override {
        auto& counters = validation_state().counters;
        ++counters.clear_background_calls;
        counters.last_clear_background = color;
#if !defined(__EMSCRIPTEN__)
        if (!platform::sdl2::is_ready()) {
            ++counters.clear_background_skipped_not_ready;
            return;
        }
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
                          Color /*tint*/) override {
        ++validation_state().counters.draw_texture_pro_calls;
    }

    void begin_mode_3d(const Camera3D& camera) override {
        auto& counters = validation_state().counters;
        ++counters.begin_mode_3d_calls;
#if !defined(__EMSCRIPTEN__)
        if (!platform::sdl2::is_ready()) return;
        const int width = platform::sdl2::screen_width();
        const int height = platform::sdl2::screen_height();
        if (width <= 0 || height <= 0) return;

        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        Matrix projection = {};
        if (camera.projection == CAMERA_ORTHOGRAPHIC) {
            const float top = camera.fovy * 0.5f;
            const float right = top * aspect;
            projection = MatrixOrtho(-right, right, -top, top, near_plane_, far_plane_);
        } else {
            projection = MatrixPerspective(camera.fovy * DEG2RAD, aspect, near_plane_, far_plane_);
        }
        const Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);

        glEnable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(MatrixToFloat(projection));
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(MatrixToFloat(view));
#else
        (void)camera;
#endif
    }

    void end_mode_3d() override {
        auto& counters = validation_state().counters;
        ++counters.end_mode_3d_calls;
#if !defined(__EMSCRIPTEN__)
        if (!platform::sdl2::is_ready()) return;
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
#endif
    }

    void draw_line_3d(const Vector3& start, const Vector3& end, Color color) override {
        auto& counters = validation_state().counters;
        ++counters.draw_line_3d_calls;
#if !defined(__EMSCRIPTEN__)
        if (!platform::sdl2::is_ready()) return;
        glColor4ub(color.r, color.g, color.b, color.a);
        glBegin(GL_LINES);
        glVertex3f(start.x, start.y, start.z);
        glVertex3f(end.x, end.y, end.z);
        glEnd();
#else
        (void)start;
        (void)end;
        (void)color;
#endif
    }

    void draw_triangle_3d(const Vector3& a,
                          const Vector3& b,
                          const Vector3& c,
                          Color color) override {
        auto& counters = validation_state().counters;
        ++counters.draw_triangle_3d_calls;
#if !defined(__EMSCRIPTEN__)
        if (!platform::sdl2::is_ready()) return;
        glColor4ub(color.r, color.g, color.b, color.a);
        glBegin(GL_TRIANGLES);
        glVertex3f(a.x, a.y, a.z);
        glVertex3f(b.x, b.y, b.z);
        glVertex3f(c.x, c.y, c.z);
        glEnd();
#else
        (void)a;
        (void)b;
        (void)c;
        (void)color;
#endif
    }

private:
    float near_plane_ = 1.0f;
    float far_plane_ = 5000.0f;
};

}  // namespace

Renderer& sdl2_renderer() {
    static Sdl2Renderer instance;
    return instance;
}

Renderer& renderer() {
    return sdl2_renderer();
}

void reset_renderer_validation_counters() noexcept {
    auto& state = validation_state();
    state.counters = RendererValidationCounters{};
}

[[nodiscard]] RendererValidationCounters renderer_validation_counters() noexcept {
    return validation_state().counters;
}

void set_renderer_frame_time_override(float frame_time_seconds) noexcept {
    auto& state = validation_state();
    state.frame_time_override_enabled = true;
    state.frame_time_override_seconds = frame_time_seconds;
}

void clear_renderer_frame_time_override() noexcept {
    auto& state = validation_state();
    state.frame_time_override_enabled = false;
    state.frame_time_override_seconds = 0.0f;
}

}  // namespace platform::graphics
