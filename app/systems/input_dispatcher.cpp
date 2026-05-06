#include "input_routing.h"
#include "../components/input_events.h"
#include "../components/registry_context.h"
#include "input_latency_probe.h"
#include <entt/entt.hpp>

namespace {

struct InputDispatcherWiringState {
    bool wired = false;
};

void route_swipe_input_to_go(entt::registry& reg, const InputEvent& evt);

void clear_input_queues(entt::dispatcher& disp) {
    disp.clear<InputEvent>();
    disp.clear<GoEvent>();
    disp.clear<ButtonPressEvent>();
}

void connect_semantic_input_sinks(entt::dispatcher& disp, entt::registry& reg) {
    disp.sink<GoEvent>().connect<&game_state_handle_go>(reg);
    disp.sink<GoEvent>().connect<&level_select_handle_go>(reg);
    disp.sink<GoEvent>().connect<&player_input_handle_go>(reg);

    disp.sink<ButtonPressEvent>().connect<&game_state_handle_press>(reg);
    disp.sink<ButtonPressEvent>().connect<&level_select_handle_press>(reg);
    disp.sink<ButtonPressEvent>().connect<&player_input_handle_press>(reg);
}

void disconnect_semantic_input_sinks(entt::dispatcher& disp, entt::registry& reg) {
    disp.sink<GoEvent>().disconnect<&game_state_handle_go>(reg);
    disp.sink<GoEvent>().disconnect<&level_select_handle_go>(reg);
    disp.sink<GoEvent>().disconnect<&player_input_handle_go>(reg);

    disp.sink<ButtonPressEvent>().disconnect<&game_state_handle_press>(reg);
    disp.sink<ButtonPressEvent>().disconnect<&level_select_handle_press>(reg);
    disp.sink<ButtonPressEvent>().disconnect<&player_input_handle_press>(reg);
}

void route_swipe_input_to_go(entt::registry& reg, const InputEvent& evt) {
    if (evt.type != InputType::Swipe) return;
    registry_ctx_get<entt::dispatcher>(reg).enqueue<GoEvent>({evt.dir});
    input_latency_note_go_event_enqueued(reg, evt.dir);
}
}

// Routes Swipe InputEvents into GoEvents. Tap and other input types are
// intentionally ignored here.
// ── Drain-ownership model ────────────────────────────────────────────────────
// Two-tier delivery (#333):
//
// Tier 1 — Raw InputEvent (before fixed-step):
//   input_system enqueues InputEvent objects; game_loop_frame then calls
//   disp.update<InputEvent>(), which fires route_swipe_input_to_go and
//   enqueues GoEvent for swipe input.
//
// Tier 2 — Semantic GoEvent / ButtonPressEvent (fixed-step delivery):
//   game_state_system (first in tick_fixed_systems) calls
//   drain_semantic_input_events(), draining the semantic queues and delivering
//   events to all registered listeners.
//   A second update<T>() later in the same tick (e.g., player_input_system)
//   finds an empty queue and is a guaranteed no-op.
//
// Listener registration order IS the canonical processing order (EnTT R1).
// Changing this order changes observable game behaviour.
void wire_input_dispatcher(entt::registry& reg) {
    auto& state = registry_ctx_get_or_emplace<InputDispatcherWiringState>(reg);
    if (state.wired) return;

    auto& disp = registry_ctx_get_or_emplace<entt::dispatcher>(reg);
    disp.sink<InputEvent>().connect<&route_swipe_input_to_go>(reg);
    connect_semantic_input_sinks(disp, reg);
    clear_input_queues(disp);
    state.wired = true;
}

void unwire_input_dispatcher(entt::registry& reg) {
    auto* state = registry_ctx_find<InputDispatcherWiringState>(reg);
    if (!state) return;

    auto* disp = registry_ctx_find<entt::dispatcher>(reg);
    if (!state->wired || !disp) {
        state->wired = false;
        return;
    }

    disp->sink<InputEvent>().disconnect<&route_swipe_input_to_go>(reg);
    disconnect_semantic_input_sinks(*disp, reg);
    clear_input_queues(*disp);
    state->wired = false;
}

void drain_semantic_input_events(entt::registry& reg) {
    auto* disp = registry_ctx_find<entt::dispatcher>(reg);
    if (!disp) return;
    disp->update<GoEvent>();
    disp->update<ButtonPressEvent>();
}
