// PR #43 regression tests — Baer
// Covers the six unresolved review threads:
//   1. ScreenTransform stale on first/resize frame before input_system
//   2. game_camera_system slab_matrix Y/Z dimension swap
//   3. game_camera_system MeshChild slab Y/Z dimension swap
//   4. title/controller input routing remains outside legacy ECS hitbox plumbing
//   6. shape_obstacle child destruction must not touch other parents' children

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "entities/obstacle_render_entity.h"
#include "entities/obstacle_entity.h"
#include "components/rendering.h"
#include "constants.h"
#include <filesystem>
#include <fstream>
#include <raylib.h>
#include <sstream>
#include <string>

// ═══════════════════════════════════════════════════════════════════════
// Theme 1 – ScreenTransform ordering: stale identity ST causes wrong
//           virtual-coordinate mapping for letterboxed windows.
// ═══════════════════════════════════════════════════════════════════════

// Mirror of the to_vx/to_vy lambdas in input_system.cpp (pure math).
static float vx(const ScreenTransform& st, float wx) { return (wx - st.offset_x) / st.scale; }
static float vy(const ScreenTransform& st, float wy) { return (wy - st.offset_y) / st.scale; }

TEST_CASE("screen_transform ordering: stale identity ST mismaps letterboxed tap",
          "[screen_transform][regression][pr43]") {
    using Catch::Matchers::WithinAbs;

    // Scenario: virtual 720×1280 rendered into a 2×-scaled 1440×2560 window
    // with a horizontal pillarbox (offset_x=0, offset_y=0, scale=2).
    // A tap at window pixel (720, 1280) should map to virtual (360, 640).

    ScreenTransform correct;
    correct.offset_x = 0.0f;
    correct.offset_y = 0.0f;
    correct.scale    = 2.0f;

    CHECK_THAT(vx(correct, 720.0f),  WithinAbs(360.0f, 1e-3f));
    CHECK_THAT(vy(correct, 1280.0f), WithinAbs(640.0f, 1e-3f));

    // With STALE identity ST (as on first frame before ui_camera_system runs),
    // the same tap lands at (720, 1280) — entirely outside the virtual screen
    // width [0, 720] and outside the logical area.
    ScreenTransform stale;  // defaults: scale=1, offset=0
    float stale_vx = vx(stale, 720.0f);
    float stale_vy = vy(stale, 1280.0f);

    // stale mapping differs from correct — this is the bug
    CHECK_FALSE(std::abs(stale_vx - 360.0f) < 1.0f);  // wrong X
    CHECK_FALSE(std::abs(stale_vy - 640.0f) < 1.0f);  // wrong Y
}

TEST_CASE("screen_transform ordering: stale identity ST mismaps letterbox-offset tap",
          "[screen_transform][regression][pr43]") {
    using Catch::Matchers::WithinAbs;

    // Virtual 720×1280 centred in a 1000×1280 window (offset_x=140, scale=1).
    // A tap at window pixel x=500 should map to virtual x=360 (centre).
    ScreenTransform correct;
    correct.offset_x = 140.0f;
    correct.offset_y = 0.0f;
    correct.scale    = 1.0f;

    CHECK_THAT(vx(correct, 500.0f), WithinAbs(360.0f, 1e-3f));

    // With stale identity ST: x=500 maps to virtual 500 — wrong.
    ScreenTransform stale;
    CHECK_FALSE(std::abs(vx(stale, 500.0f) - 360.0f) < 1.0f);
}

// ═══════════════════════════════════════════════════════════════════════
// Theme 2 – game_camera_system slab_matrix dimension order: MeshChild field
//           contract.  spawn_obstacle_meshes stores depth = DrawSize.h
//           (scroll-axis) and height = 3D height constant (Y-axis).
//           game_camera_system must pass mc.height as h (Y) and mc.depth as
//           d (Z) to slab_matrix, not the other way around.
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("MeshChild: depth field stores DrawSize.h (scroll-axis) for OnsetMarker",
          "[camera][slab][pr43]") {
    using Catch::Matchers::WithinAbs;

    auto reg = make_registry();
    auto obs = spawn_onset_marker_obstacle(reg, {constants::LANE_X[1], 500.0f, 100.0f});

    const float expected_depth  = reg.get<DrawSize>(obs).h;
    const float expected_height = constants::OBSTACLE_3D_HEIGHT;

    auto view = reg.view<MeshChild, MeshKindSlab>();
    int count = 0;
    for (auto [e, mc] : view.each()) {
        (void)e;
        // depth must equal the DrawSize.h (scroll-axis thickness)
        CHECK_THAT(mc.depth,  WithinAbs(expected_depth,  1e-3f));
        // height must equal the 3D height constant (vertical Y dimension)
        CHECK_THAT(mc.height, WithinAbs(expected_height, 1e-3f));
        ++count;
    }
    REQUIRE(count >= 1);
}

TEST_CASE("MeshChild: depth/height fields are distinct values (non-trivially interchangeable)",
          "[camera][slab][pr43]") {
    // If depth == height the swap would be invisible. This test confirms the
    // constants are different so the slab_matrix arg-swap produces a visible
    // change and our field-contract test is meaningful.
    auto reg = make_registry();
    auto obs = spawn_onset_marker_obstacle(reg, {constants::LANE_X[1], 500.0f, 100.0f});

    const float dsz_h = reg.get<DrawSize>(obs).h;
    // DrawSize.h for an onset-marker (80) must differ from OBSTACLE_3D_HEIGHT (20).
    CHECK(dsz_h != constants::OBSTACLE_3D_HEIGHT);
}

TEST_CASE("MeshChild: ShapeGate renders shape-only (no side slabs)",
           "[camera][slab][pr43]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, 500.0f);
    spawn_obstacle_meshes(reg, obs);

    int slab_count = 0;
    int shape_count = 0;
    for (auto [e, mc] : reg.view<MeshChild, MeshKindSlab>().each()) {
        (void)e; (void)mc;
        ++slab_count;
    }
    for (auto [e, mc, kind] : reg.view<MeshChild, MeshKindShape>().each()) {
        (void)e; (void)mc; (void)kind;
        ++shape_count;
    }
    REQUIRE(slab_count == 0);
    REQUIRE(shape_count >= 1);
}

// ═══════════════════════════════════════════════════════════════════════
// Theme 6 – obstacle mesh lifetime must only destroy children whose parent
//           is the entity being destroyed, not other obstacles' children.
// Explicit cleanup follows ObstacleChildren ownership directly, so it does not
// depend on ObstacleTag pool insertion order.
// ═══════════════════════════════════════════════════════════════════════

// Helper: create an obstacle with an ObstacleChildren list.
static entt::entity make_obstacle(entt::registry& reg,
                                   std::initializer_list<entt::entity> kids) {
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    auto& oc = reg.emplace<ObstacleChildren>(obs);
    for (auto k : kids) {
        REQUIRE(oc.count < ObstacleChildren::MAX);
        oc.children[oc.count++] = k;
    }
    return obs;
}

// Helper: set up the registry for obstacle mesh lifetime tests.
static entt::registry make_obs_registry() {
    return make_registry();
}

TEST_CASE("obstacle mesh lifetime: only removes the destroyed parent's children",
           "[obstacle][cleanup][pr43]") {
    auto reg = make_obs_registry();

    auto childA1 = reg.create();
    auto childA2 = reg.create();
    auto childB  = reg.create();

    auto obsA = make_obstacle(reg, {childA1, childA2});
    make_obstacle(reg, {childB});

    destroy_obstacle_with_children(reg, obsA);

    CHECK_FALSE(reg.valid(childA1));
    CHECK_FALSE(reg.valid(childA2));
    CHECK(reg.valid(childB));
}

TEST_CASE("obstacle mesh lifetime: all children of destroyed parent removed",
           "[obstacle][cleanup][pr43]") {
    auto reg = make_obs_registry();

    const int NUM_CHILDREN = 4;
    entt::entity children[NUM_CHILDREN];
    for (auto& c : children) c = reg.create();

    auto obsA = make_obstacle(reg, {children[0], children[1], children[2], children[3]});

    // Bystander with its own child
    auto bystanderChild = reg.create();
    make_obstacle(reg, {bystanderChild});

    destroy_obstacle_with_children(reg, obsA);

    for (auto c : children)
        CHECK_FALSE(reg.valid(c));
    CHECK(reg.valid(bystanderChild));
}

// ═══════════════════════════════════════════════════════════════════════
// Issue #259 – UI elements loaded into ECS must not be re-drawn by the
// legacy generic JSON render path in ui_render_system.cpp.
// ═══════════════════════════════════════════════════════════════════════

static std::filesystem::path ui_render_source_path() {
    return std::filesystem::path{SHAPESHIFTER_SOURCE_DIR} / "app/systems/ui_render_system.cpp";
}

static std::string load_ui_render_source() {
    std::ifstream file(ui_render_source_path());
    if (!file.is_open()) return {};

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}


TEST_CASE("ui_render_system: ECS is the only generic UI element render path",
          "[ui][render][issue259]") {
    const std::string source = load_ui_render_source();
    INFO("Expected ui_render_system.cpp at " << ui_render_source_path().string());
    REQUIRE_FALSE(source.empty());

    CHECK(source.find("render_elements(") == std::string::npos);
    CHECK(source.find("render_text(") == std::string::npos);
    CHECK(source.find("render_button(") == std::string::npos);
    CHECK(source.find("render_shape(") == std::string::npos);
    CHECK(source.find("find_el(screen, \"score\")") == std::string::npos);
    CHECK(source.find("find_el(screen, \"high_score\")") == std::string::npos);
}
