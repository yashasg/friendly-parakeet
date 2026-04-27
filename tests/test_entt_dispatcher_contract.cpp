// tests/test_entt_dispatcher_contract.cpp
//
// Verifies the entt::dispatcher semantics that the input pipeline refactor
// (#entt-input-model) must correctly implement.  Uses entt::dispatcher and
// the game's event types (InputEvent, GoEvent) directly — no game system
// calls — so every test compiles and passes against the current build and
// forms a stable, implementation-independent contract for Keaton's work.
//
// Key hazards the dispatcher model introduces that these tests document:
//
//   1. enqueue() without update() silently drops events (not an error).
//   2. Events produced BY a listener during update() are NOT delivered until
//      the next update() call — the "one-frame latency" hazard.  This is the
//      core architectural decision: gesture_routing must use trigger(), not
//      enqueue(), when emitting GoEvents so player_input_system receives them
//      in the same logical frame.
//   3. update() drains its queue; a second update() with no new enqueues is
//      a no-op.  This is the fix for the #213 fixed-step replay bug: once the
//      refactor lands, calling update() twice per render frame must not replay
//      the same GoEvents / ButtonPressEvents to player_input_system.

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

// Mimics gesture_routing_system routing via enqueue() — introduces latency.
struct EnqueueRouter {
    entt::dispatcher* disp{nullptr};
    void on_input(const InputEvent& e) {
        if (e.type == InputType::Swipe)
            disp->enqueue(GoEvent{e.dir});
    }
};

// Mimics gesture_routing_system routing via trigger() — no latency.
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
// This order-dependent one-frame latency is the core hazard Keaton must avoid.
// The safe fix is to use trigger() in intermediate listeners (see next test).

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

// ── Correct pattern: trigger() in intermediate listener eliminates latency ──
//
// gesture_routing_system must call dispatcher.trigger<GoEvent>() (not enqueue)
// when routing a swipe.  trigger() fires listeners synchronously, so the
// GoEvent reaches player_input_system within the same update() that delivered
// the InputEvent.

TEST_CASE("dispatcher: trigger() in listener delivers GoEvent in same update() — correct zero-latency pattern",
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
