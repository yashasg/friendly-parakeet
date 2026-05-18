#pragma once

#include <entt/entt.hpp>
#include "components/transform.h"
#include "components/player.h"
#include "components/obstacle.h"
#include "tags/tags.h"
#include "systems/input.h"
#include "systems/input_events.h"
#include "components/game_state.h"
#include "components/scoring.h"
#include "components/rendering.h"
#include "components/particle.h"
#include "components/audio.h"
#include "systems/haptics.h"
#include "systems/audio_events.h"
#include "systems/audio_routing.h"
#include "entities/settings.h"
#include "components/rhythm.h"
#include "util/rhythm_math.h"
#include "util/shape_tag.h"
#include "components/high_score.h"
#include "systems/gameplay_intents.h"
#include "constants.h"
#include "entities/beat_map.h"
#include "entities/obstacle_render_entity.h"
#include "systems/all_systems.h"
#include "systems/game_phase_transition.h"
#include "systems/runtime_systems.h"
#include "systems/input_routing.h"
#include "systems/input_system_private.h"
#include "systems/session_logger_system.h"
#include "components/song_state.h"

// Sets up a registry with all singletons in their default state.
//
// Per Fabian's existential processing (#1202/#1204, PR G): there is no enum
// phase field anymore — the per-phase ctx-tag is the active phase, and
// tests prime it directly via `enter_phase<...>()` (or `set_test_phase<...>`
// below when they need to pin a specific `phase_timer` value).
inline entt::registry make_registry() {
    entt::registry reg;
    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<InputSystemPrivate>();
    reg.ctx().emplace<ScreenTransform>();
    reg.ctx().emplace<entt::dispatcher>();
    wire_input_dispatcher(reg);
    wire_audio_haptic_dispatcher(reg);
    reg.ctx().emplace<GameState>();
    sync_game_phase_tags<GamePhasePlayingTag>(reg);
    reg.ctx().emplace<ScoreState>();
    reg.ctx().emplace<ScoreDisplay>();
    reg.ctx().emplace<CurrentSongHighScore>();
    create_settings_entity(reg);  // defaults: haptics_enabled=true
    reg.ctx().emplace<LevelSelectState>();
    create_beat_map_entity(reg);
    reg.ctx().emplace<SongState>();
    reg.ctx().emplace<EnergyState>();
    reg.ctx().emplace<SongResults>();
    reg.ctx().emplace<HighScorePersistence>();
    reg.ctx().emplace<HighScoreSession>();
    runtime_system_scratch_init(reg);
    return reg;
}

// Mutate the phase in tests without going through `enter_phase<...>()` (which
// also resets `phase_timer` and would defeat tests that pin a specific
// timer value). Keeps the per-phase ctx-tag mirror invariant intact so the
// migrated consumers in `app/` dispatch on the new phase immediately.
template <typename PhaseTag>
inline void set_test_phase(entt::registry& reg) {
    sync_game_phase_tags<PhaseTag>(reg);
}

struct GoCapture {
    int up    = 0;
    int down  = 0;
    int left  = 0;
    int right = 0;

    int count() const { return up + down + left + right; }

    void on_up   (const GoUpEvent&)    { ++up;    }
    void on_down (const GoDownEvent&)  { ++down;  }
    void on_left (const GoLeftEvent&)  { ++left;  }
    void on_right(const GoRightEvent&) { ++right; }
};

struct PressCapture {
    int circle = 0;
    int square = 0;
    int triangle = 0;
    int confirm = 0;
    int restart = 0;
    int go_level_select = 0;
    int go_main_menu = 0;
    MenuSelectLevelEvent select_level_buf[4] = {};
    int select_level_count = 0;
    MenuSelectDiffEvent select_diff_buf[4] = {};
    int select_diff_count = 0;

    int shape_count() const { return circle + square + triangle; }
    int menu_count() const {
        return confirm + restart + go_level_select + go_main_menu +
               select_level_count + select_diff_count;
    }
    int count() const { return shape_count() + menu_count(); }

    void on_circle  (const ShapePressCircleEvent&)   { ++circle;   }
    void on_square  (const ShapePressSquareEvent&)   { ++square;   }
    void on_triangle(const ShapePressTriangleEvent&) { ++triangle; }
    void on_confirm        (const MenuConfirmEvent&)        { ++confirm; }
    void on_restart        (const MenuRestartEvent&)        { ++restart; }
    void on_go_level_select(const MenuGoLevelSelectEvent&)  { ++go_level_select; }
    void on_go_main_menu   (const MenuGoMainMenuEvent&)     { ++go_main_menu; }
    void on_select_level   (const MenuSelectLevelEvent& e)  {
        if (select_level_count < 4) select_level_buf[select_level_count++] = e;
    }
    void on_select_diff    (const MenuSelectDiffEvent& e)   {
        if (select_diff_count < 4) select_diff_buf[select_diff_count++] = e;
    }
};

struct SfxCapture {
    SFX buf[16] = {};
    int count   = 0;
    void capture(const PlaySfxEvent& e) { if (count < 16) buf[count++] = e.clip; }
};

struct HapticCapture {
    HapticEvent buf[8] = {};
    int         count  = 0;
    void capture(const PlayHapticEvent& e) { if (count < 8) buf[count++] = e.evt; }
};

inline GoCapture drain_go_events(entt::registry& reg) {
    GoCapture cap;
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.sink<GoUpEvent>()   .connect<&GoCapture::on_up>   (cap);
    disp.sink<GoDownEvent>() .connect<&GoCapture::on_down> (cap);
    disp.sink<GoLeftEvent>() .connect<&GoCapture::on_left> (cap);
    disp.sink<GoRightEvent>().connect<&GoCapture::on_right>(cap);
    disp.update<GoUpEvent>();
    disp.update<GoDownEvent>();
    disp.update<GoLeftEvent>();
    disp.update<GoRightEvent>();
    disp.sink<GoUpEvent>()   .disconnect<&GoCapture::on_up>   (cap);
    disp.sink<GoDownEvent>() .disconnect<&GoCapture::on_down> (cap);
    disp.sink<GoLeftEvent>() .disconnect<&GoCapture::on_left> (cap);
    disp.sink<GoRightEvent>().disconnect<&GoCapture::on_right>(cap);
    return cap;
}

inline PressCapture drain_press_events(entt::registry& reg) {
    PressCapture cap;
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.sink<ShapePressCircleEvent>()  .connect<&PressCapture::on_circle>(cap);
    disp.sink<ShapePressSquareEvent>()  .connect<&PressCapture::on_square>(cap);
    disp.sink<ShapePressTriangleEvent>().connect<&PressCapture::on_triangle>(cap);
    disp.sink<MenuConfirmEvent>()       .connect<&PressCapture::on_confirm>(cap);
    disp.sink<MenuRestartEvent>()       .connect<&PressCapture::on_restart>(cap);
    disp.sink<MenuGoLevelSelectEvent>() .connect<&PressCapture::on_go_level_select>(cap);
    disp.sink<MenuGoMainMenuEvent>()    .connect<&PressCapture::on_go_main_menu>(cap);
    disp.sink<MenuSelectLevelEvent>()   .connect<&PressCapture::on_select_level>(cap);
    disp.sink<MenuSelectDiffEvent>()    .connect<&PressCapture::on_select_diff>(cap);
    disp.update<ShapePressCircleEvent>();
    disp.update<ShapePressSquareEvent>();
    disp.update<ShapePressTriangleEvent>();
    disp.update<MenuConfirmEvent>();
    disp.update<MenuRestartEvent>();
    disp.update<MenuGoLevelSelectEvent>();
    disp.update<MenuGoMainMenuEvent>();
    disp.update<MenuSelectLevelEvent>();
    disp.update<MenuSelectDiffEvent>();
    disp.sink<ShapePressCircleEvent>()  .disconnect<&PressCapture::on_circle>(cap);
    disp.sink<ShapePressSquareEvent>()  .disconnect<&PressCapture::on_square>(cap);
    disp.sink<ShapePressTriangleEvent>().disconnect<&PressCapture::on_triangle>(cap);
    disp.sink<MenuConfirmEvent>()       .disconnect<&PressCapture::on_confirm>(cap);
    disp.sink<MenuRestartEvent>()       .disconnect<&PressCapture::on_restart>(cap);
    disp.sink<MenuGoLevelSelectEvent>() .disconnect<&PressCapture::on_go_level_select>(cap);
    disp.sink<MenuGoMainMenuEvent>()    .disconnect<&PressCapture::on_go_main_menu>(cap);
    disp.sink<MenuSelectLevelEvent>()   .disconnect<&PressCapture::on_select_level>(cap);
    disp.sink<MenuSelectDiffEvent>()    .disconnect<&PressCapture::on_select_diff>(cap);
    return cap;
}

// Drains all press-event queues (per-shape ShapePress* + per-action menu
// events) in the same order game_state_system uses (issue #1202/#1204/#1277).
// Use in tests that previously called `disp.update<ButtonPressEvent>()` or
// `disp.update<MenuPressEvent>()` directly (both eradicated by the per-event-
// type split).
inline void update_press_events(entt::registry& reg) {
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
}

// Flush the dispatcher PlaySfxEvent queue and return all events that were pending.
// NOTE: This calls disp.update<PlaySfxEvent>() which also invokes audio_handle_play_sfx.
inline SfxCapture drain_sfx_events(entt::registry& reg) {
    SfxCapture cap;
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.sink<PlaySfxEvent>().connect<&SfxCapture::capture>(cap);
    disp.update<PlaySfxEvent>();
    disp.sink<PlaySfxEvent>().disconnect<&SfxCapture::capture>(cap);
    return cap;
}

// RAII guard that silences raylib TraceLog output for the duration of a scope
// and restores the default LOG_INFO threshold on destruction. Use to wrap test
// calls that deliberately exercise production warnings/errors so they don't
// pollute the test-runner's stderr (which would mask real future warnings).
// raylib has no GetTraceLogLevel(); restoring to LOG_INFO matches raylib's
// default and is the level used by the rest of the test binary.
struct ScopedTraceLogSilencer {
    ScopedTraceLogSilencer()  { SetTraceLogLevel(LOG_NONE); }
    ~ScopedTraceLogSilencer() { SetTraceLogLevel(LOG_INFO); }
    ScopedTraceLogSilencer(const ScopedTraceLogSilencer&)            = delete;
    ScopedTraceLogSilencer& operator=(const ScopedTraceLogSilencer&) = delete;
};

// Peek at (and flush) pending PlayHapticEvent events without invoking hardware.
// Useful for assertions about what events were enqueued before delivery.
inline HapticCapture drain_haptic_events(entt::registry& reg) {
    HapticCapture cap;
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.sink<PlayHapticEvent>().connect<&HapticCapture::capture>(cap);
    disp.update<PlayHapticEvent>();
    disp.sink<PlayHapticEvent>().disconnect<&HapticCapture::capture>(cap);
    return cap;
}

// Drains all directional Go*Event queues (per-direction split from #1279).
// Use in tests that previously called `disp.update<GoEvent>()` directly.
inline void update_go_events(entt::registry& reg) {
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.update<GoUpEvent>();
    disp.update<GoDownEvent>();
    disp.update<GoLeftEvent>();
    disp.update<GoRightEvent>();
}

// ── Semantic input injection helpers ─────────────────────────────────────────
inline void push_go_up   (entt::registry& reg) { reg.ctx().get<entt::dispatcher>().enqueue<GoUpEvent>({});    }
inline void push_go_down (entt::registry& reg) { reg.ctx().get<entt::dispatcher>().enqueue<GoDownEvent>({});  }
inline void push_go_left (entt::registry& reg) { reg.ctx().get<entt::dispatcher>().enqueue<GoLeftEvent>({});  }
inline void push_go_right(entt::registry& reg) { reg.ctx().get<entt::dispatcher>().enqueue<GoRightEvent>({}); }

struct TestShapeButtonData {
    Shape shape = Shape::Circle;
};

// Per-action menu-button tag for tests. Mirrors the per-event-type split
// (#1277) on the producer side — the function pointer captures which event
// type to enqueue when the button is "pressed". The `index` field is only
// consulted for `MenuSelectLevelEvent` / `MenuSelectDiffEvent`.
struct TestMenuButtonData {
    void (*enqueue)(entt::dispatcher&, uint8_t index) = nullptr;
    uint8_t index = 0;
};

// ── Button-press injection helper ──────────────────────────────────────────
// Per #1202/#1204/#1277: enqueues the per-shape ShapePress*Event (or one of
// the per-action Menu*Event types for menu buttons) corresponding to the
// button's TestShapeButtonData / TestMenuButtonData. The per-shape table
// mirrors the same function-pointer-per-row mechanic used by
// `kEnqueueShapePress` in test_player_system.cpp.
inline void press_button(entt::registry& reg, entt::entity btn) {
    auto& disp = reg.ctx().get<entt::dispatcher>();
    if (reg.all_of<TestShapeButtonData>(btn)) {
        const auto shape = reg.get<TestShapeButtonData>(btn).shape;
        const int idx = shape_index(shape);
        if (idx < 0) return;
        using EnqueueFn = void (*)(entt::dispatcher&);
        static constexpr EnqueueFn kEnqueueFns[kShapeCount] = {
            [](entt::dispatcher& d){ d.enqueue<ShapePressCircleEvent>({});   },
            [](entt::dispatcher& d){ d.enqueue<ShapePressSquareEvent>({});   },
            [](entt::dispatcher& d){ d.enqueue<ShapePressTriangleEvent>({}); },
            [](entt::dispatcher&){},  // Hexagon: never pressed; tests never set this
        };
        kEnqueueFns[idx](disp);
    } else if (reg.all_of<TestMenuButtonData>(btn)) {
        const auto& mb = reg.get<TestMenuButtonData>(btn);
        if (mb.enqueue) mb.enqueue(disp, mb.index);
    }
}

inline void run_semantic_input_tick(entt::registry& reg, float dt = 0.016f) {
    game_state_system(reg, dt);
}

// Sets up a registry with rhythm singletons included
inline entt::registry make_rhythm_registry() {
    entt::registry reg = make_registry();
    auto& song = reg.ctx().get<SongState>();
    song.bpm = 120.0f;
    song.offset = 0.0f;
    song.lead_beats = 4;
    song.duration_sec = 60.0f;
    song.playing = true;
    song_state_compute_derived(song);
    reg.ctx().get<EnergyState>() = EnergyState{};
    reg.ctx().get<SongResults>() = SongResults{};
    return reg;
}

// ── BeatCursor / LastLoggedBeat helpers (issue #1545) ─────────────────────
// The legacy `SongState::current_beat = -1` and
// `SessionLog::last_logged_beat = -1` sentinels migrated to ctx-singleton
// row tables (Fabian Principle 3). These helpers preserve the legacy
// `song.current_beat = N` / `log.last_logged_beat = N` ergonomic in tests:
//   - N >= 0 → row exists with that value;
//   - N <  0 → row absent (legacy "fresh-session" semantics).

inline void set_beat_cursor(entt::registry& reg, int beat) {
    if (beat < 0) {
        if (reg.ctx().contains<BeatCursor>()) {
            reg.ctx().erase<BeatCursor>();
        }
    } else {
        reg.ctx().insert_or_assign(BeatCursor{beat});
    }
}

inline int beat_cursor_value(const entt::registry& reg) {
    const auto* c = reg.ctx().find<BeatCursor>();
    return c ? c->last_crossed : -1;
}

inline void set_last_logged_beat(entt::registry& reg, int beat) {
    if (beat < 0) {
        if (reg.ctx().contains<LastLoggedBeat>()) {
            reg.ctx().erase<LastLoggedBeat>();
        }
    } else {
        reg.ctx().insert_or_assign(LastLoggedBeat{beat});
    }
}

inline int last_logged_beat_value(const entt::registry& reg) {
    const auto* l = reg.ctx().find<LastLoggedBeat>();
    return l ? l->beat : -1;
}

// Creates a player entity in lane 1 (center) with default shape (Circle)
inline entt::entity make_player(entt::registry& reg) {
    auto player = reg.create();
    reg.emplace<PlayerTag>(player);
    reg.emplace<WorldPosition>(player, WorldPosition{{constants::LANE_X[1], constants::PLAYER_Y}});
    reg.emplace<PlayerShape>(player);
    // Default player shape is Circle (presence of `ShapeCircleTag`) — matches
    // the legacy `PlayerShape::current` default (enum first value).
    set_player_shape_tag(reg, player, Shape::Circle);
    reg.emplace<ShapeWindow>(player);
    set_target_shape_tag(reg, player, Shape::Circle);
    reg.emplace<Lane>(player);
    // Grounded == absence of Jumping/Sliding (issue #1202/#1204).
    reg.emplace<Color>(player, Color{80, 180, 255, 255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<TagWorldPass>(player);
    return player;
}

// Creates a player entity for rhythm mode (starts as Hexagon)
inline entt::entity make_rhythm_player(entt::registry& reg) {
    auto player = make_player(reg);
    set_player_shape_tag(reg, player, Shape::Hexagon);
    set_target_shape_tag(reg, player, Shape::Hexagon);
    // Idle window phase = absence of all `ShapeWindow*Tag` (#1202/#1204).
    return player;
}

// ── Shape window phase test helpers (#1202/#1204) ──────────────────────────
// The former WindowPhase enum was eradicated in favour of per-phase tags on
// the player entity. These helpers preserve the test-readable
// "set the player into phase X" pattern that old `sw.phase = …` used.

inline void set_window_phase_idle(entt::registry& reg, entt::entity player) {
    reg.remove<ShapeWindowMorphInTag>(player);
    reg.remove<ShapeWindowActiveTag>(player);
    reg.remove<ShapeWindowMorphOutTag>(player);
}

inline void set_window_phase_morph_in(entt::registry& reg, entt::entity player) {
    reg.remove<ShapeWindowActiveTag>(player);
    reg.remove<ShapeWindowMorphOutTag>(player);
    reg.emplace_or_replace<ShapeWindowMorphInTag>(player);
}

inline void set_window_phase_active(entt::registry& reg, entt::entity player) {
    reg.remove<ShapeWindowMorphInTag>(player);
    reg.remove<ShapeWindowMorphOutTag>(player);
    reg.emplace_or_replace<ShapeWindowActiveTag>(player);
}

inline void set_window_phase_morph_out(entt::registry& reg, entt::entity player) {
    reg.remove<ShapeWindowMorphInTag>(player);
    reg.remove<ShapeWindowActiveTag>(player);
    reg.emplace_or_replace<ShapeWindowMorphOutTag>(player);
}

inline bool window_phase_is_idle(const entt::registry& reg, entt::entity player) {
    return !reg.any_of<ShapeWindowMorphInTag,
                       ShapeWindowActiveTag,
                       ShapeWindowMorphOutTag>(player);
}

inline bool window_phase_is_morph_in(const entt::registry& reg, entt::entity player) {
    return reg.all_of<ShapeWindowMorphInTag>(player);
}

inline bool window_phase_is_active(const entt::registry& reg, entt::entity player) {
    return reg.all_of<ShapeWindowActiveTag>(player);
}

inline bool window_phase_is_morph_out(const entt::registry& reg, entt::entity player) {
    return reg.all_of<ShapeWindowMorphOutTag>(player);
}

// Creates a shape gate obstacle at given y, requiring given shape
inline entt::entity make_shape_gate(entt::registry& reg, Shape shape, float y) {
    const auto& song = reg.ctx().get<SongState>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ShapeGateTag>(obs);
    reg.emplace<WorldPosition>(obs, WorldPosition{{constants::LANE_X[1], y}});
    reg.emplace<Vector2>(obs, Vector2{0.0f, song.scroll_speed});
    reg.emplace<Obstacle>(obs, int16_t{constants::PTS_SHAPE_GATE});
    set_required_shape_tag(reg, obs, shape);
    reg.emplace<int8_t>(obs, int8_t{1});
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
    reg.emplace<TagWorldPass>(obs);
    reg.emplace<Color>(obs, Color{255, 255, 255, 255});
    return obs;
}

// Creates a split path requiring shape AND specific lane
inline entt::entity make_split_path(entt::registry& reg, Shape shape, int8_t lane, float y) {
    const auto& song = reg.ctx().get<SongState>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<SplitPathTag>(obs);
    reg.emplace<WorldPosition>(obs, WorldPosition{{constants::LANE_X[1], y}});
    reg.emplace<Vector2>(obs, Vector2{0.0f, song.scroll_speed});
    reg.emplace<Obstacle>(obs, int16_t{constants::PTS_SPLIT_PATH});
    set_required_shape_tag(reg, obs, shape);
    reg.emplace<int8_t>(obs, lane);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
    reg.emplace<TagWorldPass>(obs);
    reg.emplace<Color>(obs, Color{255, 215, 0, 255});
    return obs;
}


// ── UI Button helpers for tests ──────────────────────────────────────────────

inline entt::entity make_shape_button(entt::registry& reg, Shape shape) {
    auto btn = reg.create();
    reg.emplace<TestShapeButtonData>(btn, shape);
    return btn;
}

// Per-action menu-button factories (#1277). Each factory wires up the
// corresponding per-event-type enqueue so `press_button(reg, btn)` produces
// the right event type with no enum discriminator.
inline entt::entity make_menu_confirm_button(entt::registry& reg) {
    auto btn = reg.create();
    reg.emplace<TestMenuButtonData>(btn,
        +[](entt::dispatcher& d, uint8_t){ d.enqueue<MenuConfirmEvent>({}); },
        uint8_t{0});
    return btn;
}

inline entt::entity make_menu_restart_button(entt::registry& reg) {
    auto btn = reg.create();
    reg.emplace<TestMenuButtonData>(btn,
        +[](entt::dispatcher& d, uint8_t){ d.enqueue<MenuRestartEvent>({}); },
        uint8_t{0});
    return btn;
}

inline entt::entity make_menu_go_level_select_button(entt::registry& reg) {
    auto btn = reg.create();
    reg.emplace<TestMenuButtonData>(btn,
        +[](entt::dispatcher& d, uint8_t){ d.enqueue<MenuGoLevelSelectEvent>({}); },
        uint8_t{0});
    return btn;
}

inline entt::entity make_menu_go_main_menu_button(entt::registry& reg) {
    auto btn = reg.create();
    reg.emplace<TestMenuButtonData>(btn,
        +[](entt::dispatcher& d, uint8_t){ d.enqueue<MenuGoMainMenuEvent>({}); },
        uint8_t{0});
    return btn;
}

inline entt::entity make_menu_select_level_button(entt::registry& reg, uint8_t index) {
    auto btn = reg.create();
    reg.emplace<TestMenuButtonData>(btn,
        +[](entt::dispatcher& d, uint8_t i){ d.enqueue<MenuSelectLevelEvent>({i}); },
        index);
    return btn;
}

inline entt::entity make_menu_select_diff_button(entt::registry& reg, uint8_t index) {
    auto btn = reg.create();
    reg.emplace<TestMenuButtonData>(btn,
        +[](entt::dispatcher& d, uint8_t i){ d.enqueue<MenuSelectDiffEvent>({i}); },
        index);
    return btn;
}
