#include "renderer_backend.h"
#include "../platform/sdl2/sdl2_graphics_context.h"
#include <cmath>
#include <utility>

#include "../platform/sdl2/sdl2_headers.h"
#include "runtime/runtime_api.h"

namespace platform::graphics {

namespace {

struct ValidationState {
    RendererValidationCounters counters{};
    bool frame_time_override_enabled = false;
    float frame_time_override_seconds = 0.0f;
    float near_plane = 1.0f;
    float far_plane = 5000.0f;
};

ValidationState& validation_state() {
    static ValidationState state;
    return state;
}

}  // namespace

[[nodiscard]] float frame_time() noexcept {
    auto& state = validation_state();
    ++state.counters.frame_time_calls;
    if (state.frame_time_override_enabled) {
        return state.frame_time_override_seconds;
    }
    return platform::sdl2::consume_frame_time();
}

void set_clip_planes(double near_plane, double far_plane) {
    auto& state = validation_state();
    ++state.counters.set_clip_planes_calls;
    state.near_plane = static_cast<float>(near_plane);
    state.far_plane = static_cast<float>(far_plane);
}

void begin_texture_mode(RenderTexture2D target) {
    ++validation_state().counters.begin_texture_mode_calls;
    if (!platform::sdl2::is_ready() || target.id == 0u) return;
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(target.id));
    glViewport(0, 0, target.texture.width, target.texture.height);
}

void end_texture_mode() {
    ++validation_state().counters.end_texture_mode_calls;
    if (!platform::sdl2::is_ready()) return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, platform::sdl2::screen_width(), platform::sdl2::screen_height());
}

void begin_drawing() {
    auto& counters = validation_state().counters;
    ++counters.begin_drawing_calls;
    if (!platform::sdl2::is_ready()) {
        ++counters.begin_drawing_skipped_not_ready;
        return;
    }
    glViewport(0, 0, platform::sdl2::screen_width(), platform::sdl2::screen_height());
}

void end_drawing() {
    auto& counters = validation_state().counters;
    ++counters.end_drawing_calls;
    ++counters.swap_window_calls;
    platform::sdl2::swap_window();
}

void clear_background(Color color) {
    auto& counters = validation_state().counters;
    ++counters.clear_background_calls;
    counters.last_clear_background = color;
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
}

void draw_texture_pro(Texture2D texture,
                      const Rectangle& source,
                      const Rectangle& destination,
                      const Vector2& origin,
                      float rotation,
                      Color tint) {
    ++validation_state().counters.draw_texture_pro_calls;
    if (!platform::sdl2::is_ready() || texture.id == 0u ||
        texture.width <= 0 || texture.height <= 0) {
        return;
    }

    const float inv_w = 1.0f / static_cast<float>(texture.width);
    const float inv_h = 1.0f / static_cast<float>(texture.height);

    float src_x = source.x;
    float src_y = source.y;
    float src_w = source.width;
    float src_h = source.height;
    bool flip_x = false;
    bool flip_y = false;

    if (src_w < 0.0f) {
        src_w = -src_w;
        flip_x = true;
    }
    if (src_h < 0.0f) {
        src_h = -src_h;
        flip_y = true;
    }

    float u0 = src_x * inv_w;
    float v0 = src_y * inv_h;
    float u1 = (src_x + src_w) * inv_w;
    float v1 = (src_y + src_h) * inv_h;
    if (flip_x) std::swap(u0, u1);
    if (flip_y) std::swap(v0, v1);

    const float rad = rotation * DEG2RAD;
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    const auto rotate = [&](float x, float y) {
        return Vector2{destination.x + origin.x + x * c - y * s,
                       destination.y + origin.y + x * s + y * c};
    };

    const Vector2 p0 = rotate(-origin.x, -origin.y);
    const Vector2 p1 = rotate(destination.width - origin.x, -origin.y);
    const Vector2 p2 = rotate(destination.width - origin.x, destination.height - origin.y);
    const Vector2 p3 = rotate(-origin.x, destination.height - origin.y);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int vp_w = viewport[2] > 0 ? viewport[2] : platform::sdl2::screen_width();
    const int vp_h = viewport[3] > 0 ? viewport[3] : platform::sdl2::screen_height();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(vp_w), static_cast<double>(vp_h), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture.id));
    glColor4ub(tint.r, tint.g, tint.b, tint.a);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0); glVertex2f(p0.x, p0.y);
    glTexCoord2f(u1, v0); glVertex2f(p1.x, p1.y);
    glTexCoord2f(u1, v1); glVertex2f(p2.x, p2.y);
    glTexCoord2f(u0, v1); glVertex2f(p3.x, p3.y);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

void begin_mode_3d(const Camera3D& camera) {
    auto& counters = validation_state().counters;
    ++counters.begin_mode_3d_calls;
    if (!platform::sdl2::is_ready()) return;
    const int width = platform::sdl2::screen_width();
    const int height = platform::sdl2::screen_height();
    if (width <= 0 || height <= 0) return;

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    Matrix projection = {};
    const auto& state = validation_state();
    if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        const float top = camera.fovy * 0.5f;
        const float right = top * aspect;
        projection = MatrixOrtho(-right, right, -top, top, state.near_plane, state.far_plane);
    } else {
        projection = MatrixPerspective(camera.fovy * DEG2RAD, aspect, state.near_plane, state.far_plane);
    }
    const Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);

    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(MatrixToFloat(projection));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(MatrixToFloat(view));
}

void end_mode_3d() {
    auto& counters = validation_state().counters;
    ++counters.end_mode_3d_calls;
    if (!platform::sdl2::is_ready()) return;
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
}

void draw_line_3d(const Vector3& start, const Vector3& end, Color color) {
    auto& counters = validation_state().counters;
    ++counters.draw_line_3d_calls;
    if (!platform::sdl2::is_ready()) return;
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_LINES);
    glVertex3f(start.x, start.y, start.z);
    glVertex3f(end.x, end.y, end.z);
    glEnd();
}

void draw_triangle_3d(const Vector3& a,
                      const Vector3& b,
                      const Vector3& c,
                      Color color) {
    auto& counters = validation_state().counters;
    ++counters.draw_triangle_3d_calls;
    if (!platform::sdl2::is_ready()) return;
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_TRIANGLES);
    glVertex3f(a.x, a.y, a.z);
    glVertex3f(b.x, b.y, b.z);
    glVertex3f(c.x, c.y, c.z);
    glEnd();
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
