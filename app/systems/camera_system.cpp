#include "camera_system.h"
#include "camera_resources.h"
#include "all_systems.h"
#include "shape_mesh.h"
#include "../components/rendering.h"
#include "../components/transform.h"
#include "../components/player.h"
#include "../entities/beat_map.h"
#include "../components/particle.h"
#include "../components/scoring.h"
#include "../components/song_state.h"
#include "../constants.h"
#include "../entities/settings.h"
#include "../platform_display.h"
#include "../util/shape_lane_mapping.h"
#include "../util/shape_tag.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
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

// ── ShapeMeshes RAII member definitions ─────────────────────────────────────

void ShapeMeshes::release() {
    if (!owned) return;
    for (int i = 0; i < 4; ++i)
        UnloadMesh(shapes[i]);
    UnloadMesh(slab);
    UnloadMesh(quad);
    // raylib's UnloadMaterial() owns material.shader; unloading it separately
    // double-frees the shader's location array.
    UnloadMaterial(material);
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

    ShapeMeshes sm = {};
    sm.shapes[0] = GenMeshCylinder(1.0f, mesh_config.props[0].height_ratio, 12);
    sm.shapes[1] = GenMeshCube(2.0f, mesh_config.props[1].height_ratio, 2.0f);
    sm.shapes[2] = GenMeshCylinder(1.0f, mesh_config.props[2].height_ratio, 3);
    sm.shapes[3] = GenMeshCylinder(1.0f, mesh_config.props[3].height_ratio, 6);
    sm.slab = GenMeshCube(1.0f, 1.0f, 1.0f);
    sm.quad = GenMeshPlane(1.0f, 1.0f, 1, 1);

    Shader shader = LoadShaderFromMemory(MESH_VS, MESH_FS);
    sm.material = LoadMaterialDefault();
    sm.material.shader = shader;

    sm.owned = true;
    reg.ctx().emplace<ShapeMeshes>(std::move(sm));
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

// Pick the correct matrix for a shape mesh. Triangle prism is rotated 90°
// around Y so one vertex of the cross-section points up (△); other shapes
// are uniform-scaled and translated to (cx, y_3d, cz).
static Matrix make_shape_matrix(uint8_t mesh_index, float cx, float y_3d, float cz,
                                float sz, float radius_scale) {
    const float s = sz * radius_scale;
    Matrix m = MatrixScale(s, s, s);
    if (mesh_index == static_cast<uint8_t>(Shape::Triangle))
        m = MatrixMultiply(m, MatrixRotateY(90.0f * DEG2RAD));
    return MatrixMultiply(m, MatrixTranslate(cx, y_3d, cz));
}

// ── game_camera_system: model-to-world transforms for all 3D renderables ────

void game_camera_system(entt::registry& reg, [[maybe_unused]] float dt) {
    const auto& mesh_config = reg.ctx().get<ShapeMeshConfig>();


    // 1. MeshChild transforms (multi-slab obstacles, ghost shapes)
    // 1. MeshChild transforms (multi-slab obstacles, ghost shapes)
    //    Existential processing: one transform per kind tag (issue #1202/#1204).
    //    Stale children are batched across both tag views and destroyed once.
    {
        auto* scratch = reg.ctx().find<MeshChildCleanupScratch>();
        if (!scratch) scratch = &reg.ctx().emplace<MeshChildCleanupScratch>();
        auto& stale_children = scratch->stale_children;
        stale_children.clear();

        auto record_stale_or_compute_z = [&](entt::entity entity, const MeshChild& mc,
                                             float& out_z) -> bool {
            auto* parent_wt = reg.try_get<WorldPosition>(mc.parent);
            if (!parent_wt) {
                if (stale_children.size() >= stale_children.capacity()) {
                    ++scratch->capacity_exceeded_count;
                }
                stale_children.push_back(entity);
                return false;
            }
            out_z = parent_wt->position.y + mc.z_offset;
            return true;
        };

        // Slab children: no per-kind columns.
        // GenMeshCube is centered at origin; translate so the slab's bottom-left
        // corner sits at (mc.x, 0) and the depth dimension is centred on z (the
        // beat/timing plane).
        for (auto [entity, mc] : reg.view<MeshChild, MeshKindSlab>().each()) {
            float z = 0.0f;
            if (!record_stale_or_compute_z(entity, mc, z)) continue;
            reg.get_or_emplace<ModelTransform>(entity) =
                ModelTransform{MatrixMultiply(MatrixScale(mc.width, mc.height, mc.depth),
                                              MatrixTranslate(mc.x + mc.width / 2,
                                                              mc.height / 2, z)),
                                mc.tint};
        }

        // Shape children: kind row carries mesh_index column.
        for (auto [entity, mc, kind] : reg.view<MeshChild, MeshKindShape>().each()) {
            float z = 0.0f;
            if (!record_stale_or_compute_z(entity, mc, z)) continue;
            if (kind.mesh_index >= kShapeCount) {
                TraceLog(LOG_WARNING, "game_camera_system skipped invalid MeshChild shape index %u",
                         static_cast<unsigned>(kind.mesh_index));
                reg.remove<ModelTransform>(entity);
                continue;
            }
            const auto& props = mesh_config.props[kind.mesh_index];
            reg.get_or_emplace<ModelTransform>(entity) =
                ModelTransform{make_shape_matrix(kind.mesh_index, mc.x, 0.0f, z,
                                                  mc.width, props.radius_scale),
                                 mc.tint};
        }

        for (auto entity : stale_children) {
            if (reg.valid(entity)) {
                reg.destroy(entity);
            }
        }
    }

    // 3. Player shape transform
    {
        auto view = reg.view<PlayerTag, WorldPosition, PlayerShape, Color>();
        for (auto [entity, transform, pshape, col] : view.each()) {
            (void)pshape;
            const auto* jump = reg.try_get<Jumping>(entity);
            float y_3d = jump ? -jump->y_offset : 0.0f;
            float sz = constants::PLAYER_SIZE;
            if (reg.all_of<Sliding>(entity)) sz *= 0.5f;
            const Shape current = current_player_shape(reg, entity);
            const int shape_idx = shape_index(current);
            if (shape_idx < 0) {
                TraceLog(LOG_WARNING, "game_camera_system skipped invalid PlayerShape %d",
                         static_cast<int>(current));
                reg.remove<ModelTransform>(entity);
                reg.remove<MeshKindShape>(entity);
                continue;
            }
            const auto mesh_index = static_cast<uint8_t>(shape_idx);
            const auto& props = mesh_config.props[shape_idx];
            reg.get_or_emplace<ModelTransform>(entity) =
                ModelTransform{make_shape_matrix(mesh_index, transform.position.x, y_3d,
                                                 transform.position.y, sz, props.radius_scale),
                               col};
            reg.get_or_emplace<MeshKindShape>(entity).mesh_index = mesh_index;
        }
    }

    // 4. Particle transforms
    {
        auto view = reg.view<ParticleTag, WorldPosition, ParticleData, Color>();
        for (auto [entity, transform, pdata, col] : view.each()) {
            float ratio = (pdata.max_time > 0.0f) ? (pdata.remaining / pdata.max_time) : 1.0f;
            float sz = pdata.size * ratio;
            float half = sz / 2.0f;
            Matrix mat = MatrixMultiply(
                MatrixScale(sz, 1, sz),
                MatrixTranslate(transform.position.x - half, 0, transform.position.y - half));
            reg.get_or_emplace<ModelTransform>(entity) =
                ModelTransform{mat, col};
            reg.get_or_emplace<MeshKindQuad>(entity);
        }
    }

    // 5. Floor pulse params (SongState → FloorParams singleton)
    {
        auto& fp = reg.ctx().get<FloorParams>();
        auto* song = reg.ctx().find<SongState>();
        auto* map = find_beat_map(reg);
        const auto* settings = find_settings_state(reg);
        const float audio_offset_sec = settings ? settings::audio_offset_seconds(*settings) : 0.0f;
        const auto* cursor = reg.ctx().find<BeatCursor>();
        float pulse = 0.0f;
        if (song && song->playing && song->beat_period > 0.0f && cursor) {
            float beat_time = song->offset + static_cast<float>(cursor->last_crossed) * song->beat_period;
            const auto beat_index = static_cast<size_t>(cursor->last_crossed);
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

void ui_camera_system(entt::registry& reg, [[maybe_unused]] float dt) {
    // ScreenTransform is computed once per frame by game_loop_frame (before
    // input_system) and stored in the registry context.  Reading it here is
    // sufficient; do NOT call compute_screen_transform again (#241).

    // Popup screen-space projection
    {
        auto& cam = game_camera(reg).cam;
        auto view = reg.view<ScorePopup, WorldPosition>();
        for (auto [entity, popup, transform] : view.each()) {
            Vector3 world_pos = {transform.position.x, 5.0f, transform.position.y};
            Vector2 sp = GetWorldToScreenEx(world_pos, cam,
                             constants::SCREEN_W, constants::SCREEN_H);
            reg.get_or_emplace<ScreenPosition>(entity) =
                ScreenPosition{sp.x, sp.y};
        }
    }
}
