// tests/test_entt_dispatcher_contract.cpp
//
// Verifies entt::dispatcher semantics relied on by the semantic input
// pipeline (per-direction Go*Event + per-shape ShapePress*Event + per-action
// menu events only). Uses GoRightEvent as the test subject for the
// dispatcher-contract cases; semantics are identical across the four
// per-direction event types (#1279 split).

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include "components/game_state.h"
#include "systems/input_events.h"
#include "systems/scoring_system.h"
#include "systems/obstacle_despawn_system.h"
#include "systems/popup_display_system.h"
#include "systems/particle_system.h"
#include "test_helpers.h"

struct GoCounter {
    int count{0};
    void on_go(const GoRightEvent&) { ++count; }
};

struct PressCounter {
    int count{0};
    void on_press(const ShapePressCircleEvent&) { ++count; }
};

struct OrderedListener {
    int* order;
    int marker;
    int* cursor;
    void on_go(const GoRightEvent&) { order[(*cursor)++] = marker; }
};

struct ReenqueueGoListener {
    entt::dispatcher* disp{nullptr};
    int seen{0};
    void on_go(const GoRightEvent&) {
        ++seen;
        if (seen == 1) {
            disp->enqueue(GoRightEvent{});
        }
    }
};

TEST_CASE("dispatcher: trigger delivers Go*Event to all listeners immediately",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    GoCounter a, b;
    dispatcher.sink<GoRightEvent>().connect<&GoCounter::on_go>(a);
    dispatcher.sink<GoRightEvent>().connect<&GoCounter::on_go>(b);

    dispatcher.trigger(GoRightEvent{});

    CHECK(a.count == 1);
    CHECK(b.count == 1);
}

TEST_CASE("dispatcher: enqueue without update delivers nothing",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    GoCounter counter;
    dispatcher.sink<GoRightEvent>().connect<&GoCounter::on_go>(counter);

    dispatcher.enqueue(GoRightEvent{});

    CHECK(counter.count == 0);
}

TEST_CASE("dispatcher: enqueue then update delivers the event",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    GoCounter counter;
    dispatcher.sink<GoRightEvent>().connect<&GoCounter::on_go>(counter);

    dispatcher.enqueue(GoRightEvent{});
    dispatcher.update();

    CHECK(counter.count == 1);
}

TEST_CASE("dispatcher: multiple enqueued events all delivered in single update",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    GoCounter counter;
    dispatcher.sink<GoRightEvent>().connect<&GoCounter::on_go>(counter);

    dispatcher.enqueue(GoRightEvent{});
    dispatcher.enqueue(GoRightEvent{});
    dispatcher.enqueue(GoRightEvent{});
    dispatcher.update();

    CHECK(counter.count == 3);
}

TEST_CASE("dispatcher: update drains queue — second update does not replay events",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    GoCounter counter;
    dispatcher.sink<GoRightEvent>().connect<&GoCounter::on_go>(counter);

    dispatcher.enqueue(GoRightEvent{});

    dispatcher.update();
    CHECK(counter.count == 1);

    dispatcher.update();
    CHECK(counter.count == 1);
}

TEST_CASE("dispatcher: Go*Event listeners fire in sink connection order (last connected first)",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    int order[4] = {-1, -1, -1, -1};
    int cursor = 0;
    OrderedListener first{order, 1, &cursor};
    OrderedListener second{order, 2, &cursor};

    dispatcher.sink<GoRightEvent>().connect<&OrderedListener::on_go>(first);
    dispatcher.sink<GoRightEvent>().connect<&OrderedListener::on_go>(second);

    dispatcher.enqueue(GoRightEvent{});
    dispatcher.update<GoRightEvent>();

    REQUIRE(cursor == 2);
    CHECK(order[0] == 2);
    CHECK(order[1] == 1);
}

TEST_CASE("dispatcher: same-type enqueue inside listener is delivered next update",
          "[entt_dispatcher][latency]") {
    entt::dispatcher dispatcher;
    ReenqueueGoListener listener{&dispatcher};
    GoCounter counter;

    dispatcher.sink<GoRightEvent>().connect<&ReenqueueGoListener::on_go>(listener);
    dispatcher.sink<GoRightEvent>().connect<&GoCounter::on_go>(counter);

    dispatcher.enqueue(GoRightEvent{});
    dispatcher.update<GoRightEvent>();

    CHECK(listener.seen == 1);
    CHECK(counter.count == 1);

    dispatcher.update<GoRightEvent>();
    CHECK(listener.seen == 2);
    CHECK(counter.count == 2);
}

TEST_CASE("dispatcher: trigger ShapePressEvent reaches listener immediately",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    PressCounter counter;
    dispatcher.sink<ShapePressCircleEvent>().connect<&PressCounter::on_press>(counter);

    dispatcher.trigger(ShapePressCircleEvent{});

    CHECK(counter.count == 1);
}

TEST_CASE("dispatcher: enqueue ShapePressEvent without update delivers nothing",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    PressCounter counter;
    dispatcher.sink<ShapePressCircleEvent>().connect<&PressCounter::on_press>(counter);

    dispatcher.enqueue(ShapePressCircleEvent{});

    CHECK(counter.count == 0);
}

TEST_CASE("wire_input_dispatcher prewarms semantic event queues without pending events",
          "[entt_dispatcher][input_pipeline]") {
    auto reg = make_registry();
    auto& dispatcher = reg.ctx().get<entt::dispatcher>();

    CHECK(dispatcher.size<GoUpEvent>()    == 0);
    CHECK(dispatcher.size<GoDownEvent>()  == 0);
    CHECK(dispatcher.size<GoLeftEvent>()  == 0);
    CHECK(dispatcher.size<GoRightEvent>() == 0);
    CHECK(dispatcher.size<ShapePressCircleEvent>()   == 0);
    CHECK(dispatcher.size<ShapePressSquareEvent>()   == 0);
    CHECK(dispatcher.size<ShapePressTriangleEvent>() == 0);
    CHECK(dispatcher.size<MenuConfirmEvent>()        == 0);
    CHECK(dispatcher.size<MenuRestartEvent>()        == 0);
    CHECK(dispatcher.size<MenuGoLevelSelectEvent>()  == 0);
    CHECK(dispatcher.size<MenuGoMainMenuEvent>()     == 0);
    CHECK(dispatcher.size<MenuSelectLevelEvent>()    == 0);
    CHECK(dispatcher.size<MenuSelectDiffEvent>()     == 0);
}

TEST_CASE("runtime scratch queues are explicit registry context state",
          "[ecs][scratch]") {
    auto reg = make_registry();

    CHECK(reg.ctx().contains<ScoringSystemScratch>());
    // PendingEnergyEffects ctx singleton was eradicated by issue #1627 — its
    // contents now live as per-frame row entities tagged
    // `PendingEnergyEffectTag` (no ctx residency to assert).
    CHECK(reg.ctx().contains<ScorePopupRequestQueue>());
    CHECK(reg.ctx().contains<ObstacleDespawnScratch>());
    CHECK(reg.ctx().contains<PopupDisplayScratch>());
    CHECK(reg.ctx().contains<ParticleSystemScratch>());
}

TEST_CASE("R7: Go*Event delivered in GameOver phase — player_input handler no-ops due to phase guard",
          "[dispatcher][R7][regression]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane  = reg.get<Lane>(player);
    auto& disp  = reg.ctx().get<entt::dispatcher>();

    set_test_phase<GamePhaseGameOverTag>(reg);

    disp.enqueue(GoRightEvent{});
    disp.update<GoRightEvent>();

    CHECK_FALSE(reg.all_of<LaneTransition>(player));
    CHECK(lane.current == 1);
}

TEST_CASE("R7: drain-first order — Go*Event processed in pre-transition phase, queue empty post-transition",
          "[dispatcher][R7]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& disp = reg.ctx().get<entt::dispatcher>();

    REQUIRE(reg.ctx().contains<GamePhasePlayingTag>());

    disp.enqueue(GoRightEvent{});

    disp.update<GoRightEvent>();
    REQUIRE(reg.all_of<LaneTransition>(player));
    CHECK(reg.get<LaneTransition>(player).target == 2);

    set_test_phase<GamePhaseGameOverTag>(reg);

    disp.update<GoRightEvent>();
    CHECK(reg.get<LaneTransition>(player).target == 2);
}

TEST_CASE("R7: two-tick stale-event regression — Go*Event from tick N absent in tick N+1",
          "[dispatcher][R7][regression]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    auto& disp = reg.ctx().get<entt::dispatcher>();

    REQUIRE(reg.ctx().contains<GamePhasePlayingTag>());
    disp.enqueue(GoRightEvent{});
    disp.update<GoRightEvent>();
    REQUIRE(reg.all_of<LaneTransition>(player));
    CHECK(reg.get<LaneTransition>(player).target == 2);

    reg.get<LaneTransition>(player).lerp_t = 0.5f;

    set_test_phase<GamePhaseGameOverTag>(reg);

    disp.update<GoRightEvent>();

    CHECK(reg.get<LaneTransition>(player).target  == 2);
    CHECK(reg.get<LaneTransition>(player).lerp_t  == 0.5f);
    CHECK(lane.current == 1);
}
