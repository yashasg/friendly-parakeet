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

void game_state_system(entt::registry& reg, float dt) {
    auto& gs = reg.ctx().get<GameState>();

    gs.phase_timer += dt;

    // ── Primary event drain ───────────────────────────────────────────────────
    // game_state_system runs first in tick_fixed_systems (game_loop.cpp) and
    // owns the authoritative drain for the per-direction Go*Event queues and
    // all press event queues (per-shape ShapePress* + per-action MenuConfirm/
    // MenuRestart/… — see input_events.h for the per-event-type split,
    // issue #1202/#1204/#1277/#1279).
    // Calling update<T>() here fires every registered listener in registration
    // order: game_state → level_select → player_input
    // (see wire_input_dispatcher in systems/input_dispatcher.cpp).
    //
    // Events are enqueued earlier in the same frame by input_system and the
    // gameplay HUD input collector before the fixed-step loop. Shape presses
    // drain before movement so a same-frame mobile tap + swipe both resolve
    // deterministically.
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
    disp.update<MenuConfirmEvent>();
    disp.update<MenuRestartEvent>();
    disp.update<MenuGoLevelSelectEvent>();
    disp.update<MenuGoMainMenuEvent>();
    disp.update<MenuSelectLevelEvent>();
    disp.update<MenuSelectDiffEvent>();
    disp.update<GoUpEvent>();
    disp.update<GoDownEvent>();
    disp.update<GoLeftEvent>();
    disp.update<GoRightEvent>();

    if (is_phase_transition_pending(reg)) {
        // Clear any in-flight pointer capture when changing screens so
        // down/up state from the previous phase cannot leak into the next UI.
        // Owned by input_system because suppress_mouse_release lives in
        // InputSystemPrivate (issue #1196).
        input_system_clear_pointer_state(reg);

        // Per Fabian's existential processing (issue #1202/#1204, PR G):
        // each former `case GamePhase::X` is its own per-tag transform.
        // `request_phase_transition<NextPhase*Tag>()` (called by UI / input /
        // upstream systems) is the sole writer of the `NextPhase*Tag` mirror;
        // the transforms below dispatch on tag presence; `clear_next_phase_tags`
        // drains the mirror so a stale request cannot fire again next frame.
        auto& ctx = reg.ctx();

        if (ctx.contains<NextPhasePlayingTag>()) {
            // Resume from Paused must NOT re-init the play session — that
            // would discard score/energy/obstacles. All other paths into
            // Playing (Title/LevelSelect/etc.) bootstrap a fresh session.
            // See #482 for the rationale that converged screen controllers
            // and input routing on the deferred (transition_pending) path.
            //
            // Resume-from-pause check reads the current-phase tag mirror
            // (`GamePhasePausedTag`), per Fabian's existential processing
            // (issue #1202/#1204): consumers dispatch on tag presence,
            // never on an enum value. `request_phase_transition` mutates
            // only the `NextPhase*Tag` mirror, so the current phase tag
            // still reflects the pre-transition phase here.
            if (ctx.contains<GamePhasePausedTag>()) {
                enter_phase<GamePhasePlayingTag>(reg);
            } else {
                setup_play_session(reg);
            }
        }
        if (ctx.contains<NextPhaseGameOverTag>()) {
            game_state_enter_terminal_phase_game_over(reg);
        }
        if (ctx.contains<NextPhaseSongCompleteTag>()) {
            game_state_enter_terminal_phase_song_complete(reg);
        }
        if (ctx.contains<NextPhasePausedTag>()) {
            enter_phase<GamePhasePausedTag>(reg);
        }
        if (ctx.contains<NextPhaseTitleTag>()) {
            enter_phase<GamePhaseTitleTag>(reg);
        }
        if (ctx.contains<NextPhaseLevelSelectTag>()) {
            enter_phase<GamePhaseLevelSelectTag>(reg);
            if (reg.ctx().find<LevelSelectConfirmedTag>()) {
                reg.ctx().erase<LevelSelectConfirmedTag>();
            }
        }
        if (ctx.contains<NextPhaseSettingsTag>()) {
            enter_phase<GamePhaseSettingsTag>(reg);
        }
        if (ctx.contains<NextPhaseTutorialTag>()) {
            enter_phase<GamePhaseTutorialTag>(reg);
        }

        clear_next_phase_tags(reg);
#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
        if (!reg.ctx().contains<GamePhasePlayingTag>()) {
            reg.ctx().erase<WasmSmokeLastLane>();
        }
#endif
        return;
    }

    // LevelSelect input handling
    if (reg.ctx().contains<GamePhaseLevelSelectTag>() &&
        gs.phase_timer > constants::UI_ENTRY_DEBOUNCE) {
        if (reg.ctx().find<LevelSelectConfirmedTag>()) {
            reg.ctx().erase<LevelSelectConfirmedTag>();
            const auto* settings_ptr = find_settings_state(reg);
            if (settings_ptr && !settings::ftue_complete(*settings_ptr)) {
                request_phase_transition<NextPhaseTutorialTag>(reg);
            } else {
                request_phase_transition<NextPhasePlayingTag>(reg);
            }
        }
    }

#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
    // Update browser tab title with current lane when in Playing phase, so
    // WASM smoke tests can observe player state via document.title polling.
    // Title is rewritten only on lane change; absence of player or non-Playing
    // phase erases the `WasmSmokeLastLane` row so the next entry rewrites the
    // title (row absence == "no lane reported yet", per Fabian Principle 3).
    {
        auto player_view = reg.view<PlayerTag, Lane>();
        const bool playing = reg.ctx().contains<GamePhasePlayingTag>();
        const bool has_player = player_view.begin() != player_view.end();
        if (!playing || !has_player) {
            reg.ctx().erase<WasmSmokeLastLane>();
        } else {
            const auto player_entity = *player_view.begin();
            const int lane = static_cast<int>(player_view.get<Lane>(player_entity).current);
            const auto* prev = reg.ctx().find<WasmSmokeLastLane>();
            if (prev == nullptr || prev->lane != lane) {
                reg.ctx().insert_or_assign(WasmSmokeLastLane{lane});
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
        }
    }
#endif
    // Playing → SongComplete when song finishes (all obstacles cleared)
    if (reg.ctx().contains<GamePhasePlayingTag>()) {
        auto* energy = reg.ctx().find<EnergyState>();
        auto* song = reg.ctx().find<SongState>();
        if (energy && energy->energy <= 0.0f) {
            reg.ctx().insert_or_assign(EnergyDepletedDeath{});
            request_phase_transition<NextPhaseGameOverTag>(reg);
            return;
        }

        if (song && song->finished) {
            // Wait until all obstacle entities have been destroyed.
            if (reg.view<ObstacleTag>().empty()) {
                request_phase_transition<NextPhaseSongCompleteTag>(reg);
            }
        }
    }

    game_state_end_screen_system(reg, dt);
}
