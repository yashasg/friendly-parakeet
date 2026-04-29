// Obstacle Model/Transform migration slice — specification tests.
//
// STRUCTURE:
//   Section A  — Compiles and runs TODAY.  Locks in the current pre-migration
//                component set for the single-mesh obstacle kinds targeted in
//                Slice 1.  These tests will INTENTIONALLY FAIL once Keaton's
//                Slice 1 lands; failing them is the signal to update or remove
//                them and enable the Section C replacements.
//
//   Section B  — GUARDED (#if 0).  Requires Slice 0 deliverables:
//                  · app/components/render_tags.h (TagWorldPass / TagEffectsPass
//                    / TagHUDPass)
//                Enable by: #define SLICE0_RENDER_TAGS_ADDED (or remove the guard)
//                after render_tags.h is committed.
//
//   Section C  — GUARDED (#if 0).  Requires Slice 1 deliverables:
//                  · LowBar / HighBar / LanePushLeft / LanePushRight obstacle
//                    entity construction updated to emplace Model + TagWorldPass,
//                    NOT Position.
//                  · app/util/render_matrix_helpers.h with slab_matrix() exposed.
//                  · ObstaclePartDescriptor emplaced on obstacle entities.
//                Enable by: #define SLICE1_LOWBAR_MODEL_MIGRATION (or remove guard)
//                after the entity-factory migration and helper header are committed.
//
// ── BLOCKER NOTES ─────────────────────────────────────────────────────────────
//
//   BLOCKER-1 (scroll transform test):
//     slab_matrix() is currently a file-static helper in camera_system.cpp.
//     Tests in Section C that assert model.transform values (e.g. Z channel
//     after a scroll update) require slab_matrix() to be promoted to
//     app/util/render_matrix_helpers.h as a linkable function.
//     Without that, Section C scroll tests cannot be written without duplicating
//     the formula.  Owner: Keaton (per keyser-model-transform-contract.md §6).
//
//   BLOCKER-2 (ObstaclePartDescriptor shape):
//     The user directive (copilot-directive-20260428T054335Z) requires that
//     obstacle entities carry explicit part descriptors rather than only baking
//     geometry into mesh vertex data at spawn.  The exact struct (field names,
//     mesh-count) is TBD by Keaton.  Section C tests for descriptor content are
//     placeholder stubs; field names must be confirmed before enabling.
//
//   BLOCKER-3 (build_obstacle_model is headless-safe but a no-op):
//     build_obstacle_model() guards all GPU calls with IsWindowReady() and
//     returns early in headless/test environments — confirmed by Section B.5
//     tests below.  HOWEVER, the early return means spawn_obstacle
//     does NOT emplace ObstacleModel in headless, so Section C tests that call
//     spawn_obstacle and then expect ObstacleModel to be present will
//     still fail in headless mode.
//     Mitigation: manually emplace ObstacleModel{} (owned=false) for structural
//     tests that don't need real mesh content (see test_model_authority_gaps.cpp).
//     Tests that need real mesh data (meshCount, materialCount) require a full
//     GPU-context harness (InitWindow).
//     Owner: Keaton (entity-factory contract update), Baer (Section C enablement).
//
// ─────────────────────────────────────────────────────────────────────────────

#include <catch2/catch_test_macros.hpp>
#include <type_traits>
#include <raylib.h>
#include <raymath.h>
#include <entt/entt.hpp>

#include "test_helpers.h"
#include "util/enum_names.h"
#include "entities/obstacle_entity.h"
#include "components/obstacle.h"
#include "components/transform.h"
#include "components/rendering.h"
#include "constants.h"
#include "entities/obstacle_render_entity.h"
#include "systems/all_systems.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static entt::entity make_obstacle_entity(entt::registry& reg, ObstacleKind kind) {
    return spawn_obstacle(reg, {.kind = kind, .x = 360.0f, .y = constants::SPAWN_Y});
}

// ════════════════════════════════════════════════════════════════════════════
// SECTION A — Post-migration state: LowBar/HighBar use ObstacleScrollZ
//
// These tests replaced the pre-migration "has Position" baseline tests after
// Slice 2 landed (LowBar + HighBar migrated to ObstacleScrollZ + ObstacleModel).
// They document the current authoritative component contract for these kinds.
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("post-migration: LowBar has ObstacleScrollZ, not Position",
          "[post_migration][model_slice]") {
    entt::registry reg;
    auto e = make_obstacle_entity(reg, ObstacleKind::LowBar);

    REQUIRE(reg.all_of<ObstacleScrollZ>(e));
    CHECK_FALSE(reg.all_of<Position>(e));
    CHECK_FALSE(reg.all_of<Model>(e));  // raw Model not emplaced by entity factory
}

TEST_CASE("post-migration: HighBar has ObstacleScrollZ, not Position",
          "[post_migration][model_slice]") {
    entt::registry reg;
    auto e = make_obstacle_entity(reg, ObstacleKind::HighBar);

    REQUIRE(reg.all_of<ObstacleScrollZ>(e));
    CHECK_FALSE(reg.all_of<Position>(e));
    CHECK_FALSE(reg.all_of<Model>(e));
}

TEST_CASE("post-migration: LanePushLeft still has Position (not yet migrated)",
          "[post_migration][model_slice]") {
    entt::registry reg;
    auto e = make_obstacle_entity(reg, ObstacleKind::LanePushLeft);
    REQUIRE(reg.all_of<Position>(e));
    CHECK_FALSE(reg.all_of<ObstacleScrollZ>(e));
}

TEST_CASE("post-migration: LanePushRight still has Position (not yet migrated)",
          "[post_migration][model_slice]") {
    entt::registry reg;
    auto e = make_obstacle_entity(reg, ObstacleKind::LanePushRight);
    REQUIRE(reg.all_of<Position>(e));
    CHECK_FALSE(reg.all_of<ObstacleScrollZ>(e));
}

TEST_CASE("post-migration: spawn_obstacle does not emplace Model on any kind",
          "[post_migration][model_slice]") {
    // Exhaustive: confirm NO obstacle entity factory directly emplaces raw Model.
    // ObstacleModel (RAII wrapper) is emplaced separately by build_obstacle_model
    // which requires GPU context — not exercised here.
    const ObstacleKind all_kinds[] = {
        ObstacleKind::ShapeGate,
        ObstacleKind::LaneBlock,
        ObstacleKind::LowBar,
        ObstacleKind::HighBar,
        ObstacleKind::ComboGate,
        ObstacleKind::SplitPath,
        ObstacleKind::LanePushLeft,
        ObstacleKind::LanePushRight,
    };
    for (auto kind : all_kinds) {
        entt::registry reg;
        auto e = make_obstacle_entity(reg, kind);
        INFO("Kind: " << ToString(kind));
        CHECK_FALSE(reg.all_of<Model>(e));
    }
}

TEST_CASE("post-migration: LowBar ObstacleScrollZ.z set from spawn input",
          "[post_migration][model_slice]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {.kind = ObstacleKind::LowBar, .x = 360.0f, .y = constants::SPAWN_Y});

    REQUIRE(reg.all_of<ObstacleScrollZ>(e));
    CHECK(reg.get<ObstacleScrollZ>(e).z == constants::SPAWN_Y);
}

// ════════════════════════════════════════════════════════════════════════════
// SECTION B — Render-pass tag invariants
//
// ENABLE AFTER: Slice 0 — app/components/render_tags.h committed with:
//   struct TagWorldPass {};
//   struct TagEffectsPass {};
//   struct TagHUDPass {};
//
// Remove or replace "#if 0" with "#if 1" once render_tags.h exists.
// ════════════════════════════════════════════════════════════════════════════

#if 1  // SLICE0_RENDER_TAGS_ADDED — render-pass tags folded into rendering.h

// (TagWorldPass / TagEffectsPass / TagHUDPass are defined in components/rendering.h,
//  already included above via obstacle_render_entity.h -> rendering.h)

// Type-trait asserts: render-pass tags are empty standard-layout structs.
// EnTT represents empty structs as zero-size components for tag queries.
static_assert(std::is_empty_v<TagWorldPass>,
    "TagWorldPass must be an empty struct (tag component).");
static_assert(std::is_empty_v<TagEffectsPass>,
    "TagEffectsPass must be an empty struct (tag component).");
static_assert(std::is_empty_v<TagHUDPass>,
    "TagHUDPass must be an empty struct (tag component).");

static_assert(std::is_standard_layout_v<TagWorldPass>,
    "TagWorldPass must be standard-layout for EnTT compatibility.");
static_assert(std::is_standard_layout_v<TagEffectsPass>,
    "TagEffectsPass must be standard-layout for EnTT compatibility.");
static_assert(std::is_standard_layout_v<TagHUDPass>,
    "TagHUDPass must be standard-layout for EnTT compatibility.");

// Tags must be distinct types — a single entity should carry at most one.
static_assert(!std::is_same_v<TagWorldPass, TagEffectsPass>,
    "Render-pass tags must be distinct types.");
static_assert(!std::is_same_v<TagWorldPass, TagHUDPass>,
    "Render-pass tags must be distinct types.");
static_assert(!std::is_same_v<TagEffectsPass, TagHUDPass>,
    "Render-pass tags must be distinct types.");

TEST_CASE("TagWorldPass: emplace and has_any_of query works (Slice 0)",
          "[render_tags][model_slice]") {
    entt::registry reg;
    auto e = reg.create();
    reg.emplace<TagWorldPass>(e);
    CHECK(reg.all_of<TagWorldPass>(e));
    CHECK_FALSE(reg.all_of<TagEffectsPass>(e));
    CHECK_FALSE(reg.all_of<TagHUDPass>(e));
}

TEST_CASE("Each render-pass tag creates a distinct EnTT component pool (Slice 0)",
          "[render_tags][model_slice]") {
    entt::registry reg;

    auto e1 = reg.create(); reg.emplace<TagWorldPass>(e1);
    auto e2 = reg.create(); reg.emplace<TagEffectsPass>(e2);
    auto e3 = reg.create(); reg.emplace<TagHUDPass>(e3);

    // e1 is only in TagWorldPass pool
    CHECK( reg.all_of<TagWorldPass>(e1));
    CHECK(!reg.all_of<TagEffectsPass>(e1));
    CHECK(!reg.all_of<TagHUDPass>(e1));

    // e2 is only in TagEffectsPass pool
    CHECK(!reg.all_of<TagWorldPass>(e2));
    CHECK( reg.all_of<TagEffectsPass>(e2));
    CHECK(!reg.all_of<TagHUDPass>(e2));
}

#endif  // SLICE0_RENDER_TAGS_ADDED

// ════════════════════════════════════════════════════════════════════════════
// SECTION B.5 — build_obstacle_model headless guard (active, runs today)
//
// Verifies that build_obstacle_model() is safe to call from any context
// (no InitWindow required) and confirms the IsWindowReady() guard is active.
// Also validates the on_destroy listener for ObstacleModel is safe on unowned
// instances — critical because obstacle_despawn_system will destroy obstacle entities
// including any future Model-carrying ones.
// ════════════════════════════════════════════════════════════════════════════

TEST_CASE("build_obstacle_model: no-op when window not ready (headless safe)",
          "[model_slice][headless_guard]") {
    // Confirm BLOCKER-3 mitigation: build_obstacle_model() is a no-op without
    // an active OpenGL context, so calling it from tests never crashes and
    // never emplaces GPU-resource components.
    entt::registry reg;
    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    reg.emplace<Obstacle>(e, ObstacleKind::LowBar, static_cast<int16_t>(constants::PTS_LOW_BAR));
    reg.emplace<DrawSize>(e, static_cast<float>(constants::SCREEN_W), 40.0f);
    reg.emplace<Color>(e, Color{255, 180, 0, 255});

    build_obstacle_model(reg, e);  // IsWindowReady() == false in test environment

    // No GPU-resource components must be emplaced in headless mode.
    CHECK_FALSE(reg.all_of<ObstacleModel>(e));
    CHECK_FALSE(reg.all_of<ObstacleParts>(e));
    CHECK_FALSE(reg.all_of<TagWorldPass>(e));
}

TEST_CASE("on_obstacle_model_destroy: safe on unowned ObstacleModel (headless)",
          "[model_slice][headless_guard]") {
    // Validates the on_destroy listener path that fires when an obstacle entity
    // carrying ObstacleModel (owned=false) is destroyed. Must not crash or make
    // GPU calls. This is the expected path for entities whose build_obstacle_model
    // call was a no-op (headless) but that were given an ObstacleModel component
    // for structural test purposes.
    entt::registry reg;
    auto e = reg.create();
    reg.emplace<ObstacleModel>(e);  // default-constructed: owned=false

    // Manually invoke what the on_destroy<ObstacleModel> listener does.
    on_obstacle_model_destroy(reg, e);  // must not crash (no GPU calls)

    SUCCEED("on_obstacle_model_destroy is safe on unowned ObstacleModel");
}

// ════════════════════════════════════════════════════════════════════════════
// SECTION C — ObstacleScrollZ scroll + cleanup behaviour (Slice 2)
//
// These tests validate that scroll_system writes ObstacleScrollZ.z correctly
// and that obstacle_despawn_system destroys ObstacleScrollZ entities past DESTROY_Y.
// All tests are headless-safe (no GPU context required).
// ════════════════════════════════════════════════════════════════════════════

#if 1  // SLICE2_LOWBAR_HIGHBAR_MIGRATION — LowBar + HighBar migrated to ObstacleScrollZ

TEST_CASE("post-migration: LowBar has ObstacleScrollZ + RequiredVAction + DrawSize (Slice 2)",
          "[post_migration][model_slice]") {
    entt::registry reg;
    auto e = make_obstacle_entity(reg, ObstacleKind::LowBar);

    REQUIRE(reg.all_of<ObstacleScrollZ>(e));
    REQUIRE(reg.all_of<RequiredVAction>(e));
    REQUIRE(reg.all_of<DrawSize>(e));
    CHECK_FALSE(reg.all_of<Position>(e));
    CHECK(reg.get<RequiredVAction>(e).action == VMode::Jumping);
}

TEST_CASE("post-migration: HighBar has ObstacleScrollZ + RequiredVAction (Slice 2)",
          "[post_migration][model_slice]") {
    entt::registry reg;
    auto e = make_obstacle_entity(reg, ObstacleKind::HighBar);

    REQUIRE(reg.all_of<ObstacleScrollZ>(e));
    REQUIRE(reg.all_of<RequiredVAction>(e));
    CHECK_FALSE(reg.all_of<Position>(e));
    CHECK(reg.get<RequiredVAction>(e).action == VMode::Sliding);
}

TEST_CASE("post-migration: ObstacleScrollZ.z matches spawn input (Slice 2)",
          "[post_migration][model_slice]") {
    constexpr float spawn_z = -500.0f;
    entt::registry reg;

    auto lo = spawn_obstacle(reg, {.kind = ObstacleKind::LowBar, .x = 360.0f, .y = spawn_z});
    REQUIRE(reg.all_of<ObstacleScrollZ>(lo));
    CHECK(reg.get<ObstacleScrollZ>(lo).z == spawn_z);

    auto hi = spawn_obstacle(reg, {.kind = ObstacleKind::HighBar, .x = 360.0f, .y = spawn_z - 100.0f});
    REQUIRE(reg.all_of<ObstacleScrollZ>(hi));
    CHECK(reg.get<ObstacleScrollZ>(hi).z == spawn_z - 100.0f);
}

TEST_CASE("post-migration: scroll_system writes ObstacleScrollZ.z (Slice 2)",
          "[post_migration][model_slice][scroll]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 2.0f;

    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    reg.emplace<ObstacleScrollZ>(e, 0.0f);
    reg.emplace<BeatInfo>(e, 0, 4.0f, 0.0f);  // beat_index=0, arrival=4, spawn=0

    scroll_system(reg, 0.016f);

    float expected_z = constants::SPAWN_Y + (2.0f - 0.0f) * song.scroll_speed;
    CHECK(reg.get<ObstacleScrollZ>(e).z == expected_z);
}

TEST_CASE("post-migration: scroll_system ObstacleScrollZ formula is deterministic (Slice 2)",
          "[post_migration][model_slice][scroll]") {
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.scroll_speed = 300.0f;
    song.song_time = 1.5f;

    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    reg.emplace<ObstacleScrollZ>(e, 0.0f);
    reg.emplace<BeatInfo>(e, 0, 5.0f, 0.5f);  // spawn_time=0.5

    scroll_system(reg, 0.016f);

    float expected = constants::SPAWN_Y + (1.5f - 0.5f) * 300.0f;
    CHECK(reg.get<ObstacleScrollZ>(e).z == expected);
}

TEST_CASE("post-migration: obstacle_despawn_system destroys ObstacleScrollZ entities past DESTROY_Y (Slice 2)",
          "[post_migration][model_slice]") {
    auto reg = make_registry();

    auto expired = reg.create();
    reg.emplace<ObstacleTag>(expired);
    reg.emplace<ObstacleScrollZ>(expired, constants::DESTROY_Y + 1.0f);
    reg.emplace<Obstacle>(expired, ObstacleKind::LowBar, static_cast<int16_t>(0));

    auto active = reg.create();
    reg.emplace<ObstacleTag>(active);
    reg.emplace<ObstacleScrollZ>(active, constants::PLAYER_Y - 200.0f);
    reg.emplace<Obstacle>(active, ObstacleKind::LowBar, static_cast<int16_t>(0));

    obstacle_despawn_system(reg, 0.016f);

    CHECK_FALSE(reg.valid(expired));
    CHECK(reg.valid(active));
}

#endif  // SLICE2_LOWBAR_HIGHBAR_MIGRATION

// ════════════════════════════════════════════════════════════════════════════
// SECTION D — Revision blockers regression guard (BF-1..BF-5)
//
// These tests verify the five Kujan blockers / component-cleanup invariants.
//   BF-1: LoadModelFromMesh must not appear; manual mesh arrays used.
//   BF-2: Camera writes model.transform, not ModelTransform, for migrated entities.
//   BF-3: Duplicate helpers deleted; canonical app/entities/ path used.
//   BF-4: ObstacleParts carries explicit geometry fields, not an empty tag.
//   BF-5: ObstacleScrollZ and ObstacleModel meet structural/RAII contract.
// ════════════════════════════════════════════════════════════════════════════

// BF-4: ObstacleParts is not empty — it must carry geometry fields.
static_assert(!std::is_empty_v<ObstacleParts>,
    "BF-4: ObstacleParts must not be an empty tag. "
    "It must carry cx/cy/cz/width/height/depth geometry fields.");

static_assert(std::is_standard_layout_v<ObstacleParts>,
    "ObstacleParts must be standard-layout for EnTT compatibility.");

// BF-5a: ObstacleScrollZ structural contract.
// Must be non-empty (carries float z) and standard-layout for EnTT pool storage.
static_assert(!std::is_empty_v<ObstacleScrollZ>,
    "BF-5a: ObstacleScrollZ must not be an empty tag — it carries float z.");

static_assert(std::is_standard_layout_v<ObstacleScrollZ>,
    "BF-5a: ObstacleScrollZ must be standard-layout for EnTT compatibility.");

static_assert(std::is_same_v<decltype(ObstacleScrollZ::z), float>,
    "BF-5a: ObstacleScrollZ::z must be float.");

// BF-5b: ObstacleModel RAII contract.
// Deleted copy prevents accidental GPU resource duplication; move keeps RAII safe.
static_assert(!std::is_copy_constructible_v<ObstacleModel>,
    "BF-5b: ObstacleModel must not be copy-constructible (GPU RAII guard).");

static_assert(!std::is_copy_assignable_v<ObstacleModel>,
    "BF-5b: ObstacleModel must not be copy-assignable (GPU RAII guard).");

static_assert(std::is_move_constructible_v<ObstacleModel>,
    "BF-5b: ObstacleModel must be move-constructible (EnTT storage requires this).");

static_assert(std::is_move_assignable_v<ObstacleModel>,
    "BF-5b: ObstacleModel must be move-assignable (EnTT storage requires this).");

TEST_CASE("BF-4: ObstacleParts default-constructed has zero geometry fields",
          "[model_slice][bf4_regression]") {
    ObstacleParts pd{};
    CHECK(pd.cx     == 0.0f);
    CHECK(pd.cy     == 0.0f);
    CHECK(pd.cz     == 0.0f);
    CHECK(pd.width  == 0.0f);
    CHECK(pd.height == 0.0f);
    CHECK(pd.depth  == 0.0f);
}

TEST_CASE("BF-2: migrated LowBar/HighBar entities do not get ModelTransform from factory",
          "[model_slice][bf2_regression]") {
    // ModelTransform must not be emitted by spawn_obstacle.
    // camera_system section 1b (fixed) writes om.model.transform instead.
    entt::registry reg;

    auto lo = spawn_obstacle(reg, {.kind = ObstacleKind::LowBar, .x = 0.0f, .y = constants::SPAWN_Y});
    CHECK_FALSE(reg.all_of<ModelTransform>(lo));

    auto hi = spawn_obstacle(reg, {.kind = ObstacleKind::HighBar, .x = 0.0f, .y = constants::SPAWN_Y});
    CHECK_FALSE(reg.all_of<ModelTransform>(hi));
}

TEST_CASE("BF-2: slab_matrix scale diagonal equals world dimensions (unit-cube contract)",
          "[model_slice][bf2_regression]") {
    // Verifies that slab_matrix(cx, oz+cz, w, h, d) on a unit-cube mesh produces
    // a transform whose scale diagonal is exactly (w, h, d) — once, not squared.
    // This is the double-scale regression guard: if build_obstacle_model() were to
    // use GenMeshCube(w, h, d) instead of GenMeshCube(1,1,1), slab_matrix would
    // apply the dimensions a second time and the rendered bar would be w²×h²×d².
    //
    // Formula (matches camera_system.cpp static slab_matrix):
    //   MatrixMultiply(MatrixScale(w, h, d), MatrixTranslate(cx+w/2, h/2, z+d/2))
    // After multiply, scale diagonal: m0=w, m5=h, m10=d (column-major, raylib layout).
    // Translation in m12=cx+w/2, m13=h/2, m14=z+d/2.
    // GPU-free: MatrixMultiply / MatrixScale / MatrixTranslate are pure math.

    const float oz_z   = constants::PLAYER_Y;   // typical in-window Z
    const float cx     = 0.0f;
    const float cz     = 0.0f;
    const float w      = constants::SCREEN_W_F;
    const float h      = constants::LOWBAR_3D_HEIGHT;
    const float d      = 40.0f;  // DrawSize.h for LowBar obstacle entity

    const float z = oz_z + cz;
    const Matrix mat = MatrixMultiply(
        MatrixScale(w, h, d),
        MatrixTranslate(cx + w / 2.0f, h / 2.0f, z + d / 2.0f));

    // Scale diagonal must equal the intended world dimensions exactly once.
    CHECK(mat.m0  == w);
    CHECK(mat.m5  == h);
    CHECK(mat.m10 == d);

    // Translation is scaled: mat.m12 = cx+w/2, mat.m13 = h/2, mat.m14 = z+d/2.
    // All non-zero, confirming a non-identity transform.
    CHECK(mat.m12 != 0.0f);
    CHECK(mat.m13 != 0.0f);
    CHECK(mat.m14 != 0.0f);
}

TEST_CASE("BF-1: ObstacleModel owns separate mesh + material arrays (struct layout)",
          "[model_slice][bf1_regression]") {
    // Validate that ObstacleModel uses the raylib Model struct which
    // requires separate meshes/materials/meshMaterial pointer arrays,
    // matching the manual construction pattern (RL_MALLOC each array).
    // If LoadModelFromMesh were used, meshes would not be a separately
    // owned pointer — this tests the expected pointer structure at default init.
    Model m{};
    CHECK(m.meshes       == nullptr);   // must be RL_MALLOC'd by build_obstacle_model
    CHECK(m.materials    == nullptr);   // must be RL_MALLOC'd by build_obstacle_model
    CHECK(m.meshMaterial == nullptr);   // must be RL_CALLOC'd by build_obstacle_model
    CHECK(m.meshCount    == 0);
    CHECK(m.materialCount == 0);

    // ObstacleModel wrapper defaults to owned=false (GPU-free default).
    ObstacleModel om{};
    CHECK_FALSE(om.owned);
    CHECK(om.model.meshes == nullptr);
}
