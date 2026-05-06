// SDL2-backed render API implementation.
// Provides software 3D rendering (world→screen projection) via SDL_RenderGeometry
// and SDL_RenderDrawLineF, plus CPU-side mesh generators using glm math.

#include "render_api.h"
#include "platform_state.h"
#include "../constants.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <SDL.h>

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <optional>

namespace render_api {

// ── Module-level state ────────────────────────────────────────────────────────

namespace {

struct State3D {
    glm::mat4 view_proj{1.0f};
    bool active = false;
};

struct GlobalState {
    State3D mode3d;
    RendererValidationCounters counters;
    std::optional<float> frame_time_override;
};

GlobalState& global_state() noexcept {
    static GlobalState s;
    return s;
}

// Project a world-space point to screen space.
// Returns false (sets out to zero) if the point is behind the camera.
bool project_to_screen(const glm::vec3& world,
                       const glm::mat4& view_proj,
                       float& out_x, float& out_y) noexcept {
    const glm::vec4 clip = view_proj * glm::vec4(world, 1.0f);
    if (clip.w <= 0.0f) {
        out_x = 0.0f;
        out_y = 0.0f;
        return false;
    }
    const float inv_w = 1.0f / clip.w;
    const float ndc_x = clip.x * inv_w;
    const float ndc_y = clip.y * inv_w;
    out_x = (ndc_x + 1.0f) * 0.5f * static_cast<float>(constants::SCREEN_W);
    out_y = (1.0f - ndc_y) * 0.5f * static_cast<float>(constants::SCREEN_H);
    return true;
}

}  // namespace

// ── Validation counters ───────────────────────────────────────────────────────

void reset_renderer_validation_counters() noexcept {
    global_state().counters = RendererValidationCounters{};
}

RendererValidationCounters renderer_validation_counters() noexcept {
    return global_state().counters;
}

void set_renderer_frame_time_override(float dt) noexcept {
    global_state().frame_time_override = dt;
}

void clear_renderer_frame_time_override() noexcept {
    global_state().frame_time_override = std::nullopt;
}

float frame_time() noexcept {
    ++global_state().counters.frame_time_calls;
    if (global_state().frame_time_override) {
        return *global_state().frame_time_override;
    }
    return platform_state::consume_frame_time();
}

// ── Frame lifecycle ───────────────────────────────────────────────────────────

void begin_texture_mode(const RenderTexture2D& target) noexcept {
    ++global_state().counters.begin_texture_mode_calls;
    SDL_Renderer* r = platform_state::renderer();
    if (r) {
        SDL_SetRenderTarget(r, target.texture);
    }
}

void end_texture_mode() noexcept {
    ++global_state().counters.end_texture_mode_calls;
    SDL_Renderer* r = platform_state::renderer();
    if (r) {
        SDL_SetRenderTarget(r, nullptr);
    }
}

void begin_drawing() noexcept {
    auto& s = global_state();
    ++s.counters.begin_drawing_calls;
    SDL_Renderer* r = platform_state::renderer();
    if (!r) {
        ++s.counters.begin_drawing_skipped_not_ready;
    }
}

void end_drawing() noexcept {
    ++global_state().counters.end_drawing_calls;
    SDL_Renderer* r = platform_state::renderer();
    if (r) {
        ++global_state().counters.swap_window_calls;
        SDL_RenderPresent(r);
    }
}

void clear_background(SDL_Color color) noexcept {
    auto& s = global_state();
    ++s.counters.clear_background_calls;
    s.counters.last_clear_background = color;
    SDL_Renderer* r = platform_state::renderer();
    if (!r) {
        ++s.counters.clear_background_skipped_not_ready;
        return;
    }
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_RenderClear(r);
}

// ── 2D draw ───────────────────────────────────────────────────────────────────

void draw_texture_pro(const Texture2D& texture,
                      const Rectangle& src, const Rectangle& dst,
                      glm::vec2 origin, float rotation,
                      SDL_Color tint) noexcept {
    ++global_state().counters.draw_texture_pro_calls;
    SDL_Renderer* r = platform_state::renderer();
    if (!r || !texture.texture) return;
    const SDL_Rect s_rect{
        static_cast<int>(src.x), static_cast<int>(src.y),
        static_cast<int>(src.width), static_cast<int>(src.height)};
    const SDL_FRect d_rect{dst.x - origin.x, dst.y - origin.y, dst.width, dst.height};
    SDL_SetTextureColorMod(texture.texture, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(texture.texture, tint.a);
    SDL_RenderCopyExF(r, texture.texture,
                      (src.width > 0.0f ? &s_rect : nullptr),
                      &d_rect,
                      static_cast<double>(rotation),
                      nullptr, SDL_FLIP_NONE);
}

void draw_rectangle_rec(const Rectangle& rect, SDL_Color color) noexcept {
    SDL_Renderer* r = platform_state::renderer();
    if (!r) return;
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    const SDL_FRect fr{rect.x, rect.y, rect.width, rect.height};
    SDL_RenderFillRectF(r, &fr);
}

void set_texture_filter(const Texture2D& /*texture*/,
                        TextureFilter /*filter*/) noexcept {
    // SDL_Renderer has no per-texture filter API; no-op.
}

// ── 3D mode ───────────────────────────────────────────────────────────────────

void begin_mode_3d(const Camera3D& camera,
                   const CameraClipPlanes& clip) noexcept {
    ++global_state().counters.begin_mode_3d_calls;

    auto& s = global_state().mode3d;
    s.active = true;

    const float near_p = clip.near_plane > 0.001f ? clip.near_plane : 0.001f;
    const float far_p  = clip.far_plane > near_p + 0.001f
                             ? clip.far_plane
                             : near_p + 0.001f;

    glm::mat4 proj;
    if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        constexpr float sw = static_cast<float>(constants::SCREEN_W);
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        const float aspect = sw / sh;
        const float top    = camera.fovy * 0.5f;
        const float right  = top * aspect;
        proj = glm::ortho(-right, right, -top, top, near_p, far_p);
    } else {
        constexpr float sw = static_cast<float>(constants::SCREEN_W);
        constexpr float sh = static_cast<float>(constants::SCREEN_H);
        const float aspect = sw / sh;
        proj = glm::perspective(glm::radians(camera.fovy), aspect, near_p, far_p);
    }

    const glm::mat4 view = glm::lookAt(camera.position, camera.target, camera.up);
    s.view_proj = proj * view;
}

void end_mode_3d() noexcept {
    ++global_state().counters.end_mode_3d_calls;
    global_state().mode3d.active = false;
}

// ── 3D primitives ─────────────────────────────────────────────────────────────

void draw_triangle_3d(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                      SDL_Color color) noexcept {
    ++global_state().counters.draw_triangle_3d_calls;
    SDL_Renderer* r = platform_state::renderer();
    if (!r || !global_state().mode3d.active) return;

    const glm::mat4& vp = global_state().mode3d.view_proj;
    float sx1, sy1, sx2, sy2, sx3, sy3;
    if (!project_to_screen(v1, vp, sx1, sy1)) return;
    if (!project_to_screen(v2, vp, sx2, sy2)) return;
    if (!project_to_screen(v3, vp, sx3, sy3)) return;

    SDL_Vertex verts[3] = {
        {{sx1, sy1}, {color.r, color.g, color.b, color.a}, {0.0f, 0.0f}},
        {{sx2, sy2}, {color.r, color.g, color.b, color.a}, {0.0f, 0.0f}},
        {{sx3, sy3}, {color.r, color.g, color.b, color.a}, {0.0f, 0.0f}},
    };
    SDL_RenderGeometry(r, nullptr, verts, 3, nullptr, 0);
}

void draw_line_3d(glm::vec3 start, glm::vec3 end, SDL_Color color) noexcept {
    ++global_state().counters.draw_line_3d_calls;
    SDL_Renderer* r = platform_state::renderer();
    if (!r || !global_state().mode3d.active) return;

    const glm::mat4& vp = global_state().mode3d.view_proj;
    float sx1, sy1, sx2, sy2;
    if (!project_to_screen(start, vp, sx1, sy1)) return;
    if (!project_to_screen(end,   vp, sx2, sy2)) return;

    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLineF(r, sx1, sy1, sx2, sy2);
}

// ── CPU-side mesh generators ──────────────────────────────────────────────────

// Emit a quad (2 triangles) as 6 sequential vertices into the flat array.
// p0-p3 are the four corners in CCW order.
static void emit_quad(float* verts, int& vi,
                      glm::vec3 p0, glm::vec3 p1,
                      glm::vec3 p2, glm::vec3 p3) noexcept {
    const auto emit = [&](glm::vec3 p) noexcept {
        verts[vi++] = p.x;
        verts[vi++] = p.y;
        verts[vi++] = p.z;
    };
    emit(p0); emit(p1); emit(p2);  // tri 1
    emit(p0); emit(p2); emit(p3);  // tri 2
}

Mesh gen_mesh_cube(float w, float h, float d) noexcept {
    // 6 faces × 2 triangles × 3 vertices = 36 vertices, 12 triangles.
    constexpr int kTriCount = 12;
    constexpr int kVertCount = 36;
    auto* verts = static_cast<float*>(std::malloc(sizeof(float) * kVertCount * 3));
    if (!verts) return {};

    const float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;
    int vi = 0;

    // +X face
    emit_quad(verts, vi, {hw,-hh,-hd},{hw,-hh, hd},{hw, hh, hd},{hw, hh,-hd});
    // -X face
    emit_quad(verts, vi, {-hw,-hh, hd},{-hw,-hh,-hd},{-hw, hh,-hd},{-hw, hh, hd});
    // +Y face
    emit_quad(verts, vi, {-hw, hh,-hd},{hw, hh,-hd},{hw, hh, hd},{-hw, hh, hd});
    // -Y face
    emit_quad(verts, vi, {-hw,-hh, hd},{hw,-hh, hd},{hw,-hh,-hd},{-hw,-hh,-hd});
    // +Z face
    emit_quad(verts, vi, {-hw,-hh, hd},{-hw, hh, hd},{hw, hh, hd},{hw,-hh, hd});
    // -Z face
    emit_quad(verts, vi, {hw,-hh,-hd},{hw, hh,-hd},{-hw, hh,-hd},{-hw,-hh,-hd});

    Mesh mesh;
    mesh.vertices     = verts;
    mesh.vertexCount  = kVertCount;
    mesh.triangleCount = kTriCount;
    return mesh;
}

Mesh gen_mesh_cylinder(float radius, float height, int slices) noexcept {
    if (slices < 3) slices = 3;
    // Top fan: slices triangles
    // Bottom fan: slices triangles
    // Side: slices × 2 triangles = 2*slices triangles
    const int tri_count  = slices * 4;
    const int vert_count = tri_count * 3;
    auto* verts = static_cast<float*>(std::malloc(sizeof(float) * vert_count * 3));
    if (!verts) return {};

    const float hh   = height * 0.5f;
    const float step = glm::two_pi<float>() / static_cast<float>(slices);
    int vi = 0;

    const auto emit = [&](glm::vec3 p) noexcept {
        verts[vi++] = p.x;
        verts[vi++] = p.y;
        verts[vi++] = p.z;
    };

    const glm::vec3 top_c{0.0f,  hh, 0.0f};
    const glm::vec3 bot_c{0.0f, -hh, 0.0f};

    for (int i = 0; i < slices; ++i) {
        const float a0 = step * static_cast<float>(i);
        const float a1 = step * static_cast<float>(i + 1);
        const float c0 = std::cos(a0), s0 = std::sin(a0);
        const float c1 = std::cos(a1), s1 = std::sin(a1);

        const glm::vec3 t0{radius*c0,  hh, radius*s0};
        const glm::vec3 t1{radius*c1,  hh, radius*s1};
        const glm::vec3 b0{radius*c0, -hh, radius*s0};
        const glm::vec3 b1{radius*c1, -hh, radius*s1};

        // Top fan
        emit(top_c); emit(t0); emit(t1);
        // Bottom fan (reversed winding)
        emit(bot_c); emit(b1); emit(b0);
        // Side quad (2 tris)
        emit(t0); emit(b0); emit(t1);
        emit(t1); emit(b0); emit(b1);
    }

    Mesh mesh;
    mesh.vertices      = verts;
    mesh.vertexCount   = vert_count;
    mesh.triangleCount = tri_count;
    return mesh;
}

Mesh gen_mesh_plane(float w, float h, int /*res_x*/, int /*res_z*/) noexcept {
    // 1 plane = 2 triangles = 6 vertices (XZ plane, y=0)
    constexpr int kTriCount  = 2;
    constexpr int kVertCount = 6;
    auto* verts = static_cast<float*>(std::malloc(sizeof(float) * kVertCount * 3));
    if (!verts) return {};

    const float hw = w * 0.5f, hh = h * 0.5f;
    int vi = 0;
    emit_quad(verts, vi,
              {-hw, 0.0f, -hh},
              {-hw, 0.0f,  hh},
              { hw, 0.0f,  hh},
              { hw, 0.0f, -hh});

    Mesh mesh;
    mesh.vertices      = verts;
    mesh.vertexCount   = kVertCount;
    mesh.triangleCount = kTriCount;
    return mesh;
}

// ── Model lifecycle ───────────────────────────────────────────────────────────

void unload_model(Model& model) noexcept {
    for (int i = 0; i < model.meshCount; ++i) {
        free_mesh_data(model.meshes[i]);
    }
    std::free(model.meshes);     model.meshes        = nullptr;
    std::free(model.materials);  model.materials     = nullptr;
    std::free(model.meshMaterial); model.meshMaterial = nullptr;
    model.meshCount     = 0;
    model.materialCount = 0;
}

}  // namespace render_api
