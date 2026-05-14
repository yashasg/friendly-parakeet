// tests/test_entt_dispatcher_contract.cpp
//
// Verifies entt::dispatcher semantics relied on by the semantic input pipeline
// (GoEvent/ButtonPressEvent only).

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include "components/game_state.h"
#include "components/input_events.h"
#include "components/system_scratch.h"
#include "test_helpers.h"

struct GoCounter {
    int count{0};
    Direction last{Direction::Up};
    void on_go(const GoEvent& e) { ++count; last = e.dir; }
};

struct PressCounter {
    int count{0};
    void on_press(const ButtonPressEvent&) { ++count; }
};

struct OrderedListener {
    int* order;
    int marker;
    int* cursor;
    void on_go(const GoEvent&) { order[(*cursor)++] = marker; }
};

struct ReenqueueGoListener {
    entt::dispatcher* disp{nullptr};
    int seen{0};
    void on_go(const GoEvent&) {
        ++seen;
        if (seen == 1) {
            disp->enqueue(GoEvent{Direction::Left});
        }
    }
};

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

TEST_CASE("dispatcher: update drains queue — second update does not replay events",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    GoCounter counter;
    dispatcher.sink<GoEvent>().connect<&GoCounter::on_go>(counter);

    dispatcher.enqueue(GoEvent{Direction::Left});

    dispatcher.update();
    CHECK(counter.count == 1);

    dispatcher.update();
    CHECK(counter.count == 1);
}

TEST_CASE("dispatcher: GoEvent listeners fire in sink connection order (last connected first)",
          "[entt_dispatcher]") {
    entt::dispatcher dispatcher;
    int order[4] = {-1, -1, -1, -1};
    int cursor = 0;
    OrderedListener first{order, 1, &cursor};
    OrderedListener second{order, 2, &cursor};

    dispatcher.sink<GoEvent>().connect<&OrderedListener::on_go>(first);
    dispatcher.sink<GoEvent>().connect<&OrderedListener::on_go>(second);

    dispatcher.enqueue(GoEvent{Direction::Right});
    dispatcher.update<GoEvent>();

    REQUIRE(cursor == 2);
    CHECK(order[0] == 2);
    CHECK(order[1] == 1);
}

TEST_CASE("dispatcher: same-type enqueue inside listener is delivered next update",
          "[entt_dispatcher][latency]") {
    entt::dispatcher dispatcher;
    ReenqueueGoListener listener{&dispatcher};
    GoCounter counter;

    dispatcher.sink<GoEvent>().connect<&ReenqueueGoListener::on_go>(listener);
    dispatcher.sink<GoEvent>().connect<&GoCounter::on_go>(counter);

    dispatcher.enqueue(GoEvent{Direction::Right});
    dispatcher.update<GoEvent>();

    CHECK(listener.seen == 1);
    CHECK(counter.count == 1);
    CHECK(counter.last == Direction::Right);

    dispatcher.update<GoEvent>();
    CHECK(listener.seen == 2);
    CHECK(counter.count == 2);
    CHECK(counter.last == Direction::Left);
}

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

TEST_CASE("wire_input_dispatcher prewarms semantic event queues without pending events",
          "[entt_dispatcher][input_pipeline]") {
    auto reg = make_registry();
    auto& dispatcher = reg.ctx().get<entt::dispatcher>();

    CHECK(dispatcher.size<GoEvent>() == 0);
    CHECK(dispatcher.size<ButtonPressEvent>() == 0);
}

TEST_CASE("runtime scratch queues are explicit registry context state",
          "[ecs][scratch]") {
    auto reg = make_registry();

    CHECK(reg.ctx().contains<ScoringSystemScratch>());
    CHECK(reg.ctx().contains<PendingEnergyEffects>());
    CHECK(reg.ctx().contains<ScorePopupRequestQueue>());
    CHECK(reg.ctx().contains<ObstacleDespawnScratch>());
    CHECK(reg.ctx().contains<PopupDisplayScratch>());
    CHECK(reg.ctx().contains<ParticleSystemScratch>());
}

TEST_CASE("R7: GoEvent delivered in GameOver phase — player_input_handle_go no-ops due to phase guard",
          "[dispatcher][R7][regression]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane  = reg.get<Lane>(player);
    auto& gs    = reg.ctx().get<GameState>();
    auto& disp  = reg.ctx().get<entt::dispatcher>();

    gs.phase = GamePhase::GameOver;

    disp.enqueue(GoEvent{Direction::Right});
    disp.update<GoEvent>();

    CHECK(lane.target  == -1);
    CHECK(lane.current == 1);
}

TEST_CASE("R7: drain-first order — GoEvent processed in pre-transition phase, queue empty post-transition",
          "[dispatcher][R7]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    auto& gs   = reg.ctx().get<GameState>();
    auto& disp = reg.ctx().get<entt::dispatcher>();

    REQUIRE(gs.phase == GamePhase::Playing);

    disp.enqueue(GoEvent{Direction::Right});

    disp.update<GoEvent>();
    CHECK(lane.target == 2);

    gs.phase = GamePhase::GameOver;

    disp.update<GoEvent>();
    CHECK(lane.target == 2);
}

TEST_CASE("R7: two-tick stale-event regression — GoEvent from tick N absent in tick N+1",
          "[dispatcher][R7][regression]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane = reg.get<Lane>(player);
    auto& gs   = reg.ctx().get<GameState>();
    auto& disp = reg.ctx().get<entt::dispatcher>();

    REQUIRE(gs.phase == GamePhase::Playing);
    disp.enqueue(GoEvent{Direction::Right});
    disp.update<GoEvent>();
    CHECK(lane.target == 2);

    lane.lerp_t = 0.5f;

    gs.phase = GamePhase::GameOver;

    disp.update<GoEvent>();

    CHECK(lane.target  == 2);
    CHECK(lane.lerp_t  == 0.5f);
    CHECK(lane.current == 1);
}
