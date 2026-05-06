#pragma once
// SDL2-backed render API — direct SDL2/glm, no Raylib compat layer.
//
// 3D draw calls (draw_triangle_3d, draw_line_3d) project world-space
// coordinates using the current view-projection matrix (set by begin_mode_3d)
// and rasterise via SDL_RenderGeometry / SDL_RenderDrawLineF.
//
// Validation counters (reset_renderer_validation_counters, etc.) support
// headless testing without a real GPU context.

#include "../util/render_types.h"
#include "../entities/camera_entity.h"   // Camera3D
#include "../components/transform.h"     // CameraClipPlanes
#include <SDL.h>

namespace render_api {

// ── Renderer validation/testing support ──────────────────────────────────────
struct RendererValidationCounters {
    int       begin_texture_mode_calls           = 0;
    int       end_texture_mode_calls             = 0;
    int       begin_drawing_calls                = 0;
    int       clear_background_calls             = 0;
    int       draw_texture_pro_calls             = 0;
    int       begin_mode_3d_calls                = 0;
    int       draw_triangle_3d_calls             = 0;
    int       draw_line_3d_calls                 = 0;
    int       end_mode_3d_calls                  = 0;
    int       end_drawing_calls                  = 0;
    int       swap_window_calls                  = 0;
    int       begin_drawing_skipped_not_ready    = 0;
    int       clear_background_skipped_not_ready = 0;
    SDL_Color last_clear_background              = {0, 0, 0, 0};
    int       frame_time_calls                   = 0;
};

void reset_renderer_validation_counters() noexcept;
[[nodiscard]] RendererValidationCounters renderer_validation_counters() noexcept;

void set_renderer_frame_time_override(float dt) noexcept;
void clear_renderer_frame_time_override() noexcept;
[[nodiscard]] float frame_time() noexcept;

// ── Frame lifecycle ───────────────────────────────────────────────────────────
void begin_texture_mode(const RenderTexture2D& target) noexcept;
void end_texture_mode() noexcept;
void begin_drawing() noexcept;
void end_drawing() noexcept;
void clear_background(SDL_Color color) noexcept;

// ── 2D draw ───────────────────────────────────────────────────────────────────
void draw_texture_pro(const Texture2D& texture,
                      const Rectangle& src, const Rectangle& dst,
                      glm::vec2 origin, float rotation,
                      SDL_Color tint) noexcept;
void draw_rectangle_rec(const Rectangle& rect, SDL_Color color) noexcept;

// ── Texture utility ───────────────────────────────────────────────────────────
// No-op in SDL2_Renderer path (no per-texture filter state on SDL_Texture).
void set_texture_filter(const Texture2D& texture, TextureFilter filter) noexcept;

// ── 3D mode ───────────────────────────────────────────────────────────────────
// Computes the view-projection matrix from camera/clip params and enters 3D mode.
// All subsequent draw_triangle_3d / draw_line_3d calls project using this matrix.
void begin_mode_3d(const Camera3D& camera,
                   const CameraClipPlanes& clip) noexcept;
void end_mode_3d() noexcept;

// ── 3D primitives (project → rasterise via SDL2) ─────────────────────────────
void draw_triangle_3d(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                      SDL_Color color) noexcept;
void draw_line_3d(glm::vec3 start, glm::vec3 end, SDL_Color color) noexcept;

// ── CPU-side mesh generators ─────────────────────────────────────────────────
// Return Mesh objects with heap-allocated vertex arrays.
// Caller owns memory; free with free_mesh_data() or unload_model().
[[nodiscard]] Mesh gen_mesh_cube(float w, float h, float d) noexcept;
[[nodiscard]] Mesh gen_mesh_cylinder(float radius, float height, int slices) noexcept;
[[nodiscard]] Mesh gen_mesh_plane(float w, float h, int res_x, int res_z) noexcept;

// ── Material / shader helpers ─────────────────────────────────────────────────
// Return no-op structs — SDL2 renderer has no GPU material/shader state.
[[nodiscard]] inline Material load_material_default() noexcept { return {}; }
[[nodiscard]] inline Shader   load_shader_from_memory(const char* /*vs*/,
                                                       const char* /*fs*/) noexcept { return {}; }

// ── Model lifecycle ───────────────────────────────────────────────────────────
// Frees mesh vertex data and model arrays (meshes[], materials[], meshMaterial[]).
void unload_model(Model& model) noexcept;

}  // namespace render_api
