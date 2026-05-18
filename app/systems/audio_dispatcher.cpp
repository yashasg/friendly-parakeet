#include "audio_routing.h"
#include "audio_events.h"
#include <entt/entt.hpp>
#include <stdexcept>

namespace {

// Raw `entt::connection` (not `scoped_connection`) on purpose: see issue #1268.
// Same rationale as `input_dispatcher.cpp` — `reg.ctx()` is a `dense_map` whose
// packed `std::vector` destruction order is unspecified across standard
// libraries (libc++ reverse, libstdc++ forward). A `scoped_connection` whose
// destructor fires after the dispatcher is freed would dereference a dead
// `sigh` and SIGSEGV in test teardown. Registry teardown is now a no-op for
// these handles; explicit release happens through `release_all()` only when
// the dispatcher is known to still be alive.
struct AudioHapticDispatcherConnections {
    entt::dispatcher* owner = nullptr;
    entt::connection sfx;
    entt::connection haptic;

    void release_all() noexcept {
        haptic.release();
        sfx.release();
        owner = nullptr;
    }
};

}  // namespace

// audio_system and haptic_system own their post-render event drains.
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
    state->release_all();
    state->owner = disp;

    state->sfx =
        disp->sink<PlaySfxEvent>().connect<&audio_handle_play_sfx>(reg);
    state->haptic =
        disp->sink<PlayHapticEvent>().connect<&haptic_handle_play>(reg);

    // Move first vector allocation out of the first gameplay audio frame.
    disp->enqueue<PlaySfxEvent>(PlaySfxEvent{});
    disp->enqueue<PlayHapticEvent>(PlayHapticEvent{});
    disp->clear<PlaySfxEvent>();
    disp->clear<PlayHapticEvent>();
}

void unwire_audio_haptic_dispatcher(entt::registry& reg) {
    if (auto* state = reg.ctx().find<AudioHapticDispatcherConnections>()) {
        state->release_all();
    }
}
