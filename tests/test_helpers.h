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
    reg.ctx().emplace<HighScoreState>();
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
    GoEvent  buf[8] = {};
    int      count  = 0;
    void capture(const GoEvent& e) { if (count < 8) buf[count++] = e; }
};

struct PressCapture {
    int circle = 0;
    int square = 0;
    int triangle = 0;
    MenuPressEvent menu_buf[8] = {};
    int menu_count = 0;

    int shape_count() const { return circle + square + triangle; }
    int count() const { return shape_count() + menu_count; }

    void on_circle  (const ShapePressCircleEvent&)   { ++circle;   }
    void on_square  (const ShapePressSquareEvent&)   { ++square;   }
    void on_triangle(const ShapePressTriangleEvent&) { ++triangle; }
    void on_menu(const MenuPressEvent& e) {
        if (menu_count < 8) menu_buf[menu_count++] = e;
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
    disp.sink<GoEvent>().connect<&GoCapture::capture>(cap);
    disp.update<GoEvent>();
    disp.sink<GoEvent>().disconnect<&GoCapture::capture>(cap);
    return cap;
}

inline PressCapture drain_press_events(entt::registry& reg) {
    PressCapture cap;
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.sink<ShapePressCircleEvent>().connect<&PressCapture::on_circle>(cap);
    disp.sink<ShapePressSquareEvent>().connect<&PressCapture::on_square>(cap);
    disp.sink<ShapePressTriangleEvent>().connect<&PressCapture::on_triangle>(cap);
    disp.sink<MenuPressEvent>().connect<&PressCapture::on_menu>(cap);
    disp.update<ShapePressCircleEvent>();
    disp.update<ShapePressSquareEvent>();
    disp.update<ShapePressTriangleEvent>();
    disp.update<MenuPressEvent>();
    disp.sink<ShapePressCircleEvent>().disconnect<&PressCapture::on_circle>(cap);
    disp.sink<ShapePressSquareEvent>().disconnect<&PressCapture::on_square>(cap);
    disp.sink<ShapePressTriangleEvent>().disconnect<&PressCapture::on_triangle>(cap);
    disp.sink<MenuPressEvent>().disconnect<&PressCapture::on_menu>(cap);
    return cap;
}

// Drains all press-event queues (per-shape ShapePress* + MenuPressEvent) in
// the same order game_state_system uses (issue #1202/#1204). Use in tests
// that previously called `disp.update<ButtonPressEvent>()` directly.
inline void update_press_events(entt::registry& reg) {
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.update<ShapePressCircleEvent>();
    disp.update<ShapePressSquareEvent>();
    disp.update<ShapePressTriangleEvent>();
    disp.update<MenuPressEvent>();
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

// ── Semantic input injection helpers ─────────────────────────────────────────
inline void push_go(entt::registry& reg, Direction dir) {
    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({dir});
}

struct TestShapeButtonData {
    Shape shape = Shape::Circle;
};

struct TestMenuButtonData {
    MenuActionKind kind = MenuActionKind::Confirm;
    uint8_t index = 0;
};

// ── Button-press injection helper ──────────────────────────────────────────
// Per #1202/#1204: enqueues the per-shape ShapePress*Event (or MenuPressEvent
// for menu buttons) corresponding to the button's TestShapeButtonData /
// TestMenuButtonData. The per-shape table mirrors the same function-pointer-
// per-row mechanic used by `kEnqueueShapePress` in test_player_system.cpp.
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
        const auto& ma = reg.get<TestMenuButtonData>(btn);
        disp.enqueue<MenuPressEvent>({ma.kind, ma.index});
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

// Creates a player entity in lane 1 (center) with default shape (Circle)
inline entt::entity make_player(entt::registry& reg) {
    auto player = reg.create();
    reg.emplace<PlayerTag>(player);
    reg.emplace<WorldTransform>(player, WorldTransform{{constants::LANE_X[1], constants::PLAYER_Y}});
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
    reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[1], y}});
    reg.emplace<Vector2>(obs, Vector2{0.0f, song.scroll_speed});
    reg.emplace<Obstacle>(obs, int16_t{constants::PTS_SHAPE_GATE});
    set_required_shape_tag(reg, obs, shape);
    reg.emplace<ShapeGateLane>(obs, int8_t{1});
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
    reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[1], y}});
    reg.emplace<Vector2>(obs, Vector2{0.0f, song.scroll_speed});
    reg.emplace<Obstacle>(obs, int16_t{constants::PTS_SPLIT_PATH});
    set_required_shape_tag(reg, obs, shape);
    reg.emplace<RequiredLane>(obs, lane);
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

inline entt::entity make_menu_button(entt::registry& reg, MenuActionKind kind,
                                      uint8_t index = 0) {
    auto btn = reg.create();
    reg.emplace<TestMenuButtonData>(btn, kind, index);
    return btn;
}
