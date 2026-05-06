#include <catch2/catch_test_macros.hpp>

#include "systems/input_latency_probe.h"
#include "test_helpers.h"

namespace {

void run_latency_probe_pipeline(entt::registry& reg, float dt = 0.016f) {
    run_input_tier1(reg);
    player_input_system(reg, dt);
}

}  // namespace

TEST_CASE("input latency probe: swipe path records same-frame pipeline deltas", "[input][latency]") {
    auto reg = make_rhythm_registry();
    auto& probe = reg.ctx().emplace<InputLatencyProbe>();
    probe.enabled = true;

    input_latency_begin_frame(reg);
    push_input(reg, InputType::Swipe, 0.0f, 0.0f, Direction::Right);
    input_latency_note_input_event_enqueued(reg, InputType::Swipe, Direction::Right, 123u);
    run_latency_probe_pipeline(reg);

    CHECK(probe.input_events_enqueued == 1);
    CHECK(probe.go_events_enqueued == 1);
    CHECK(probe.go_events_handled == 1);
    CHECK(probe.last_source_timestamp_ms == 123u);
    CHECK(probe.last_input_to_go_enqueue_frames == 0u);
    CHECK(probe.last_input_to_go_handle_frames == 0u);
}

TEST_CASE("input latency probe: disabled probe is non-invasive", "[input][latency]") {
    auto reg = make_rhythm_registry();
    auto& probe = reg.ctx().emplace<InputLatencyProbe>();
    probe.enabled = false;

    input_latency_begin_frame(reg);
    input_latency_note_input_event_enqueued(reg, InputType::Tap, Direction::Up, 10u);
    input_latency_note_go_event_enqueued(reg, Direction::Up);
    input_latency_note_go_event_handled(reg, Direction::Up);

    CHECK(probe.frame_index == 0u);
    CHECK(probe.input_events_enqueued == 0u);
    CHECK(probe.go_events_enqueued == 0u);
    CHECK(probe.go_events_handled == 0u);
}
