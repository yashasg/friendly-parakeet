#include "camera_system.h"
#include "all_systems.h"
#include "../components/shape_mesh.h"
#include "../components/rendering.h"
#include "../components/transform.h"
#include "../components/player.h"
#include "../components/obstacle.h"
#include "../components/particle.h"
#include "../components/scoring.h"
#include "../components/song_state.h"
#include "../constants.h"
#include "../platform_display.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <tuple>

// Shape properties: radius_scale, height_ratio (height / radius)
const ShapeProps SHAPE_PROPS[4] = {
    { 0.5f, 0.6f },  // Circle:   cylinder
    { 0.5f, 0.6f },  // Square:   cube
    { 0.5f, 0.6f },  // Triangle: cone
    { 0.6f, 1.17f }, // Hexagon:  cylinder (6 slices)
};

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

static ShapeMeshes build_shape_meshes() {
    ShapeMeshes sm = {};

    sm.shapes[0] = GenMeshCylinder(1.0f, SHAPE_PROPS[0].height_ratio, 12);
    sm.shapes[1] = GenMeshCube(2.0f, SHAPE_PROPS[1].height_ratio, 2.0f);
    sm.shapes[2] = GenMeshCylinder(1.0f, SHAPE_PROPS[2].height_ratio, 3);
    sm.shapes[3] = GenMeshCylinder(1.0f, SHAPE_PROPS[3].height_ratio, 6);
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
    reg.ctx().emplace<ShapeMeshes>(build_shape_meshes());
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

// GenMeshCube is centered at origin. Translate to position the slab's
// bottom-left corner at (x, 0, z) with the given dimensions.
static Matrix slab_matrix(float x, float z, float w, float h, float d) {
    return MatrixMultiply(MatrixScale(w, h, d),
                          MatrixTranslate(x + w/2, h/2, z + d/2));
}

static Matrix shape_matrix(float cx, float y_3d, float cz, float sz, float radius_scale) {
    float s = sz * radius_scale;
    return MatrixMultiply(MatrixScale(s, s, s), MatrixTranslate(cx, y_3d, cz));
}

// Triangular prism rotated so one vertex of the cross-section points up (△)
static Matrix prism_matrix(float cx, float y_3d, float cz, float sz, float radius_scale) {
    float s = sz * radius_scale;
    Matrix scale = MatrixScale(s, s, s);
    Matrix rot = MatrixRotateY(90.0f * DEG2RAD);
    Matrix translate = MatrixTranslate(cx, y_3d, cz);
    return MatrixMultiply(MatrixMultiply(scale, rot), translate);
}

// Pick the correct matrix for a shape mesh (triangle prism needs rotation)
static Matrix make_shape_matrix(int mesh_index, float cx, float y_3d, float cz,
                                float sz, float radius_scale) {
    if (mesh_index == static_cast<int>(Shape::Triangle))
        return prism_matrix(cx, y_3d, cz, sz, radius_scale);
    return shape_matrix(cx, y_3d, cz, sz, radius_scale);
}

// ── game_camera_system: model-to-world transforms for all 3D renderables ────

void game_camera_system(entt::registry& reg, float /*dt*/) {
    // 1. Single-slab obstacle transforms
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {
            switch (obs.kind) {
                case ObstacleKind::LanePushLeft:
                case ObstacleKind::LanePushRight:
                    reg.get_or_emplace<ModelTransform>(entity) =
                        ModelTransform{slab_matrix(pos.x-dsz.w/2, pos.y, dsz.w, constants::OBSTACLE_3D_HEIGHT, dsz.h),
                                       col, MeshType::Slab, 0};
                    break;
                default:
                    break;
            }
        }
    }

    // 1b. Model-authority vertical bars: write scroll transform directly into
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
            auto& parent_pos = reg.get<Position>(mc.parent);
            float z = parent_pos.y + mc.z_offset;

            if (mc.mesh_type == MeshType::Slab) {
                reg.get_or_emplace<ModelTransform>(entity) =
                    ModelTransform{slab_matrix(mc.x, z, mc.width, mc.height, mc.depth),
                                   mc.tint, MeshType::Slab, 0};
            } else {
                const auto& props = SHAPE_PROPS[mc.mesh_index];
                reg.get_or_emplace<ModelTransform>(entity) =
                    ModelTransform{make_shape_matrix(mc.mesh_index, mc.x, 0.0f, z, mc.width, props.radius_scale),
                                   mc.tint, MeshType::Shape, mc.mesh_index};
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
            int shape_idx = static_cast<int>(pshape.current);
            const auto& props = SHAPE_PROPS[shape_idx];
            reg.get_or_emplace<ModelTransform>(entity) =
                ModelTransform{make_shape_matrix(shape_idx, transform.position.x, y_3d,
                                                 transform.position.y, sz, props.radius_scale),
                               col, MeshType::Shape, shape_idx};
        }
    }

    // 4. Particle transforms
    {
        auto view = reg.view<ParticleTag, WorldTransform, ParticleData, Color>();
        for (auto [entity, transform, pdata, col] : view.each()) {
            float ratio = (pdata.max_time > 0.0f) ? (pdata.remaining / pdata.max_time) : 1.0f;
            float sz = pdata.size * ratio;
            float half = sz / 2.0f;
            Matrix mat = MatrixMultiply(
                MatrixScale(sz, 1, sz),
                MatrixTranslate(transform.position.x - half, 0, transform.position.y - half));
            reg.get_or_emplace<ModelTransform>(entity) =
                ModelTransform{mat, col, MeshType::Quad, 0};
        }
    }

    // 5. Floor pulse params (SongState → FloorParams singleton)
    {
        auto& fp = reg.ctx().get<FloorParams>();
        auto* song = reg.ctx().find<SongState>();
        float pulse = 0.0f;
        if (song && song->playing && song->beat_period > 0.0f && song->current_beat >= 0) {
            float time_since_beat = song->song_time
                - (song->offset + static_cast<float>(song->current_beat) * song->beat_period);
            float pulse_t = Clamp(time_since_beat / constants::FLOOR_PULSE_DECAY, 0.0f, 1.0f);
            float ease = 1.0f - (1.0f - pulse_t) * (1.0f - pulse_t);
            pulse = 1.0f - ease;
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
