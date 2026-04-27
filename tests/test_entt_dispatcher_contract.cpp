// tests/test_entt_dispatcher_contract.cpp
//
// Verifies the entt::dispatcher semantics that the production input pipeline
// relies on.  Uses entt::dispatcher and the game's event types (InputEvent,
// GoEvent) directly — no game system calls — forming a stable,
// implementation-independent contract.
//
// Landed architecture (see game_loop.cpp + game_state_system.cpp):
//   - gesture_routing_system and hit_test_system are called directly as
//     pre-tick system functions (before the fixed-step accumulator loop).
//     They read the raw EventQueue and call disp.enqueue<GoEvent/ButtonPressEvent>().
//   - game_state_system (first in the fixed-step loop) calls
//     disp.update<GoEvent>() + disp.update<ButtonPressEvent>(), draining the
//     queues and delivering events to all registered listeners in a single tick.
//   - There is no InputEvent tier in the dispatcher: gesture routing and
//     hit-testing are direct pre-tick calls, not dispatcher listeners.  The
//     pool-order latency hazard (see below) is therefore never triggered.
//
// Key properties the dispatcher model introduces that these tests document:
//
//   1. enqueue() without update() silently drops events (not an error).
//   2. Events produced BY a listener during update() are NOT delivered until
//      the next update() call — the "one-frame latency" hazard.  The landed
//      architecture avoids this by routing gesture/hit-test through direct
//      pre-tick calls rather than dispatcher listeners, so the pool-order
//      constraint never arises in production.
//   3. update() drains its queue; a second update() with no new enqueues is
//      a no-op.  This preserves the #213 no-replay invariant: calling
//      update() on the second sub-tick of the fixed-step accumulator does
//      not replay the same GoEvents / ButtonPressEvents.

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include "components/input_events.h"   // InputEvent, GoEvent, ButtonPressEvent

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
// called inside a dispatcher listener.  Production gesture_routing_system also
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
// Production gesture_routing_system uses enqueue(), not trigger(); zero-frame
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
// gesture_routing_system and hit_test_system are called as direct pre-tick
// functions (not as dispatcher listeners), so the InputEvent pool is never
// registered in the dispatcher and the pool-order constraint cannot arise.

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
// Production gesture_routing_system uses enqueue() (not trigger()); zero-frame
// latency is achieved by calling gesture_routing_system directly as a pre-tick
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

    dispatcher.trigger(ButtonPressEvent{entt::null});

    CHECK(counter.count == 1);
}

TEST_CASE("dispatcher: enqueue ButtonPressEvent without update delivers nothing",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    PressCounter counter;
    dispatcher.sink<ButtonPressEvent>().connect<&PressCounter::on_press>(counter);

    dispatcher.enqueue(ButtonPressEvent{entt::null});

    CHECK(counter.count == 0);
}
