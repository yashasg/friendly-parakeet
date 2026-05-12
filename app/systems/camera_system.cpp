#include "camera_system.h"
#include "all_systems.h"
#include "../components/shape_mesh.h"
#include "../components/rendering.h"
#include "../components/transform.h"
#include "../components/player.h"
#include "../components/obstacle.h"
#include "../entities/beat_map.h"
#include "../components/particle.h"
#include "../components/scoring.h"
#include "../components/song_state.h"
#include "../components/system_scratch.h"
#include "../constants.h"
#include "../entities/settings.h"
#include "../platform_display.h"
#include <glm/mat4x4.hpp>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>
#include <stdexcept>

namespace camera {

// GLSL 330 (desktop OpenGL 3.3) vs GLSL 100 (WebGL / OpenGL ES 2.0)
#ifdef PLATFORM_WEB
static const char* MESH_VS =
    "#version 100\n"
    "attribute vec3 vertexPosition;\n"
    "attribute vec3 vertexNormal;\n"
    "attribute vec2 vertexTexCoord;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matNormal;\n"
    "varying vec3 fragNormal;\n"
    "void main() {\n"
    "    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char* MESH_FS =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec3 fragNormal;\n"
    "uniform vec4 colDiffuse;\n"
    "void main() {\n"
    "    float shade = 0.35 + 0.65 * clamp(fragNormal.y, 0.0, 1.0);\n"
    "    gl_FragColor = vec4(colDiffuse.rgb * shade, colDiffuse.a);\n"
    "}\n";
#else
static const char* MESH_VS =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec3 vertexNormal;\n"
    "in vec2 vertexTexCoord;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matNormal;\n"
    "out vec3 fragNormal;\n"
    "void main() {\n"
    "    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char* MESH_FS =
    "#version 330\n"
    "in vec3 fragNormal;\n"
    "uniform vec4 colDiffuse;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    float shade = 0.35 + 0.65 * clamp(fragNormal.y, 0.0, 1.0);\n"
    "    finalColor = vec4(colDiffuse.rgb * shade, colDiffuse.a);\n"
    "}\n";
#endif

static ShapeMeshes build_shape_meshes(const ShapeMeshConfig& config) {
    ShapeMeshes sm = {};

    sm.shapes[0] = GenMeshCylinder(1.0f, config.props[0].height_ratio, 12);
    sm.shapes[1] = GenMeshCube(2.0f, config.props[1].height_ratio, 2.0f);
    sm.shapes[2] = GenMeshCylinder(1.0f, config.props[2].height_ratio, 3);
    sm.shapes[3] = GenMeshCylinder(1.0f, config.props[3].height_ratio, 6);
    sm.slab = GenMeshCube(1.0f, 1.0f, 1.0f);
    sm.quad = GenMeshPlane(1.0f, 1.0f, 1, 1);

    Shader shader = LoadShaderFromMemory(MESH_VS, MESH_FS);
    sm.material = LoadMaterialDefault();
    sm.material.shader = shader;

    sm.owned = true;
    return sm;
}

static void unload_shape_meshes(ShapeMeshes& sm) {
    for (int i = 0; i < 4; ++i)
        UnloadMesh(sm.shapes[i]);
    UnloadMesh(sm.slab);
    UnloadMesh(sm.quad);
    // raylib's UnloadMaterial() owns material.shader; unloading it separately
    // double-frees the shader's location array.
    UnloadMaterial(sm.material);
}

// ── ShapeMeshes RAII member definitions ─────────────────────────────────────

void ShapeMeshes::release() {
    if (!owned) return;
    unload_shape_meshes(*this);
    for (int i = 0; i < 4; ++i) shapes[i] = {};
    slab = {}; quad = {}; material = {};
    owned = false;
}

ShapeMeshes::~ShapeMeshes() { release(); }

ShapeMeshes::ShapeMeshes(ShapeMeshes&& o) noexcept
    : slab{o.slab}, quad{o.quad}, material{o.material}, owned{o.owned}
{
    for (int i = 0; i < 4; ++i) shapes[i] = o.shapes[i];
    o.owned = false;
}

ShapeMeshes& ShapeMeshes::operator=(ShapeMeshes&& o) noexcept {
    if (this != &o) {
        release();
        for (int i = 0; i < 4; ++i) shapes[i] = o.shapes[i];
        slab = o.slab; quad = o.quad; material = o.material;
        owned = o.owned;
        o.owned = false;
    }
    return *this;
}

void init(entt::registry& reg) {
    if (reg.ctx().find<RenderTargets>() || reg.ctx().find<ShapeMeshes>()
        || !reg.view<GameCamera>().empty() || !reg.view<UICamera>().empty()) {
        throw std::logic_error("camera::init called while camera resources already exist");
    }

    // 3D gameplay camera entity
    spawn_game_camera(reg);

    // 2D UI camera entity (identity — screen-space, no transform)
    spawn_ui_camera(reg);

    // Render targets
    RenderTexture2D world = LoadRenderTexture(
        constants::SCREEN_W, constants::SCREEN_H);
    SetTextureFilter(world.texture, TEXTURE_FILTER_BILINEAR);
    RenderTexture2D ui = LoadRenderTexture(
        constants::SCREEN_W, constants::SCREEN_H);
    SetTextureFilter(ui.texture, TEXTURE_FILTER_BILINEAR);
    // RAII: owned=true set by RenderTargets(w,u) ctor
    reg.ctx().emplace<RenderTargets>(world, ui);

    reg.ctx().emplace<ScreenTransform>();
    reg.ctx().emplace<FloorParams>();
    const auto& mesh_config = reg.ctx().emplace<ShapeMeshConfig>();
    reg.ctx().emplace<ShapeMeshes>(build_shape_meshes(mesh_config));
}

void shutdown(entt::registry& reg) {
    // Belt-and-suspenders: release early before CloseWindow().
    // The RAII destructors on the ctx objects will no-op because
    // release() clears the owned flag.
    if (auto* meshes = reg.ctx().find<ShapeMeshes>()) {
        meshes->release();
    }
    if (auto* targets = reg.ctx().find<RenderTargets>()) {
        targets->release();
    }
}

} // namespace camera

// ── RenderTargets RAII member definitions ────────────────────────────────────

void RenderTargets::release() {
    if (!owned) return;
    UnloadRenderTexture(world);
    UnloadRenderTexture(ui);
    world = {}; ui = {};
    owned = false;
}

RenderTargets::~RenderTargets() { release(); }

RenderTargets::RenderTargets(RenderTargets&& o) noexcept
    : world{o.world}, ui{o.ui}, owned{o.owned}
{
    o.owned = false;
}

RenderTargets& RenderTargets::operator=(RenderTargets&& o) noexcept {
    if (this != &o) {
        release();
        world = o.world; ui = o.ui;
        owned = o.owned;
        o.owned = false;
    }
    return *this;
}

// ── Helpers ─────────────────────────────────────────────────────────────────

static glm::mat4 raylib_to_glm_matrix_camera(const Matrix& m) {
    return glm::mat4{
        m.m0, m.m1, m.m2, m.m3,
        m.m4, m.m5, m.m6, m.m7,
        m.m8, m.m9, m.m10, m.m11,
        m.m12, m.m13, m.m14, m.m15
    };
}

// GenMeshCube is centered at origin. Translate to position the slab's
// bottom-left corner at (x, 0) and center depth on z so z is the beat/timing plane.
static glm::mat4 slab_matrix(float x, float z, float w, float h, float d) {
    return raylib_to_glm_matrix_camera(MatrixMultiply(MatrixScale(w, h, d),
                                                      MatrixTranslate(x + w / 2, h / 2, z)));
}

static glm::mat4 shape_matrix(float cx, float y_3d, float cz, float sz, float radius_scale) {
    float s = sz * radius_scale;
    return raylib_to_glm_matrix_camera(MatrixMultiply(MatrixScale(s, s, s),
                                                      MatrixTranslate(cx, y_3d, cz)));
}

// Triangular prism rotated so one vertex of the cross-section points up (△)
static glm::mat4 prism_matrix(float cx, float y_3d, float cz, float sz, float radius_scale) {
    float s = sz * radius_scale;
    Matrix scale = MatrixScale(s, s, s);
    Matrix rot = MatrixRotateY(90.0f * DEG2RAD);
    Matrix translate = MatrixTranslate(cx, y_3d, cz);
    return raylib_to_glm_matrix_camera(MatrixMultiply(MatrixMultiply(scale, rot), translate));
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
    const auto& mesh_config = reg.ctx().get<ShapeMeshConfig>();


    // 1. MeshChild transforms (multi-slab obstacles, ghost shapes)
    {
        auto view = reg.view<MeshChild>();
        auto* scratch = reg.ctx().find<MeshChildCleanupScratch>();
        if (!scratch) scratch = &reg.ctx().emplace<MeshChildCleanupScratch>();
        auto& stale_children = scratch->stale_children;
        stale_children.clear();
        for (auto [entity, mc] : view.each()) {
            auto* parent_wt = reg.try_get<WorldTransform>(mc.parent);
            if (!parent_wt) {
                if (stale_children.size() >= stale_children.capacity()) {
                    ++scratch->capacity_exceeded_count;
                }
                stale_children.push_back(entity);
                continue;
            }
            float z = parent_wt->position.y + mc.z_offset;

            if (mc.mesh_type == MeshType::Slab) {
                reg.get_or_emplace<ModelTransform>(entity) =
                    ModelTransform{slab_matrix(mc.x, z, mc.width, mc.height, mc.depth),
                                    mc.tint, 0, MeshType::Slab};
            } else {
                const auto& props = mesh_config.props[mc.mesh_index];
                reg.get_or_emplace<ModelTransform>(entity) =
                    ModelTransform{make_shape_matrix(mc.mesh_index, mc.x, 0.0f, z,
                                                      mc.width, props.radius_scale),
                                     mc.tint, mc.mesh_index, MeshType::Shape};
            }
        }
        for (auto entity : stale_children) {
            if (reg.valid(entity)) {
                reg.destroy(entity);
            }
        }
    }

    // 3. Player shape transform
    {
        auto view = reg.view<PlayerTag, WorldTransform, PlayerShape, VerticalState, Color>();
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
        auto view = reg.view<ParticleTag, WorldTransform, ParticleData, Color>();
        for (auto [entity, transform, pdata, col] : view.each()) {
            float ratio = (pdata.max_time > 0.0f) ? (pdata.remaining / pdata.max_time) : 1.0f;
            float sz = pdata.size * ratio;
            float half = sz / 2.0f;
            glm::mat4 mat = raylib_to_glm_matrix_camera(MatrixMultiply(
                MatrixScale(sz, 1, sz),
                MatrixTranslate(transform.position.x - half, 0, transform.position.y - half)));
            reg.get_or_emplace<ModelTransform>(entity) =
                ModelTransform{mat, col, 0, MeshType::Quad};
        }
    }

    // 5. Floor pulse params (SongState → FloorParams singleton)
    {
        auto& fp = reg.ctx().get<FloorParams>();
        auto* song = reg.ctx().find<SongState>();
        auto* map = find_beat_map(reg);
        const auto* settings = find_settings_state(reg);
        const float audio_offset_sec = settings ? settings::audio_offset_seconds(*settings) : 0.0f;
        float pulse = 0.0f;
        if (song && song->playing && song->beat_period > 0.0f && song->current_beat >= 0) {
            float beat_time = song->offset + static_cast<float>(song->current_beat) * song->beat_period;
            const auto beat_index = static_cast<size_t>(song->current_beat);
            if (map && beat_index < map->beat_times.size()) {
                beat_time = map->beat_times[beat_index];
            }
            pulse = floor_visuals::pulse_for_beat(song->song_time, beat_time, audio_offset_sec);
        }
        float alpha_f = Lerp(constants::FLOOR_ALPHA_REST, constants::FLOOR_ALPHA_PEAK, pulse);
        float scale = Lerp(constants::FLOOR_SCALE_REST, constants::FLOOR_SCALE_PEAK, pulse);
        fp.size  = constants::FLOOR_SHAPE_SIZE * scale;
        fp.half  = fp.size / 2.0f;
        fp.thick = constants::FLOOR_OUTLINE_THICK;
        fp.alpha = static_cast<uint8_t>(alpha_f);
    }
}

// ── compute_screen_transform: letterbox scale/offset from window size ────────

void compute_screen_transform(entt::registry& reg) {
    float win_w, win_h;
    platform_get_display_size(win_w, win_h);
    float scale = std::min(
        win_w / static_cast<float>(constants::SCREEN_W),
        win_h / static_cast<float>(constants::SCREEN_H));
    float dst_w = constants::SCREEN_W * scale;
    float dst_h = constants::SCREEN_H * scale;
    auto& st     = reg.ctx().get<ScreenTransform>();
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
        auto view = reg.view<ScorePopup, WorldTransform>();
        for (auto [entity, popup, transform] : view.each()) {
            Vector3 world_pos = {transform.position.x, 5.0f, transform.position.y};
            Vector2 sp = GetWorldToScreenEx(world_pos, cam,
                             constants::SCREEN_W, constants::SCREEN_H);
            reg.get_or_emplace<ScreenPosition>(entity) =
                ScreenPosition{sp.x, sp.y};
        }
    }
}
