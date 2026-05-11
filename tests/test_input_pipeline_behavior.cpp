// tests/test_input_pipeline_behavior.cpp
//
// End-to-end input pipeline behavioral tests.
//
// The pipeline: semantic GoEvent/ButtonPressEvent producers →
// game_state_system drain (authoritative) → player_input listeners.
//
// All assertions are on player component state (Lane::target, ShapeWindow::phase,
// PlayerShape::current) — never on raw event queues.
// This makes the tests implementation-agnostic: helpers enqueue semantic
// events directly; game_state_system drains the queues and listeners mutate
// player state in the same fixed tick. The observable outcomes are the
// stable contract.
//
// Failure modes these tests guard against:
//   - One-frame latency: swipe arrives but lane.target unchanged until next tick
//     (would occur if game_state_system's disp.update<GoEvent>() call were
//     removed or moved after player_input_handle_go runs)
//   - Dropped semantic shape press: ButtonPressEvent arrives but sw.phase stays Idle
//   - Wrong-phase activation: button presses fire in an inactive phase
//   - Event replay: second pipeline tick resets lane.lerp_t (symptom: #213 bug)

#include <catch2/catch_test_macros.hpp>
#include "input/keyboard_shape_mapping.h"
#include "test_helpers.h"
#include "ui/screen_controllers/gameplay_hud_screen_controller.h"
#include "util/shape_lane_mapping.h"

// Runs the fixed-tick input path.
static void run_pipeline(entt::registry& reg, float dt = 0.016f) {
    game_state_system(reg, dt);
}

TEST_CASE("pipeline: keyboard shape shortcuts follow left-center-right lanes",
          "[input_pipeline][keyboard][issue662]") {
    CHECK(lane_for_shape(shape_for_keyboard_slot(KeyboardShapeSlot::Left)) == 0);
    CHECK(lane_for_shape(shape_for_keyboard_slot(KeyboardShapeSlot::Center)) == 1);
    CHECK(lane_for_shape(shape_for_keyboard_slot(KeyboardShapeSlot::Right)) == 2);
}

// ── Swipe → lane change ───────────────────────────────────────────────────

TEST_CASE("pipeline: swipe right produces lane change in same pipeline call — no latency",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    REQUIRE(lane.current == 1);

    push_go(reg, Direction::Right);
    run_pipeline(reg);

    CHECK(lane.target == 2);
}

TEST_CASE("pipeline: swipe left produces lane change in same pipeline call — no latency",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    REQUIRE(lane.current == 1);

    push_go(reg, Direction::Left);
    run_pipeline(reg);

    CHECK(lane.target == 0);
}

TEST_CASE("pipeline: swipe Up/Down has no lane effect",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);

    // Settle lane.target to current so we have a clear baseline.
    lane.target = lane.current;   // both == 1

    push_go(reg, Direction::Up);
    push_go(reg, Direction::Down);
    run_pipeline(reg);

    CHECK(lane.target == 1);   // unchanged: Up/Down produce no lane delta
}

TEST_CASE("pipeline: swipe right at right boundary does not wrap lane",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);

    // Settle player at rightmost lane.
    lane.current = static_cast<int8_t>(constants::LANE_COUNT - 1);
    lane.target  = lane.current;

    push_go(reg, Direction::Right);
    run_pipeline(reg);

    // At boundary delta==0: the go-event handler skips the assignment block.
    CHECK(lane.target == constants::LANE_COUNT - 1);
}

// ── Semantic shape press → shape change ───────────────────────────────────

TEST_CASE("pipeline: semantic shape press triggers shape change in same pipeline call",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    REQUIRE(sw.phase == WindowPhase::Idle);

    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>(
        {ButtonPressKind::Shape, Shape::Square, MenuActionKind::Confirm, 0});
    run_pipeline(reg);

    CHECK(sw.phase        == WindowPhase::Active);
    CHECK(sw.target_shape == Shape::Square);
}

TEST_CASE("pipeline: semantic shape press remaps lane target to shape lane",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    REQUIRE(lane.current == 1);

    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>(
        {ButtonPressKind::Shape, Shape::Circle, MenuActionKind::Confirm, 0});
    run_pipeline(reg);

    CHECK(lane.target == 2);
}

TEST_CASE("pipeline: gameplay HUD raygui shape press triggers player shape input",
          "[input_pipeline][hud]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    REQUIRE(sw.phase == WindowPhase::Idle);
    gameplay_hud_apply_button_presses(reg,
                                      false,
                                      false,
                                      true,
                                      false);
    run_pipeline(reg);

    CHECK(sw.phase == WindowPhase::Active);
    CHECK(sw.target_shape == Shape::Square);
}

TEST_CASE("pipeline: gameplay HUD shape tap uses slot rectangle bounds",
          "[input_pipeline][hud]") {
    const auto circle_input_bounds = gameplay_hud_shape_input_bounds(GameplayHudShapeSlot::Circle);
    const Vector2 tap_center = {200.0f, 1190.0f};
    const Vector2 tap_plus_49 = {200.0f, 1239.0f};
    const Vector2 tap_plus_51 = {200.0f, 1241.0f};

    CHECK(CheckCollisionPointRec(tap_center, circle_input_bounds));
    CHECK(CheckCollisionPointRec(tap_plus_49, circle_input_bounds));
    CHECK_FALSE(CheckCollisionPointRec(tap_plus_51, circle_input_bounds));
}

TEST_CASE("pipeline: gameplay HUD shape geometry matches gameplay.rgl slots",
          "[input_pipeline][hud]") {
    const auto circle_bounds = gameplay_hud_shape_input_bounds(GameplayHudShapeSlot::Circle);
    const auto square_bounds = gameplay_hud_shape_input_bounds(GameplayHudShapeSlot::Square);
    const auto triangle_bounds = gameplay_hud_shape_input_bounds(GameplayHudShapeSlot::Triangle);

    CHECK(circle_bounds.x == 130.0f);
    CHECK(square_bounds.x == 290.0f);
    CHECK(triangle_bounds.x == 450.0f);
    CHECK(circle_bounds.y == 1140.0f);
    CHECK(square_bounds.y == 1140.0f);
    CHECK(triangle_bounds.y == 1140.0f);

    CHECK(circle_bounds.width == 140.0f);
    CHECK(circle_bounds.height == 100.0f);
}

TEST_CASE("pipeline: gameplay HUD proximity ring progresses through far near perfect",
          "[input_pipeline][hud]") {
    SongState song{};
    song.scroll_speed = 400.0f;
    song.morph_duration = 0.12f;
    song.half_window = 0.15f;

    const float perfect_dist = gameplay_hud_perfect_distance(&song);
    CHECK(perfect_dist > 107.99f);
    CHECK(perfect_dist < 108.01f);

    const float appear_dist = constants::APPROACH_DIST;
    const float near_dist = perfect_dist + (appear_dist - perfect_dist) * 0.2f;

    CHECK(gameplay_hud_ring_cue(900.0f, perfect_dist, appear_dist) == GameplayHudRingCue::Far);
    CHECK(gameplay_hud_ring_cue(near_dist, perfect_dist, appear_dist) == GameplayHudRingCue::Near);
    CHECK(gameplay_hud_ring_cue(perfect_dist, perfect_dist, appear_dist) == GameplayHudRingCue::Perfect);
}

TEST_CASE("pipeline: gameplay HUD raygui shape presses ignored outside Playing",
          "[input_pipeline][hud]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Paused;
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    REQUIRE(sw.phase == WindowPhase::Idle);
    gameplay_hud_apply_button_presses(reg,
                                      false,
                                      false,
                                      true,
                                      false);

    CHECK(sw.phase == WindowPhase::Idle);
}

// ── Mixed swipe + tap in same frame ───────────────────────────────────────

TEST_CASE("pipeline: mixed swipe and tap both take effect within a single pipeline call",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    auto& sw   = reg.get<ShapeWindow>(player);
    REQUIRE(lane.current == 1);
    REQUIRE(sw.phase == WindowPhase::Idle);

    push_go(reg, Direction::Right);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>(
        {ButtonPressKind::Shape, Shape::Triangle, MenuActionKind::Confirm, 0});

    run_pipeline(reg);

    CHECK(lane.target     == 0);                  // shape auto-target wins
    CHECK(sw.phase        == WindowPhase::Active); // tap processed
    CHECK(sw.target_shape == Shape::Triangle);
}

// ── No-latency regression ─────────────────────────────────────────────────
//
// If the refactor introduces a one-frame latency (e.g., GoEvent enqueued but
// the authoritative game_state_system drain does not run), lane.target will still
// equal lane.current after this call.  The check "lane.target != lane.current"
// is the regression signal.

TEST_CASE("pipeline: swipe effect visible immediately — lane.target differs from lane.current after single pipeline call",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    REQUIRE(lane.current == 1);

    push_go(reg, Direction::Left);
    run_pipeline(reg);

    CHECK(lane.target != lane.current);  // must differ: effect is immediate, not deferred
    CHECK(lane.target == 0);
}

// ── Event consumption: no replay across sub-ticks (#213) ──────────────────
//
// The fixed-step accumulator may invoke pipeline systems more than once per
// render frame.  After the first sub-tick consumes a swipe, subsequent
// sub-ticks must NOT replay it and must not reset lane.lerp_t.
//
// After game_state_system drains semantic queues, a second sub-tick with no
// new events is a no-op and must not replay.

TEST_CASE("pipeline: swipe consumed after first sub-tick — second sub-tick does not reset lane.lerp_t (#213 behavior)",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);

    // Sub-tick 1: inject swipe and run pipeline.
    push_go(reg, Direction::Right);
    run_pipeline(reg);
    CHECK(lane.target  == 2);
    CHECK(lane.lerp_t  == 0.0f);

    // Simulate partial lerp (as player_movement_system would produce).
    lane.lerp_t = 0.4f;

    // Sub-tick 2: no new semantic inputs pushed — run_pipeline is a no-op.
    run_pipeline(reg);

    CHECK(lane.lerp_t == 0.4f);  // not reset: swipe was consumed on sub-tick 1
    CHECK(lane.target == 2);     // target unchanged
}

TEST_CASE("pipeline: tap consumed after first sub-tick — second sub-tick does not re-open shape window (#213 behavior)",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw   = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 5.0f;

    // Sub-tick 1: tap opens Active.
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>(
        {ButtonPressKind::Shape, Shape::Circle, MenuActionKind::Confirm, 0});
    run_pipeline(reg);
    CHECK(sw.phase        == WindowPhase::Active);
    float start1 = sw.window_start;

    // Advance song time so a replayed window would produce a different window_start.
    song.song_time = 6.0f;

    // Sub-tick 2: no new semantic events — run_pipeline is a no-op.
    run_pipeline(reg);

    CHECK(sw.window_start == start1);  // window_start unchanged: event not replayed
}
