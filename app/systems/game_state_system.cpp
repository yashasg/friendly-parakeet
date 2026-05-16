#include "all_systems.h"
#include "game_phase_transition.h"
#include "game_state_system.h"
#include "input_system_private.h"
#include "play_session.h"
#include "../components/game_state.h"
#include "input.h"
#include "../components/obstacle.h"
#include "input_events.h"
#include "../components/player.h"
#include "../components/rhythm.h"
#include "../entities/settings.h"
#include "../constants.h"
#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
#include <emscripten/emscripten.h>
#include <string>
#endif

#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
namespace {

WasmSmokeLaneMarkerState& wasm_smoke_lane_marker_state(entt::registry& reg) {
    if (auto* state = reg.ctx().find<WasmSmokeLaneMarkerState>()) {
        return *state;
    }
    return reg.ctx().emplace<WasmSmokeLaneMarkerState>();
}

void reset_web_playing_lane_marker(entt::registry& reg) {
    wasm_smoke_lane_marker_state(reg).last_lane = -1;
}

void update_web_playing_lane_marker(entt::registry& reg, const GameState& gs) {
    auto& marker = wasm_smoke_lane_marker_state(reg);
    if (gs.phase != GamePhase::Playing) {
        marker.last_lane = -1;
        return;
    }

    auto player_view = reg.view<PlayerTag, Lane>();
    if (player_view.begin() == player_view.end()) {
        marker.last_lane = -1;
        return;
    }

    const auto player_entity = *player_view.begin();
    const int lane = static_cast<int>(player_view.get<Lane>(player_entity).current);
    if (lane == marker.last_lane) {
        return;
    }
    marker.last_lane = lane;

    const std::string title = std::string("SHAPESHIFTER [Playing][Lane:")
        + std::to_string(lane) + "]";
    EM_ASM(
        {
            if (typeof document !== 'undefined') {
                document.title = UTF8ToString($0);
            }
        },
        title.c_str());
}

}  // namespace
#endif

void game_state_system(entt::registry& reg, float dt) {
    auto& gs = reg.ctx().get<GameState>();

    gs.phase_timer += dt;

    // ── Primary event drain ───────────────────────────────────────────────────
    // game_state_system runs first in tick_fixed_systems (game_loop.cpp) and
    // owns the authoritative drain for GoEvent and all press event queues
    // (per-shape ShapePress* + MenuPressEvent — see input_events.h for the
    // per-event-type split, issue #1202/#1204).
    // Calling update<T>() here fires every registered listener in registration
    // order: game_state → level_select → player_input
    // (see wire_input_dispatcher in systems/input_dispatcher.cpp).
    //
    // Events are enqueued earlier in the same frame by input_system and the
    // gameplay HUD input collector before the fixed-step loop. Shape presses
    // drain before movement so same-frame mobile tap+swipe combo obstacles
    // resolve deterministically.
    //
    // ⚠ Do NOT call disp.clear<>() on any of these queues before this point
    //   within a frame.  Pre-tick systems enqueue same-frame events that
    //   must reach player_input listeners through this update() call.
    //   clear() would silently drop them before delivery.
    //
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.update<ShapePressCircleEvent>();
    disp.update<ShapePressSquareEvent>();
    disp.update<ShapePressTriangleEvent>();
    disp.update<MenuPressEvent>();
    disp.update<GoEvent>();

    if (gs.transition_pending) {
        gs.transition_pending = false;

        // Clear any in-flight pointer capture when changing screens so
        // down/up state from the previous phase cannot leak into the next UI.
        // Owned by input_system because suppress_mouse_release lives in
        // InputSystemPrivate (issue #1196).
        input_system_clear_pointer_state(reg);

        // Per Fabian's existential processing (issue #1202/#1204, PR C):
        // each former `case GamePhase::X` is now its own per-tag transform.
        // `sync_next_phase_tags` mirrors `gs.next_phase` into exactly one
        // `NextPhase*Tag` ctx slot; the transforms below dispatch on tag
        // presence; `clear_next_phase_tags` drains the mirror so a stale
        // request cannot fire again next frame. The `gs.next_phase` field
        // is retained during the staged migration; PR G deletes it along
        // with the `GamePhase` enum itself.
        sync_next_phase_tags(reg, gs.next_phase);
        auto& ctx = reg.ctx();

        if (ctx.contains<NextPhasePlayingTag>()) {
            // Resume from Paused must NOT re-init the play session — that
            // would discard score/energy/obstacles. All other paths into
            // Playing (Title/LevelSelect/etc.) bootstrap a fresh session.
            // See #482 for the rationale that converged screen controllers
            // and input routing on the deferred (transition_pending) path.
            //
            // NB: this `gs.phase == GamePhase::Paused` check is on the
            // current phase (not the dispatch target) and is migrated as
            // part of PR F (`if (gs.phase == X)` sweep), not PR C.
            if (gs.phase == GamePhase::Paused) {
                enter_phase(reg, gs, GamePhase::Playing);
            } else {
                setup_play_session(reg);
            }
        }
        if (ctx.contains<NextPhaseGameOverTag>()) {
            game_state_enter_terminal_phase(reg, GamePhase::GameOver);
        }
        if (ctx.contains<NextPhaseSongCompleteTag>()) {
            game_state_enter_terminal_phase(reg, GamePhase::SongComplete);
        }
        if (ctx.contains<NextPhasePausedTag>()) {
            enter_phase(reg, gs, GamePhase::Paused);
        }
        if (ctx.contains<NextPhaseTitleTag>()) {
            enter_phase(reg, gs, GamePhase::Title);
        }
        if (ctx.contains<NextPhaseLevelSelectTag>()) {
            enter_phase(reg, gs, GamePhase::LevelSelect);
            auto& lss = reg.ctx().get<LevelSelectState>();
            lss.confirmed = false;
        }
        if (ctx.contains<NextPhaseSettingsTag>()) {
            enter_phase(reg, gs, GamePhase::Settings);
        }
        if (ctx.contains<NextPhaseTutorialTag>()) {
            enter_phase(reg, gs, GamePhase::Tutorial);
        }

        clear_next_phase_tags(reg);
#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
        if (gs.phase != GamePhase::Playing) {
            reset_web_playing_lane_marker(reg);
        }
#endif
        return;
    }

    // LevelSelect input handling
    if (gs.phase == GamePhase::LevelSelect && gs.phase_timer > constants::UI_ENTRY_DEBOUNCE) {
        auto& lss = reg.ctx().get<LevelSelectState>();
        if (lss.confirmed) {
            lss.confirmed = false;
            const auto* settings_ptr = find_settings_state(reg);
            gs.transition_pending = true;
            gs.next_phase = settings_ptr && !settings::ftue_complete(*settings_ptr)
                ? GamePhase::Tutorial
                : GamePhase::Playing;
        }
    }

#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
    update_web_playing_lane_marker(reg, gs);
#endif
    // Playing → SongComplete when song finishes (all obstacles cleared)
    if (gs.phase == GamePhase::Playing) {
        auto* energy = reg.ctx().find<EnergyState>();
        auto* song = reg.ctx().find<SongState>();
        if (energy && energy->energy <= 0.0f) {
            reg.ctx().insert_or_assign(EnergyDepletedDeath{});
            gs.transition_pending = true;
            gs.next_phase = GamePhase::GameOver;
            return;
        }

        if (song && song->finished) {
            // Wait until all obstacle entities have been destroyed.
            if (reg.view<ObstacleTag>().empty()) {
                gs.transition_pending = true;
                gs.next_phase = GamePhase::SongComplete;
            }
        }
    }

    game_state_end_screen_system(reg, dt);
}
