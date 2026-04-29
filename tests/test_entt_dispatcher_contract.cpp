// tests/test_entt_dispatcher_contract.cpp
//
// Verifies the entt::dispatcher semantics that the production input pipeline
// relies on.  Uses entt::dispatcher and the game's event types (InputEvent,
// GoEvent) directly — no game system calls — forming a stable,
// implementation-independent contract.
//
// Landed architecture (see game_loop.cpp + input_dispatcher.cpp):
//   - input_system enqueues InputEvent; game_loop calls disp.update<InputEvent>()
//     (Tier-1), which delivers to gesture_routing_handle_input (enqueues GoEvent)
//     and hit_test_handle_input (enqueues ButtonPressEvent).
//   - game_state_system (first in the fixed-step loop) calls
//     disp.update<GoEvent>() + disp.update<ButtonPressEvent>(), draining the
//     queues and delivering events to all registered listeners in a single tick.
//   - Cross-type enqueue (InputEvent listener enqueues GoEvent) is safe: the
//     "same-update() delivery hazard" only applies when a listener enqueues
//     the SAME type it is listening to.
//
// Key properties the dispatcher model introduces that these tests document:
//
//   1. enqueue() without update() silently drops events (not an error).
//   2. Events produced BY a listener during update() for the SAME type are NOT
//      delivered until the next update() call — the "one-frame latency" hazard.
//      The landed architecture routes gesture/hit-test as InputEvent listeners
//      that enqueue GoEvent/ButtonPressEvent (different types), so this hazard
//      cannot arise.
//   3. update() drains its queue; a second update() with no new enqueues is
//      a no-op.  This preserves the #213 no-replay invariant: calling
//      update() on the second sub-tick of the fixed-step accumulator does
//      not replay the same GoEvents / ButtonPressEvents.

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include "components/input_events.h"   // InputEvent, GoEvent, ButtonPressEvent
#include "test_helpers.h"              // make_rhythm_registry, make_rhythm_player

// ── Listener helpers ──────────────────────────────────────────────────────
// EnTT dispatcher.sink<T>().connect<&Struct::method>(instance) requires a
// member-function pointer; structs are the cleanest way to do this in tests.

struct GoCounter {
    int       count{0};
    Direction last{Direction::Up};
    void on_go(const GoEvent& e) { ++count; last = e.dir; }
};

struct PressCounter {
    int count{0};
    void on_press(const ButtonPressEvent& e) { ++count; (void)e; }
};

// Test helper: demonstrates the pool-order latency hazard when enqueue() is
// called inside a dispatcher listener.  Production gesture routing also
// uses enqueue(), but is invoked as a direct pre-tick function — not a
// listener — so this hazard never arises in production.
struct EnqueueRouter {
    entt::dispatcher* disp{nullptr};
    void on_input(const InputEvent& e) {
        if (e.type == InputType::Swipe)
            disp->enqueue(GoEvent{e.dir});
    }
};

// Test helper: demonstrates trigger() synchronous delivery inside a listener.
// Production gesture routing uses enqueue(), not trigger(); zero-frame
// latency is achieved by calling it as a direct pre-tick function instead.
struct TriggerRouter {
    entt::dispatcher* disp{nullptr};
    void on_input(const InputEvent& e) {
        if (e.type == InputType::Swipe)
            disp->trigger(GoEvent{e.dir});
    }
};

// ── Basic delivery ────────────────────────────────────────────────────────

TEST_CASE("dispatcher: trigger delivers GoEvent to all listeners immediately",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    GoCounter a, b;
    dispatcher.sink<GoEvent>().connect<&GoCounter::on_go>(a);
    dispatcher.sink<GoEvent>().connect<&GoCounter::on_go>(b);

    dispatcher.trigger(GoEvent{Direction::Left});

    CHECK(a.count == 1);
    CHECK(a.last  == Direction::Left);
    CHECK(b.count == 1);
    CHECK(b.last  == Direction::Left);
}

TEST_CASE("dispatcher: enqueue without update delivers nothing",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    GoCounter counter;
    dispatcher.sink<GoEvent>().connect<&GoCounter::on_go>(counter);

    dispatcher.enqueue(GoEvent{Direction::Right});

    // No update() call.
    CHECK(counter.count == 0);
}

TEST_CASE("dispatcher: enqueue then update delivers the event",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    GoCounter counter;
    dispatcher.sink<GoEvent>().connect<&GoCounter::on_go>(counter);

    dispatcher.enqueue(GoEvent{Direction::Down});
    dispatcher.update();

    CHECK(counter.count == 1);
    CHECK(counter.last  == Direction::Down);
}

TEST_CASE("dispatcher: multiple enqueued events all delivered in single update",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    GoCounter counter;
    dispatcher.sink<GoEvent>().connect<&GoCounter::on_go>(counter);

    dispatcher.enqueue(GoEvent{Direction::Left});
    dispatcher.enqueue(GoEvent{Direction::Right});
    dispatcher.enqueue(GoEvent{Direction::Up});
    dispatcher.update();

    CHECK(counter.count == 3);
}

// ── Drain / no-replay contract (#213 analog) ──────────────────────────────
//
// In the game's fixed-step accumulator a render frame may invoke the tick
// systems more than once.  After the dispatcher refactor, calling update()
// on the second sub-tick must NOT re-deliver GoEvents that were already
// dispatched on the first sub-tick.

TEST_CASE("dispatcher: update drains queue — second update does not replay events (#213 analog)",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    GoCounter counter;
    dispatcher.sink<GoEvent>().connect<&GoCounter::on_go>(counter);

    dispatcher.enqueue(GoEvent{Direction::Left});

    dispatcher.update();          // delivers once, drains queue
    CHECK(counter.count == 1);

    dispatcher.update();          // no new events — must be a no-op
    CHECK(counter.count == 1);    // not replayed
}

// ── One-frame latency hazard ───────────────────────────────────────────────
//
// EnTT v3.16.0 dispatches event-type pools in the order they were first
// registered (sink<T>() or enqueue<T>()).  If the GoEvent pool is registered
// BEFORE the InputEvent pool, update() processes the GoEvent pool first
// (empty at that point), then processes the InputEvent pool (fires the router
// which enqueues a GoEvent) — but the GoEvent pool has already run, so the
// new GoEvent is NOT delivered until the next update() call.
//
// This order-dependent one-frame latency is documented here as an EnTT
// contract property.  The production architecture avoids it entirely:
// gesture_routing_handle_input and hit_test_handle_input are InputEvent
// listeners that enqueue GoEvent/ButtonPressEvent (different types).
// The pool-order hazard only occurs when a listener enqueues the SAME type
// it is listening to — which never happens in production.

TEST_CASE("dispatcher: GoEvent pool registered before InputEvent pool — enqueue in listener introduces one-frame latency",
          "[entt_dispatcher][latency]") {
    entt::dispatcher dispatcher;

    // GoEvent pool registered FIRST: it will be processed first by update().
    GoCounter player;
    dispatcher.sink<GoEvent>().connect<&GoCounter::on_go>(player);

    // InputEvent pool registered SECOND.
    EnqueueRouter router{&dispatcher};
    dispatcher.sink<InputEvent>().connect<&EnqueueRouter::on_input>(router);

    dispatcher.enqueue(InputEvent{InputType::Swipe, Direction::Right, 0.f, 0.f});
    dispatcher.update();
    // update() order: GoEvent pool runs first (empty) → InputEvent pool runs
    // second (fires router → enqueues GoEvent).  GoEvent pool already done.

    CHECK(player.count == 0);   // latency: GoEvent not delivered until next update()

    dispatcher.update();        // second update: GoEvent pool now delivers
    CHECK(player.count == 1);
}

// ── EnTT property: trigger() in listener delivers synchronously ──────────────
//
// This test documents trigger()'s synchronous semantics for completeness.
// Production gesture routing uses enqueue() (not trigger()); zero-frame
// latency is achieved by delivering InputEvent routing before the fixed tick
// function before the fixed-step loop, so its enqueued GoEvents are ready to
// drain when game_state_system calls disp.update<GoEvent>() on the first tick.

TEST_CASE("dispatcher: trigger() in listener delivers GoEvent in same update() — synchronous delivery semantics",
          "[entt_dispatcher][latency]") {
    entt::dispatcher dispatcher;
    TriggerRouter router{&dispatcher};
    GoCounter player;

    dispatcher.sink<InputEvent>().connect<&TriggerRouter::on_input>(router);
    dispatcher.sink<GoEvent>().connect<&GoCounter::on_go>(player);

    dispatcher.enqueue(InputEvent{InputType::Swipe, Direction::Right, 0.f, 0.f});
    dispatcher.update();
    // router fired → GoEvent triggered immediately → player listener called
    // synchronously inside update().

    CHECK(player.count == 1);              // no latency
    CHECK(player.last  == Direction::Right);
}

// ── ButtonPressEvent delivery ─────────────────────────────────────────────

TEST_CASE("dispatcher: trigger ButtonPressEvent reaches listener immediately",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    PressCounter counter;
    dispatcher.sink<ButtonPressEvent>().connect<&PressCounter::on_press>(counter);

    dispatcher.trigger(ButtonPressEvent{ButtonPressKind::Shape, Shape::Circle});

    CHECK(counter.count == 1);
}

TEST_CASE("dispatcher: enqueue ButtonPressEvent without update delivers nothing",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    PressCounter counter;
    dispatcher.sink<ButtonPressEvent>().connect<&PressCounter::on_press>(counter);

    dispatcher.enqueue(ButtonPressEvent{ButtonPressKind::Shape, Shape::Circle});

    CHECK(counter.count == 0);
}

TEST_CASE("wire_input_dispatcher prewarms event queues without leaving pending events",
          "[entt_dispatcher][input_pipeline]") {
    auto reg = make_registry();
    auto& dispatcher = reg.ctx().get<entt::dispatcher>();

    CHECK(dispatcher.size<InputEvent>() == 0);
    CHECK(dispatcher.size<GoEvent>() == 0);
    CHECK(dispatcher.size<ButtonPressEvent>() == 0);
}

// ── R7: Stale-event discard across phase transitions ──────────────────────
//
// R7 contract: queued dispatcher events must not cause effects in the wrong
// gameplay phase after an authoritative phase transition.
//
// The production architecture satisfies R7 through two cooperating mechanisms:
//   1. Drain-first order: game_state_system calls disp.update<GoEvent>() BEFORE
//      processing transition_pending, so events queued in phase X are processed
//      in phase X's context — not leaked into phase Y.
//   2. Phase guards: player_input_handle_go and player_input_handle_press each
//      check gs.phase == Playing at entry and return early otherwise.  Even if
//      a GoEvent is delivered in a non-Playing phase, these guards prevent mutation.
//
// The three tests below document both mechanisms and their interaction.

TEST_CASE("R7: GoEvent delivered in GameOver phase — player_input_handle_go no-ops due to phase guard",
          "[dispatcher][R7][regression]") {
    // Arrange: simulate a stale GoEvent reaching the dispatcher after a
    // Playing→GameOver phase transition (e.g. transition happened without a
    // preceding drain, which can arise in test-harness or edge-case paths).
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane  = reg.get<Lane>(player);
    auto& gs    = reg.ctx().get<GameState>();
    auto& disp  = reg.ctx().get<entt::dispatcher>();

    // Transition to GameOver BEFORE enqueuing — simulating that the phase
    // boundary crossed before the input reached the drain.
    gs.phase = GamePhase::GameOver;

    disp.enqueue(GoEvent{Direction::Right});
    disp.update<GoEvent>();   // deliver in GameOver context

    // player_input_handle_go: phase != Playing → early return → lane unchanged
    CHECK(lane.target  == -1);   // default — never updated
    CHECK(lane.current == 1);    // unchanged
}

TEST_CASE("R7: drain-first order — GoEvent processed in pre-transition phase, queue empty post-transition",
          "[dispatcher][R7]") {
    // Documents production game_state_system behaviour: events are drained
    // BEFORE transition_pending is processed.  A GoEvent queued in Playing is
    // delivered while still in Playing (lane moves), then queue empties.
    // Any additional update() calls after the transition are no-ops —
    // no stale replay in the post-transition frame.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    auto& gs   = reg.ctx().get<GameState>();
    auto& disp = reg.ctx().get<entt::dispatcher>();

    REQUIRE(gs.phase == GamePhase::Playing);

    disp.enqueue(GoEvent{Direction::Right});

    // Drain first (mirrors game_state_system drain before transition check).
    disp.update<GoEvent>();
    CHECK(lane.target == 2);   // event processed in Playing phase → lane moved

    // Now perform the phase transition.
    gs.phase = GamePhase::GameOver;

    // Post-transition drain: queue already empty — must be a no-op.
    disp.update<GoEvent>();
    CHECK(lane.target == 2);   // unchanged: no stale GoEvent replayed
}

TEST_CASE("R7: two-tick stale-event regression — GoEvent from tick N absent in tick N+1",
          "[dispatcher][R7][regression]") {
    // Full two-tick sequence proving cross-tick isolation.
    //
    // Tick N (Playing): enqueue GoEvent → drain → lane.target moves to 2.
    // Phase transitions to GameOver between ticks.
    // Tick N+1 (GameOver): pre-tick produces no new events; drain is a no-op.
    //   Verify lane.target is still 2 — the tick-N event was not re-delivered.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    auto& gs   = reg.ctx().get<GameState>();
    auto& disp = reg.ctx().get<entt::dispatcher>();

    // ── Tick N ──────────────────────────────────────────────────────────────
    REQUIRE(gs.phase == GamePhase::Playing);
    disp.enqueue(GoEvent{Direction::Right});
    disp.update<GoEvent>();           // authoritative drain for tick N
    CHECK(lane.target == 2);

    // Partial lerp simulates player_movement_system advancing the interpolation.
    lane.lerp_t = 0.5f;

    // ── Phase transition ─────────────────────────────────────────────────────
    gs.phase = GamePhase::GameOver;

    // ── Tick N+1 ─────────────────────────────────────────────────────────────
    // No new GoEvents enqueued (pre-tick systems produced nothing new).
    disp.update<GoEvent>();           // post-transition drain — must be a no-op

    CHECK(lane.target  == 2);         // target unchanged: no stale replay
    CHECK(lane.lerp_t  == 0.5f);      // lerp_t not reset: no second lane move
    CHECK(lane.current == 1);         // current unchanged: no instant snap
}
