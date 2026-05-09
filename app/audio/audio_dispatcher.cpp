#include "audio_routing.h"
#include "../components/audio_events.h"
#include <entt/entt.hpp>
#include <stdexcept>

namespace {

struct AudioHapticDispatcherConnections {
    entt::dispatcher* owner = nullptr;
    entt::scoped_connection sfx;
    entt::scoped_connection haptic;
};

void warm_audio_haptic_dispatcher(entt::dispatcher& disp) {
    // Move first vector allocation out of the first gameplay audio frame.
    disp.enqueue<PlaySfxEvent>(PlaySfxEvent{});
    disp.enqueue<PlayHapticEvent>(PlayHapticEvent{});
    disp.clear<PlaySfxEvent>();
    disp.clear<PlayHapticEvent>();
}

}  // namespace

// ── Drain-ownership model ────────────────────────────────────────────────────
// audio_system owns the PlaySfxEvent drain  (disp.update<PlaySfxEvent>())
// haptic_system owns the PlayHapticEvent drain (disp.update<PlayHapticEvent>())
// Both drains are called in game_loop_frame after rendering.
void wire_audio_haptic_dispatcher(entt::registry& reg) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) {
        throw std::logic_error(
            "wire_audio_haptic_dispatcher requires entt::dispatcher in registry context");
    }

    auto* state = reg.ctx().find<AudioHapticDispatcherConnections>();
    if (!state) {
        state = &reg.ctx().emplace<AudioHapticDispatcherConnections>();
    }
    if (state->owner == disp) {
        return;
    }
    *state = AudioHapticDispatcherConnections{};
    state->owner = disp;

    state->sfx    = entt::scoped_connection{
        disp->sink<PlaySfxEvent>().connect<&audio_handle_play_sfx>(reg)};
    state->haptic = entt::scoped_connection{
        disp->sink<PlayHapticEvent>().connect<&haptic_handle_play>(reg)};

    warm_audio_haptic_dispatcher(*disp);
    warm_audio_haptic_dispatcher(*disp);
}

void unwire_audio_haptic_dispatcher(entt::registry& reg) {
    if (auto* state = reg.ctx().find<AudioHapticDispatcherConnections>()) {
        *state = AudioHapticDispatcherConnections{};
    }
}
