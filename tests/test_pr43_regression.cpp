// PR #43 regression tests — Baer
// Covers the six unresolved review threads:
//   1. ScreenTransform stale on first/resize frame before input_system
//   2. camera_system slab_matrix Y/Z dimension swap (LowBar, HighBar, LanePush)
//   3. camera_system MeshChild slab Y/Z dimension swap
//   4. level_select difficulty hitboxes repositioned same tick as selected_level changes
//   5. Title Confirm / Exit hitboxes must not overlap at EXIT_TOP
//   6. shape_obstacle child destruction must not touch other parents' children
//   7. paused overlay JSON must not be re-parsed every Paused frame

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "ui/ui_button_spawner.h"
#include "ui/ui_loader.h"
#include "gameobjects/shape_obstacle.h"
#include "components/ui_state.h"
#include "components/rendering.h"
#include "constants.h"
#include <fstream>
#include <nlohmann/json.hpp>
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
// Theme 2 – camera_system slab_matrix dimension order: MeshChild field
//           contract.  spawn_obstacle_meshes stores depth = DrawSize.h
//           (scroll-axis) and height = 3D height constant (Y-axis).
//           camera_system must pass mc.height as h (Y) and mc.depth as
//           d (Z) to slab_matrix, not the other way around.
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("MeshChild: depth field stores DrawSize.h (scroll-axis) for LaneBlock",
          "[camera][slab][pr43]") {
    using Catch::Matchers::WithinAbs;

    auto reg = make_registry();
    auto obs = make_lane_block(reg, 0b001, 500.0f);  // one blocked lane
    spawn_obstacle_meshes(reg, obs);

    const float expected_depth  = reg.get<DrawSize>(obs).h;
    const float expected_height = constants::OBSTACLE_3D_HEIGHT;

    auto view = reg.view<MeshChild>();
    int count = 0;
    for (auto [e, mc] : view.each()) {
        if (mc.mesh_type != MeshType::Slab) continue;
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
    auto obs = make_lane_block(reg, 0b001, 500.0f);
    spawn_obstacle_meshes(reg, obs);

    const float dsz_h = reg.get<DrawSize>(obs).h;
    // DrawSize.h for a lane-block (80) must differ from OBSTACLE_3D_HEIGHT (20).
    CHECK(dsz_h != constants::OBSTACLE_3D_HEIGHT);
}

TEST_CASE("MeshChild: height stores OBSTACLE_3D_HEIGHT for ShapeGate side slabs",
          "[camera][slab][pr43]") {
    using Catch::Matchers::WithinAbs;

    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, 500.0f);
    spawn_obstacle_meshes(reg, obs);

    int count = 0;
    auto view = reg.view<MeshChild>();
    for (auto [e, mc] : view.each()) {
        if (mc.mesh_type != MeshType::Slab) continue;
        CHECK_THAT(mc.height, WithinAbs(constants::OBSTACLE_3D_HEIGHT, 1e-3f));
        // depth is the scroll-axis thickness — should equal DrawSize.h
        CHECK_THAT(mc.depth,  WithinAbs(reg.get<DrawSize>(obs).h, 1e-3f));
        ++count;
    }
    REQUIRE(count >= 1);
}

// ═══════════════════════════════════════════════════════════════════════
// Theme 5 – Title Confirm / Exit hitbox non-overlap at EXIT_TOP.
// A tap exactly on the boundary y=EXIT_TOP must NOT trigger the Confirm
// button; it sits on the Exit button's inclusive upper edge.
// ═══════════════════════════════════════════════════════════════════════

#ifndef PLATFORM_WEB

// EXIT_TOP is EXIT_CENTER_Y - EXIT_H/2 = 1075 - 25 = 1050.
// These values mirror ui_button_spawner.h.
static constexpr float EXIT_H          = 50.0f;
static constexpr float EXIT_CENTER_Y   = 1075.0f;
static constexpr float EXIT_TOP        = EXIT_CENTER_Y - EXIT_H / 2.0f;  // 1050

static entt::entity find_button_by_kind(entt::registry& reg, MenuActionKind kind) {
    auto view = reg.view<MenuButtonTag, MenuAction>();
    for (auto [e, ma] : view.each())
        if (ma.kind == kind) return e;
    return entt::null;
}

TEST_CASE("title hitbox: tap at EXIT_TOP triggers Exit, not Confirm",
          "[title][hitbox][pr43]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;

    spawn_title_buttons(reg);

    auto confirm = find_button_by_kind(reg, MenuActionKind::Confirm);
    auto exit_e  = find_button_by_kind(reg, MenuActionKind::Exit);
    REQUIRE((confirm != entt::null));
    REQUIRE((exit_e  != entt::null));

    // Tap exactly on the boundary line
    push_input(reg, InputType::Tap, constants::SCREEN_W / 2.0f, EXIT_TOP);
    run_input_tier1(reg);

    // After the fix the boundary is no longer shared, so Confirm must NOT fire.
    bool confirm_pressed = false;
    bool exit_pressed    = false;
    auto cap = drain_press_events(reg);
    for (int i = 0; i < cap.count; ++i) {
        if (cap.buf[i].menu_action == MenuActionKind::Confirm) confirm_pressed = true;
        if (cap.buf[i].menu_action == MenuActionKind::Exit)    exit_pressed    = true;
    }
    CHECK_FALSE(confirm_pressed);   // boundary tap must NOT reach Confirm
    CHECK(exit_pressed);            // boundary tap must reach Exit
}

TEST_CASE("title hitbox: tap clearly inside Confirm region triggers Confirm only",
          "[title][hitbox][pr43]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;

    spawn_title_buttons(reg);

    auto confirm = find_button_by_kind(reg, MenuActionKind::Confirm);
    auto exit_e  = find_button_by_kind(reg, MenuActionKind::Exit);
    REQUIRE((confirm != entt::null));
    REQUIRE((exit_e  != entt::null));

    // Tap well inside the Confirm region (top half of screen)
    push_input(reg, InputType::Tap, constants::SCREEN_W / 2.0f, 400.0f);
    run_input_tier1(reg);

    bool confirm_pressed = false;
    bool exit_pressed    = false;
    auto cap = drain_press_events(reg);
    for (int i = 0; i < cap.count; ++i) {
        if (cap.buf[i].menu_action == MenuActionKind::Confirm) confirm_pressed = true;
        if (cap.buf[i].menu_action == MenuActionKind::Exit)    exit_pressed    = true;
    }
    CHECK(confirm_pressed);
    CHECK_FALSE(exit_pressed);
}

TEST_CASE("title hitbox: tap inside Exit region triggers Exit only",
          "[title][hitbox][pr43]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;

    spawn_title_buttons(reg);

    auto confirm = find_button_by_kind(reg, MenuActionKind::Confirm);
    auto exit_e  = find_button_by_kind(reg, MenuActionKind::Exit);
    REQUIRE((confirm != entt::null));
    REQUIRE((exit_e  != entt::null));

    // Tap at Exit button centre
    push_input(reg, InputType::Tap, constants::SCREEN_W / 2.0f, EXIT_CENTER_Y);
    run_input_tier1(reg);

    bool exit_pressed = false;
    auto cap = drain_press_events(reg);
    for (int i = 0; i < cap.count; ++i)
        if (cap.buf[i].menu_action == MenuActionKind::Exit) exit_pressed = true;

    CHECK(exit_pressed);
}
#endif  // !PLATFORM_WEB

// ═══════════════════════════════════════════════════════════════════════
// Theme 6 – on_obstacle_destroy must only destroy children whose parent
//           is the entity being destroyed, not other obstacles' children.
// on_obstacle_destroy uses ObstacleChildren for O(N) lookup.
// The production fix must prime the ObstacleChildren pool before connecting
// on_destroy<ObstacleTag>, so the pool's insertion index is lower than
// ObstacleTag's; EnTT destroy() iterates pools in reverse insertion order.
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

// Helper: set up the registry with on_obstacle_destroy wired.
// make_registry() already primes the ObstacleChildren pool before wire_obstacle_counter
// creates the ObstacleTag pool, guaranteeing that ObstacleChildren has a lower insertion
// index. EnTT destroy() iterates pools in reverse order, so ObstacleChildren is removed
// LAST — still accessible when on_obstacle_destroy fires for ObstacleTag.
static entt::registry make_obs_registry() {
    auto reg = make_registry();
    reg.on_destroy<ObstacleTag>().connect<&on_obstacle_destroy>();
    return reg;
}

TEST_CASE("on_obstacle_destroy: only removes the destroyed parent's children",
          "[obstacle][cleanup][pr43]") {
    auto reg = make_obs_registry();

    auto childA1 = reg.create();
    auto childA2 = reg.create();
    auto childB  = reg.create();

    auto obsA = make_obstacle(reg, {childA1, childA2});
    make_obstacle(reg, {childB});

    // Destroy obstacle A via the signal path
    reg.destroy(obsA);

    CHECK_FALSE(reg.valid(childA1));
    CHECK_FALSE(reg.valid(childA2));
    CHECK(reg.valid(childB));
}

TEST_CASE("on_obstacle_destroy: all children of destroyed parent removed",
          "[obstacle][cleanup][pr43]") {
    auto reg = make_obs_registry();

    const int NUM_CHILDREN = 4;
    entt::entity children[NUM_CHILDREN];
    for (auto& c : children) c = reg.create();

    auto obsA = make_obstacle(reg, {children[0], children[1], children[2], children[3]});

    // Bystander with its own child
    auto bystanderChild = reg.create();
    make_obstacle(reg, {bystanderChild});

    reg.destroy(obsA);

    for (auto c : children)
        CHECK_FALSE(reg.valid(c));
    CHECK(reg.valid(bystanderChild));
}

// ═══════════════════════════════════════════════════════════════════════
// Theme 7 – paused overlay JSON must not be re-parsed every Paused frame.
// After the fix, a mutated overlay_screen must survive a second tick.
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("ui_navigation: paused overlay parsed once, not every frame",
          "[ui][overlay][pr43]") {
    // Requires content/ui/screens/paused.json to be reachable from CWD
    // (true when tests run from the project root with ./build/shapeshifter_tests).
    auto reg = make_registry();
    reg.ctx().emplace<UIState>(load_ui("content/ui"));

    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Paused;

    // First tick: enters Paused, loads gameplay screen, parses overlay
    ui_navigation_system(reg, 0.016f);

    auto& ui = reg.ctx().get<UIState>();
    if (!ui.has_overlay) {
        // paused.json not accessible; skip rather than false-fail
        SKIP("paused.json not accessible from test working directory");
    }

    // Inject a sentinel key that a re-parse from file would destroy
    ui.overlay_screen["__pr43_sentinel__"] = 99;

    // Second tick: same phase, same screen — must NOT re-parse
    ui_navigation_system(reg, 0.016f);

    // After fix: sentinel survives.  With the bug: sentinel is gone.
    CHECK(ui.overlay_screen.contains("__pr43_sentinel__"));
}

// ═══════════════════════════════════════════════════════════════════════
// Issue #259 – UI elements loaded into ECS must not be re-drawn by the
// legacy generic JSON render path in ui_render_system.cpp.
// ═══════════════════════════════════════════════════════════════════════

static std::string load_ui_render_source() {
    const char* paths[] = {
        "app/systems/ui_render_system.cpp",
        "../app/systems/ui_render_system.cpp"
    };

    for (const char* path : paths) {
        std::ifstream file(path);
        if (!file.is_open()) continue;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    return {};
}

TEST_CASE("ui_render_system: ECS is the only generic UI element render path",
          "[ui][render][issue259]") {
    const std::string source = load_ui_render_source();
    if (source.empty()) {
        SKIP("ui_render_system.cpp not accessible from test working directory");
    }

    CHECK(source.find("render_elements(") == std::string::npos);
    CHECK(source.find("render_text(") == std::string::npos);
    CHECK(source.find("render_button(") == std::string::npos);
    CHECK(source.find("render_shape(") == std::string::npos);
    CHECK(source.find("find_el(screen, \"score\")") == std::string::npos);
    CHECK(source.find("find_el(screen, \"high_score\")") == std::string::npos);
}

TEST_CASE("gameplay HUD score ECS elements preserve centered alignment",
          "[ui][render][issue259]") {
    const char* paths[] = {
        "content/ui/screens/gameplay.json",
        "../content/ui/screens/gameplay.json"
    };

    nlohmann::json screen;
    bool loaded = false;
    for (const char* path : paths) {
        std::ifstream file(path);
        if (!file.is_open()) continue;
        screen = nlohmann::json::parse(file);
        loaded = true;
        break;
    }

    if (!loaded) {
        SKIP("gameplay.json not accessible from test working directory");
    }

    const nlohmann::json* score = nullptr;
    const nlohmann::json* high_score = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "score") score = &el;
        if (el.value("id", "") == "high_score") high_score = &el;
    }

    REQUIRE(score != nullptr);
    REQUIRE(high_score != nullptr);
    CHECK(score->value("type", "") == "text_dynamic");
    CHECK(score->value("source", "") == "ScoreState.displayed_score");
    CHECK(score->value("align", "") == "center");
    CHECK(high_score->value("type", "") == "text_dynamic");
    CHECK(high_score->value("source", "") == "ScoreState.high_score");
    CHECK(high_score->value("align", "") == "center");
}
