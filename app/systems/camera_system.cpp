#include "camera_system.h"
#include "render_api.h"
#include "../components/shape_mesh.h"
#include "../components/transform.h"
#include "../components/render_tags.h"
#include "../components/player.h"
#include "../components/obstacle.h"
#include "../components/particle.h"
#include "../components/scoring.h"
#include "../components/song_state.h"
#include "../components/registry_context.h"
#include "../entities/camera_entity.h"
#include "../constants.h"
#include "../systems/platform_state.h"
#include "render_api.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

struct CameraMatrices {
    glm::mat4 projection{1.0f};
    glm::mat4 view{1.0f};
};

CameraClipPlanes sanitize_clip_planes(const CameraClipPlanes& clip_planes) {
    constexpr float kMinNearPlane = 0.001f;
    constexpr float kMinClipSpan = 0.001f;
    const float sanitized_near =
        clip_planes.near_plane >= kMinNearPlane ? clip_planes.near_plane : kMinNearPlane;
    const float sanitized_far = clip_planes.far_plane > sanitized_near + kMinClipSpan
                                    ? clip_planes.far_plane
                                    : sanitized_near + kMinClipSpan;
    return {.near_plane = sanitized_near, .far_plane = sanitized_far};
}

glm::mat4 build_projection_matrix(const Camera3D& camera,
                                  float aspect,
                                  float near_plane,
                                  float far_plane) {
    if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        const float top = camera.fovy * 0.5f;
        const float right = top * aspect;
        return glm::ortho(-right, right, -top, top, near_plane, far_plane);
    }
    return glm::perspective(glm::radians(camera.fovy), aspect, near_plane, far_plane);
}

CameraMatrices build_camera_matrices(const Camera3D& camera,
                                     int width,
                                     int height,
                                     const CameraClipPlanes& clip_planes) {
    CameraMatrices matrices{};
    if (width <= 0 || height <= 0) return matrices;
    const CameraClipPlanes sanitized = sanitize_clip_planes(clip_planes);
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    matrices.projection =
        build_projection_matrix(camera, aspect, sanitized.near_plane, sanitized.far_plane);
    matrices.view = glm::lookAt(camera.position, camera.target, camera.up);
    return matrices;
}

glm::vec2 world_to_screen(const glm::vec3& position,
                          const CameraMatrices& matrices,
                          int width,
                          int height) {
    if (width <= 0 || height <= 0) return {};
    const glm::vec4 clip = matrices.projection * matrices.view * glm::vec4{position, 1.0f};
    if (clip.w == 0.0f) return {};

    const float inv_w = 1.0f / clip.w;
    const float ndc_x = clip.x * inv_w;
    const float ndc_y = clip.y * inv_w;
    return {
        (ndc_x + 1.0f) * 0.5f * static_cast<float>(width),
        (1.0f - ndc_y) * 0.5f * static_cast<float>(height),
    };
}

}  // namespace

namespace camera {

static ShapeMeshes build_shape_meshes(const ShapeMeshConfig& config) {
    ShapeMeshes sm = {};

    sm.shapes[0] = render_api::gen_mesh_cylinder(1.0f, config.props[0].height_ratio, 12);
    sm.shapes[1] = render_api::gen_mesh_cube(2.0f, config.props[1].height_ratio, 2.0f);
    sm.shapes[2] = render_api::gen_mesh_cylinder(1.0f, config.props[2].height_ratio, 3);
    sm.shapes[3] = render_api::gen_mesh_cylinder(1.0f, config.props[3].height_ratio, 6);
    sm.slab = render_api::gen_mesh_cube(1.0f, 1.0f, 1.0f);
    sm.quad = render_api::gen_mesh_plane(1.0f, 1.0f, 1, 1);

    sm.owned = true;
    return sm;
}

void init(entt::registry& reg) {
    ensure_game_camera(reg);

    RenderTargets targets{};
    if (SDL_Renderer* renderer = platform_state::renderer(); renderer != nullptr) {
        if (SDL_RenderTargetSupported(renderer) == SDL_TRUE) {
            SDL_Texture* world = SDL_CreateTexture(renderer,
                                                   SDL_PIXELFORMAT_RGBA8888,
                                                   SDL_TEXTUREACCESS_TARGET,
                                                   constants::SCREEN_W,
                                                   constants::SCREEN_H);
            SDL_Texture* ui = SDL_CreateTexture(renderer,
                                                SDL_PIXELFORMAT_RGBA8888,
                                                SDL_TEXTUREACCESS_TARGET,
                                                constants::SCREEN_W,
                                                constants::SCREEN_H);
            if (world != nullptr && ui != nullptr) {
                SDL_SetTextureBlendMode(world, SDL_BLENDMODE_BLEND);
                SDL_SetTextureBlendMode(ui, SDL_BLENDMODE_BLEND);
                targets = RenderTargets{world, ui};
            } else {
                if (world != nullptr) SDL_DestroyTexture(world);
                if (ui != nullptr) SDL_DestroyTexture(ui);
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Render target creation failed: %s",
                            SDL_GetError());
            }
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Renderer does not support target textures; using direct backbuffer path");
        }
    }
    registry_ctx_insert_or_assign(reg, std::move(targets));
    registry_ctx_insert_or_assign(reg, ScreenTransform{});
    registry_ctx_insert_or_assign(reg, CameraClipPlanes{});
    registry_ctx_insert_or_assign(reg, FloorParams{});
    const auto& mesh_config = registry_ctx_get_or_emplace<ShapeMeshConfig>(reg);
    registry_ctx_insert_or_assign(reg, build_shape_meshes(mesh_config));
}

void shutdown(entt::registry& reg) {
    // Belt-and-suspenders: release early before CloseWindow().
    // The RAII destructors on the ctx objects will no-op because
    // release() clears the owned flag.
    if (auto* meshes = registry_ctx_find<ShapeMeshes>(reg)) {
        meshes->release();
    }
    if (auto* targets = registry_ctx_find<RenderTargets>(reg)) {
        targets->release();
    }
}

} // namespace camera

// ── Helpers ─────────────────────────────────────────────────────────────────

// GenMeshCube is centered at origin. Translate to position the slab's
// bottom-left corner at (x, 0) and center depth on z so z is the beat/timing plane.
static glm::mat4 slab_matrix(float x, float z, float w, float h, float d) {
    const glm::mat4 matrix = glm::scale(glm::mat4(1.0f), glm::vec3{w, h, d}) *
                             glm::translate(glm::mat4(1.0f), glm::vec3{x + w / 2.0f, h / 2.0f, z});
    return matrix;
}

static glm::mat4 shape_matrix(float cx, float y_3d, float cz, float sz, float radius_scale) {
    const float s = sz * radius_scale;
    const glm::mat4 matrix = glm::scale(glm::mat4(1.0f), glm::vec3{s, s, s}) *
                             glm::translate(glm::mat4(1.0f), glm::vec3{cx, y_3d, cz});
    return matrix;
}

// Triangular prism rotated so one vertex of the cross-section points up (△)
static glm::mat4 prism_matrix(float cx, float y_3d, float cz, float sz, float radius_scale) {
    const float s = sz * radius_scale;
    const glm::mat4 matrix = glm::scale(glm::mat4(1.0f), glm::vec3{s, s, s}) *
                              glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3{0.0f, 1.0f, 0.0f}) *
                              glm::translate(glm::mat4(1.0f), glm::vec3{cx, y_3d, cz});
    return matrix;
}

// Pick the correct matrix for a shape mesh (triangle prism needs rotation)
static glm::mat4 make_shape_matrix(uint8_t mesh_index, float cx, float y_3d, float cz,
                                   float sz, float radius_scale) {
    if (mesh_index == static_cast<uint8_t>(Shape::Triangle))
        return prism_matrix(cx, y_3d, cz, sz, radius_scale);
    return shape_matrix(cx, y_3d, cz, sz, radius_scale);
}

// ── game_camera_system: model-to-world transforms for all 3D renderables ────

void game_camera_system(entt::registry& reg, float /*dt*/) {
    const auto& mesh_config = registry_ctx_get<ShapeMeshConfig>(reg);

    // 1. Model-authority vertical bars: write scroll transform directly into
    //     ObstacleModel.model.transform. Do NOT emit ModelTransform for these —
    //     game_render_system draws them via the ObstacleModel + TagWorldPass path.
    {
        auto view = reg.view<ObstacleTag, ObstacleScrollZ, ObstacleModel, ObstacleParts>();
        for (auto [entity, oz, om, pd] : view.each()) {
            if (!om.owned) continue;  // headless: model not allocated, skip transform
            om.model.transform = slab_matrix(pd.cx, oz.z + pd.cz, pd.width, pd.height, pd.depth);
        }
    }

    // 2. MeshChild transforms (multi-slab obstacles, ghost shapes)
    {
        auto view = reg.view<MeshChild>();
        for (auto [entity, mc] : view.each()) {
            auto& parent_wt = reg.get<WorldTransform>(mc.parent);
            float z = parent_wt.position.y + mc.z_offset;

            if (mc.mesh_type == MeshType::Slab) {
                reg.get_or_emplace<ModelTransform>(entity) =
                    ModelTransform{slab_matrix(mc.x, z, mc.width, mc.height, mc.depth),
                                   mc.tint, 0, MeshType::Slab};
            } else {
                const auto& props = mesh_config.props[mc.mesh_index];
                reg.get_or_emplace<ModelTransform>(entity) =
                    ModelTransform{make_shape_matrix(mc.mesh_index, mc.x, 0.0f, z, mc.width, props.radius_scale),
                                   mc.tint, mc.mesh_index, MeshType::Shape};
            }
        }
    }

    // 3. Player shape transform
    {
        auto view = reg.view<PlayerTag, WorldTransform, PlayerShape, VerticalState, SDL_Color>();
        for (auto [entity, transform, pshape, vstate, col] : view.each()) {
            float y_3d = -vstate.y_offset;
            float sz = constants::PLAYER_SIZE;
            if (vstate.mode == VMode::Sliding) sz *= 0.5f;
            uint8_t shape_idx = static_cast<uint8_t>(pshape.current);
            const auto& props = mesh_config.props[shape_idx];
            reg.get_or_emplace<ModelTransform>(entity) =
                ModelTransform{make_shape_matrix(shape_idx, transform.position.x, y_3d,
                                                 transform.position.y, sz, props.radius_scale),
                               col, shape_idx, MeshType::Shape};
        }
    }

    // 4. Particle transforms
    {
        auto view = reg.view<ParticleTag, WorldTransform, ParticleData, SDL_Color>();
        for (auto [entity, transform, pdata, col] : view.each()) {
            float ratio = (pdata.max_time > 0.0f) ? (pdata.remaining / pdata.max_time) : 1.0f;
            float sz = pdata.size * ratio;
            float half = sz / 2.0f;
            const glm::mat4 matrix = glm::scale(glm::mat4(1.0f), glm::vec3{sz, 1.0f, sz}) *
                                     glm::translate(glm::mat4(1.0f),
                                                    glm::vec3{transform.position.x - half,
                                                              0.0f,
                                                              transform.position.y - half});
            reg.get_or_emplace<ModelTransform>(entity) =
                ModelTransform{matrix, col, 0, MeshType::Quad};
        }
    }

    // 5. Floor pulse params (SongState → FloorParams singleton)
    {
        auto& fp = registry_ctx_get<FloorParams>(reg);
        auto* song = registry_ctx_find<SongState>(reg);
        float pulse = 0.0f;
        if (song && song->playing && song->beat_period > 0.0f && song->current_beat >= 0) {
            const float time_since_beat = song->song_time - song->current_beat_time;
            float pulse_t = std::clamp(time_since_beat / constants::FLOOR_PULSE_DECAY, 0.0f, 1.0f);
            float ease = 1.0f - (1.0f - pulse_t) * (1.0f - pulse_t);
            pulse = 1.0f - ease;
        }
        float alpha_f = constants::FLOOR_ALPHA_REST +
                        (constants::FLOOR_ALPHA_PEAK - constants::FLOOR_ALPHA_REST) * pulse;
        float scale = constants::FLOOR_SCALE_REST +
                      (constants::FLOOR_SCALE_PEAK - constants::FLOOR_SCALE_REST) * pulse;
        fp.size  = constants::FLOOR_SHAPE_SIZE * scale;
        fp.half  = fp.size / 2.0f;
        fp.thick = constants::FLOOR_OUTLINE_THICK;
        fp.alpha = static_cast<uint8_t>(alpha_f);
    }
}

// ── compute_screen_transform: letterbox scale/offset from window size ────────

void compute_screen_transform(entt::registry& reg) {
    float win_w, win_h;
    platform_state::query_display_size(win_w, win_h);
    float scale = std::min(
        win_w / static_cast<float>(constants::SCREEN_W),
        win_h / static_cast<float>(constants::SCREEN_H));
    float dst_w = constants::SCREEN_W * scale;
    float dst_h = constants::SCREEN_H * scale;
    auto& st = registry_ctx_get<ScreenTransform>(reg);
    st.offset_x  = (win_w - dst_w) * 0.5f;
    st.offset_y  = (win_h - dst_h) * 0.5f;
    st.scale     = scale;
}

// ── ui_camera_system: screen-space transforms for UI layer ──────────────────

void ui_camera_system(entt::registry& reg, float /*dt*/) {
    // ScreenTransform is computed once per frame by game_loop_frame (before
    // input_system) and stored in the registry context.  Reading it here is
    // sufficient; do NOT call compute_screen_transform again (#241).

    // Popup screen-space projection
    {
        auto& cam = game_camera(reg).cam;
        const auto& clip_planes = registry_ctx_get<CameraClipPlanes>(reg);
        const CameraMatrices popup_camera_matrices = build_camera_matrices(
            cam,
            constants::SCREEN_W,
            constants::SCREEN_H,
            clip_planes
        );
        auto view = reg.view<ScorePopup, WorldTransform>();
        for (auto [entity, popup, transform] : view.each()) {
            (void)popup;
            const glm::vec3 world_pos = {transform.position.x, 5.0f, transform.position.y};
            const glm::vec2 sp = world_to_screen(
                world_pos,
                popup_camera_matrices,
                constants::SCREEN_W,
                constants::SCREEN_H
            );
            reg.get_or_emplace<ScreenPosition>(entity) =
                ScreenPosition{sp};
        }
    }
}
