#pragma once

#include "../components/input_events.h"

#include <cstdint>
#include <entt/entt.hpp>

enum class InputLatencyStage : uint8_t {
    InputEventEnqueued = 0,
    GoEventEnqueued = 1,
    GoEventHandled = 2,
};

struct InputLatencyProbe;
using InputLatencyProbeObserver = void(*)(const InputLatencyProbe&, InputLatencyStage, void*);

struct InputLatencyProbe {
    bool enabled = false;
    uint64_t frame_index = 0;

    uint64_t input_events_enqueued = 0;
    uint64_t go_events_enqueued = 0;
    uint64_t go_events_handled = 0;

    uint64_t last_input_frame = 0;
    uint64_t last_go_enqueue_frame = 0;
    uint64_t last_go_handle_frame = 0;

    uint32_t last_source_timestamp_ms = 0;
    uint32_t last_input_to_go_enqueue_frames = 0;
    uint32_t last_input_to_go_handle_frames = 0;

    InputType last_input_type = InputType::Tap;
    Direction last_direction = Direction::Up;
    InputLatencyStage last_stage = InputLatencyStage::InputEventEnqueued;

    InputLatencyProbeObserver observer = nullptr;
    void* observer_context = nullptr;
};

inline InputLatencyProbe* find_input_latency_probe(entt::registry& reg) {
    return reg.ctx().find<InputLatencyProbe>();
}

inline void input_latency_begin_frame(entt::registry& reg) {
    auto* probe = find_input_latency_probe(reg);
    if (!probe || !probe->enabled) return;
    ++probe->frame_index;
}

inline void input_latency_notify(const InputLatencyProbe& probe, InputLatencyStage stage) {
    if (probe.observer) {
        probe.observer(probe, stage, probe.observer_context);
    }
}

inline void input_latency_note_input_event_enqueued(
    entt::registry& reg,
    InputType type,
    Direction dir,
    uint32_t source_timestamp_ms
) {
    auto* probe = find_input_latency_probe(reg);
    if (!probe || !probe->enabled) return;
    ++probe->input_events_enqueued;
    probe->last_input_frame = probe->frame_index;
    probe->last_input_type = type;
    probe->last_direction = dir;
    probe->last_source_timestamp_ms = source_timestamp_ms;
    probe->last_stage = InputLatencyStage::InputEventEnqueued;
    input_latency_notify(*probe, InputLatencyStage::InputEventEnqueued);
}

inline void input_latency_note_go_event_enqueued(entt::registry& reg, Direction dir) {
    auto* probe = find_input_latency_probe(reg);
    if (!probe || !probe->enabled) return;
    ++probe->go_events_enqueued;
    probe->last_go_enqueue_frame = probe->frame_index;
    probe->last_direction = dir;
    if (probe->last_input_frame <= probe->frame_index) {
        probe->last_input_to_go_enqueue_frames = static_cast<uint32_t>(
            probe->frame_index - probe->last_input_frame
        );
    } else {
        probe->last_input_to_go_enqueue_frames = 0;
    }
    probe->last_stage = InputLatencyStage::GoEventEnqueued;
    input_latency_notify(*probe, InputLatencyStage::GoEventEnqueued);
}

inline void input_latency_note_go_event_handled(entt::registry& reg, Direction dir) {
    auto* probe = find_input_latency_probe(reg);
    if (!probe || !probe->enabled) return;
    ++probe->go_events_handled;
    probe->last_go_handle_frame = probe->frame_index;
    probe->last_direction = dir;
    if (probe->last_input_frame <= probe->frame_index) {
        probe->last_input_to_go_handle_frames = static_cast<uint32_t>(
            probe->frame_index - probe->last_input_frame
        );
    } else {
        probe->last_input_to_go_handle_frames = 0;
    }
    probe->last_stage = InputLatencyStage::GoEventHandled;
    input_latency_notify(*probe, InputLatencyStage::GoEventHandled);
}
