#include "platform_display.h"
#include "game_loop.h"
#include "components/game_state.h"

#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <rlgl.h>

void platform_get_display_size(float& out_w, float& out_h) {
    double css_w = 0.0, css_h = 0.0;
    emscripten_get_element_css_size("#canvas", &css_w, &css_h);
    out_w = static_cast<float>(std::max(1.0, css_w));
    out_h = static_cast<float>(std::max(1.0, css_h));

    // Sync drawing buffer to CSS size so coords align
    int cur_w = 0, cur_h = 0;
    emscripten_get_canvas_element_size("#canvas", &cur_w, &cur_h);
    int target_w = static_cast<int>(out_w);
    int target_h = static_cast<int>(out_h);
    if (cur_w != target_w || cur_h != target_h)
        emscripten_set_canvas_element_size("#canvas", target_w, target_h);
}

void platform_pre_blit() {
    // After canvas buffer resize, raylib's viewport may not reflect the new
    // dimensions.  Explicitly set viewport + ortho to match the buffer.
    int buf_w = 0, buf_h = 0;
    emscripten_get_canvas_element_size("#canvas", &buf_w, &buf_h);
    rlViewport(0, 0, buf_w, buf_h);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0.0, static_cast<double>(buf_w),
            static_cast<double>(buf_h), 0.0, 0.0, 1.0);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}

struct WasmLoopState {
    entt::registry* reg = nullptr;
    float accumulator = 0.0f;
    bool shutting_down = false;
    bool beforeunload_armed = false;
    bool visibilitychange_armed = false;
};

static void wasm_unset_beforeunload(WasmLoopState& state) {
    if (!state.beforeunload_armed) return;
    emscripten_set_beforeunload_callback(nullptr, nullptr);
    state.beforeunload_armed = false;
}

static void wasm_unset_visibilitychange(WasmLoopState& state) {
    if (!state.visibilitychange_armed) return;
    emscripten_set_visibilitychange_callback(nullptr, false, nullptr);
    state.visibilitychange_armed = false;
}

void platform_disarm_wasm_beforeunload(entt::registry& reg) {
    if (auto* state = reg.ctx().find<WasmLoopState>()) {
        wasm_unset_beforeunload(*state);
        wasm_unset_visibilitychange(*state);
        state->reg = nullptr;
        state->accumulator = 0.0f;
    } else {
        emscripten_set_beforeunload_callback(nullptr, nullptr);
        emscripten_set_visibilitychange_callback(nullptr, false, nullptr);
    }
}

static void wasm_shutdown_once(WasmLoopState& state) {
    if (!state.reg || state.shutting_down) return;
    state.shutting_down = true;
    entt::registry* reg = state.reg;
    state.reg = nullptr;
    state.accumulator = 0.0f;
    game_loop_shutdown(*reg);
}

// Idempotent WASM shutdown — guarded by WasmLoopState::reg sentinel.
// Called either from frame_callback (graceful quit) or from the browser
// beforeunload event (tab close / page navigate).
static void frame_callback(void* user_data) {
    auto& state = *static_cast<WasmLoopState*>(user_data);
    if (!state.reg) return;
    game_loop_frame(*state.reg, state.accumulator);
    // Mirror the native quit conditions: window-close OR explicit quit request.
    if (game_loop_should_quit(*state.reg)) {
        emscripten_cancel_main_loop();
        wasm_shutdown_once(state);
    }
}

// Belt-and-suspenders: also shut down when the browser unloads the page
// (tab close, navigation, refresh) while the main loop is still running.
static const char* on_web_unload(int /*event_type*/,
                                  const void* /*reserved*/,
                                  void* user_data) {
    auto* state = static_cast<WasmLoopState*>(user_data);
    if (state) wasm_shutdown_once(*state);
    return nullptr; // empty string suppresses the browser confirmation dialog
}

static EM_BOOL on_visibility_change(int /*event_type*/,
                                    const EmscriptenVisibilityChangeEvent* event,
                                    void* user_data) {
    auto* state = static_cast<WasmLoopState*>(user_data);
    if (!state || !state->reg || !event || !event->hidden) {
        return EM_FALSE;
    }

    if (auto* gs = state->reg->ctx().find<GameState>()) {
        // Per Fabian's existential processing (issue #1202/#1204, PR F): the
        // suspend-pause predicate dispatches on the `GamePhasePlayingTag` ctx
        // mirror rather than reading `gs->phase`. The `GameState::find`
        // probe is retained because the registry may not yet hold the
        // singleton during early WASM bootstrap.
        if (state->reg->ctx().contains<GamePhasePlayingTag>()) {
            gs->transition_pending = true;
            gs->next_phase = GamePhase::Paused;
        }
    }
    return EM_FALSE;
}

void platform_run_loop(entt::registry& reg) {
    auto* state = reg.ctx().find<WasmLoopState>();
    if (!state) state = &reg.ctx().emplace<WasmLoopState>();

    // Callback ownership contract:
    // - beforeunload is browser/runtime-owned after registration.
    // - WasmLoopState is registry-owned and must remain valid until callback
    //   unregistration happens during shutdown.
    // - game_loop_shutdown() calls platform_disarm_wasm_beforeunload() before
    //   resetting the registry context.
    state->reg = &reg;
    state->accumulator = 0.0f;
    state->shutting_down = false;
    state->beforeunload_armed = true;
    emscripten_set_beforeunload_callback(state, on_web_unload);
    state->visibilitychange_armed = true;
    emscripten_set_visibilitychange_callback(state, false, on_visibility_change);
    emscripten_set_main_loop_arg(frame_callback, state, 0, 1);
}

#else // Native

void platform_get_display_size(float& out_w, float& out_h) {
    out_w = std::max(1.0f, static_cast<float>(GetScreenWidth()));
    out_h = std::max(1.0f, static_cast<float>(GetScreenHeight()));
}

void platform_pre_blit() {
    // No-op on native — raylib manages the viewport.
}

#endif
