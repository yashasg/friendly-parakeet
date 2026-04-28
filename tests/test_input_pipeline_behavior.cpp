// tests/test_input_pipeline_behavior.cpp
//
// End-to-end input pipeline behavioral tests.
//
// The pipeline: gesture_routing_handle_input → hit_test_handle_input →
// player_input_system (via entt::dispatcher Tier-1 update).
//
// All assertions are on player component state (Lane::target, ShapeWindow::phase,
// PlayerShape::current) — never on raw event queues.
// This makes the tests implementation-agnostic: push_input(reg,...) injects
// a raw InputEvent; run_input_tier1(reg) fires gesture_routing + hit_test
// listeners; player_input_system drains the resulting GoEvent/ButtonPressEvent
// queues.  The observable player-state outcomes are the stable contract.
//
// Failure modes these tests guard against:
//   - One-frame latency: swipe arrives but lane.target unchanged until next tick
//     (would occur if game_state_system's disp.update<GoEvent>() call were
//     removed or moved after player_input_handle_go runs)
//   - Dropped tap: tap arrives but sw.phase stays Idle
//     (would occur if hit_test_handle_input's enqueue<ButtonPressEvent> were dropped)
//   - Wrong-phase activation: button presses fire in an inactive phase
//   - Event replay: second pipeline tick resets lane.lerp_t (symptom: #213 bug)

#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// Runs the Tier-1 input dispatch and then the player_input_system drain.
// Mirrors the production order: update<InputEvent>() fires gesture_routing +
// hit_test; player_input_system calls update<GoEvent/ButtonPressEvent>().
static void run_pipeline(entt::registry& reg, float dt = 0.016f) {
    run_input_tier1(reg);
    player_input_system(reg, dt);
}

// ── Swipe → lane change ───────────────────────────────────────────────────

TEST_CASE("pipeline: swipe right produces lane change in same pipeline call — no latency",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    REQUIRE(lane.current == 1);

    push_input(reg, InputType::Swipe, 0.f, 0.f, Direction::Right);
    run_pipeline(reg);

    CHECK(lane.target == 2);   // same-frame: no latency between InputEvent and lane.target
}

TEST_CASE("pipeline: swipe left produces lane change in same pipeline call — no latency",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    REQUIRE(lane.current == 1);

    push_input(reg, InputType::Swipe, 0.f, 0.f, Direction::Left);
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

    push_input(reg, InputType::Swipe, 0.f, 0.f, Direction::Up);
    push_input(reg, InputType::Swipe, 0.f, 0.f, Direction::Down);
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

    push_input(reg, InputType::Swipe, 0.f, 0.f, Direction::Right);
    run_pipeline(reg);

    // At boundary delta==0: player_input_system skips the assignment block.
    CHECK(lane.target == constants::LANE_COUNT - 1);
}

// ── Tap → hit-test → shape change ─────────────────────────────────────────

TEST_CASE("pipeline: tap on active button triggers shape change in same pipeline call",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    REQUIRE(sw.phase == WindowPhase::Idle);

    auto btn = make_shape_button(reg, Shape::Square);
    reg.get<UIPosition>(btn).value = {200.f, 200.f};
    reg.get<HitCircle>(btn).radius = 40.f;
    invalidate_active_tag_cache(reg);

    push_input(reg, InputType::Tap, 200.f, 200.f);
    run_pipeline(reg);

    CHECK(sw.phase        == WindowPhase::MorphIn);
    CHECK(sw.target_shape == Shape::Square);
}

TEST_CASE("pipeline: tap on button inactive in current phase has no effect",
          "[input_pipeline]") {
    // Button registered for Playing only; current phase is Title.
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    make_rhythm_player(reg);
    // player_input_system exits early when phase != Playing, so we only
    // care that no press bleeds through.

    auto btn = make_shape_button(reg, Shape::Square);  // mask = Playing
    reg.get<UIPosition>(btn).value = {100.f, 100.f};
    reg.get<HitCircle>(btn).radius = 40.f;
    invalidate_active_tag_cache(reg);

    push_input(reg, InputType::Tap, 100.f, 100.f);
    run_pipeline(reg);

    // hit_test_handle_input filtered it out (ActiveTag absent in Title) AND
    // player_input_system returned early.  Either guard alone is sufficient;
    // the test asserts the observable outcome (no shape change).
    auto view = reg.view<PlayerTag, ShapeWindow>();
    for (auto [e, sw] : view.each()) {
        CHECK(sw.phase == WindowPhase::Idle);
    }
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

    auto btn = make_shape_button(reg, Shape::Triangle);
    reg.get<UIPosition>(btn).value = {200.f, 200.f};
    reg.get<HitCircle>(btn).radius = 40.f;
    invalidate_active_tag_cache(reg);

    push_input(reg, InputType::Swipe, 0.f,   0.f,   Direction::Right);  // lane +1
    push_input(reg, InputType::Tap,   200.f, 200.f);                     // shape Triangle

    run_pipeline(reg);

    CHECK(lane.target     == 2);                  // swipe processed
    CHECK(sw.phase        == WindowPhase::MorphIn); // tap processed
    CHECK(sw.target_shape == Shape::Triangle);
}

// ── No-latency regression ─────────────────────────────────────────────────
//
// If the refactor introduces a one-frame latency (e.g., GoEvent enqueued but
// update() not called before player_input_system runs), lane.target will still
// equal lane.current after this call.  The check "lane.target != lane.current"
// is the regression signal.

TEST_CASE("pipeline: swipe effect visible immediately — lane.target differs from lane.current after single pipeline call",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    REQUIRE(lane.current == 1);

    push_input(reg, InputType::Swipe, 0.f, 0.f, Direction::Left);
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
// After update<InputEvent>() drains the Tier-1 queue, the queue is empty.
// Subsequent calls to run_input_tier1 are no-ops.  GoEvent/ButtonPressEvent
// queues are also drained by player_input_system, so no replay occurs.

TEST_CASE("pipeline: swipe consumed after first sub-tick — second sub-tick does not reset lane.lerp_t (#213 behavior)",
          "[input_pipeline]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);

    // Sub-tick 1: inject swipe and run pipeline.
    push_input(reg, InputType::Swipe, 0.f, 0.f, Direction::Right);
    run_pipeline(reg);
    CHECK(lane.target  == 2);
    CHECK(lane.lerp_t  == 0.0f);

    // Simulate partial lerp (as player_movement_system would produce).
    lane.lerp_t = 0.4f;

    // Sub-tick 2: InputEvent queue already drained by sub-tick 1's update.
    // No new inputs pushed — run_pipeline is a no-op for lane changes.
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

    auto btn = make_shape_button(reg, Shape::Circle);
    reg.get<UIPosition>(btn).value = {100.f, 100.f};
    reg.get<HitCircle>(btn).radius = 40.f;
    invalidate_active_tag_cache(reg);

    // Sub-tick 1: tap opens MorphIn.
    push_input(reg, InputType::Tap, 100.f, 100.f);
    run_pipeline(reg);
    CHECK(sw.phase        == WindowPhase::MorphIn);
    float start1 = sw.window_start;

    // Advance song time so a replayed window would produce a different window_start.
    song.song_time = 6.0f;

    // Sub-tick 2: InputEvent queue already drained — run_pipeline is a no-op.
    run_pipeline(reg);

    CHECK(sw.window_start == start1);  // window_start unchanged: event not replayed
}
