#pragma once

#include <entt/entt.hpp>
#include "components/transform.h"
#include "components/player.h"
#include "components/obstacle.h"
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
#include "components/high_score.h"
#include "systems/gameplay_intents.h"
#include "constants.h"
#include "entities/beat_map.h"
#include "entities/obstacle_render_entity.h"
#include "systems/all_systems.h"
#include "systems/runtime_systems.h"
#include "systems/input_routing.h"
#include "systems/input_system_private.h"

// Sets up a registry with all singletons in their default state
inline entt::registry make_registry() {
    entt::registry reg;
    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<InputSystemPrivate>();
    reg.ctx().emplace<ScreenTransform>();
    reg.ctx().emplace<entt::dispatcher>();
    wire_input_dispatcher(reg);
    wire_audio_haptic_dispatcher(reg);
    reg.ctx().emplace<GameState>(GameState{
        GamePhase::Playing, 0.0f, false, GamePhase::Playing
    });
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
    reg.ctx().emplace<GameOverState>();
    runtime_system_scratch_init(reg);
    return reg;
}

struct GoCapture {
    GoEvent  buf[8] = {};
    int      count  = 0;
    void capture(const GoEvent& e) { if (count < 8) buf[count++] = e; }
};

struct PressCapture {
    ButtonPressEvent buf[8] = {};
    int              count  = 0;
    void capture(const ButtonPressEvent& e) { if (count < 8) buf[count++] = e; }
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
    disp.sink<ButtonPressEvent>().connect<&PressCapture::capture>(cap);
    disp.update<ButtonPressEvent>();
    disp.sink<ButtonPressEvent>().disconnect<&PressCapture::capture>(cap);
    return cap;
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

// ── Button-press injection helper (#273) ────────────────────────────────────
// Enqueue a semantic ButtonPressEvent for a given entity, encoding the
// payload at call time (no live entity handle stored in the event).
inline void press_button(entt::registry& reg, entt::entity btn) {
    auto& disp = reg.ctx().get<entt::dispatcher>();
    if (reg.all_of<TestShapeButtonData>(btn)) {
        auto shape = reg.get<TestShapeButtonData>(btn).shape;
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, shape,
                                       MenuActionKind::Confirm, 0});
    } else if (reg.all_of<TestMenuButtonData>(btn)) {
        auto& ma = reg.get<TestMenuButtonData>(btn);
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Menu, Shape::Circle,
                                        ma.kind, ma.index});
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

// Creates a player entity in lane 1 (center) with default shape (Hexagon in rhythm mode)
inline entt::entity make_player(entt::registry& reg) {
    auto player = reg.create();
    reg.emplace<PlayerTag>(player);
    reg.emplace<WorldTransform>(player, WorldTransform{{constants::LANE_X[1], constants::PLAYER_Y}});
    reg.emplace<PlayerShape>(player);
    reg.emplace<ShapeWindow>(player);
    reg.emplace<Lane>(player);
    reg.emplace<VerticalState>(player);
    reg.emplace<Color>(player, Color{80, 180, 255, 255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<DrawLayer>(player, Layer::Game);
    reg.emplace<TagWorldPass>(player);
    return player;
}

// Creates a player entity for rhythm mode (starts as Hexagon)
inline entt::entity make_rhythm_player(entt::registry& reg) {
    auto player = make_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    ps.current = Shape::Hexagon;
    auto& sw = reg.get<ShapeWindow>(player);
    sw.target_shape = Shape::Hexagon;
    sw.phase = WindowPhase::Idle;
    return player;
}

// Creates a shape gate obstacle at given y, requiring given shape
inline entt::entity make_shape_gate(entt::registry& reg, Shape shape, float y) {
    const auto& song = reg.ctx().get<SongState>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[1], y}});
    reg.emplace<MotionVelocity>(obs, MotionVelocity{{0.0f, song.scroll_speed}});
    reg.emplace<Obstacle>(obs, int16_t{constants::PTS_SHAPE_GATE});
    reg.emplace<RequiredShape>(obs, shape);
    reg.emplace<ShapeGateLane>(obs, int8_t{1});
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<TagWorldPass>(obs);
    reg.emplace<Color>(obs, Color{255, 255, 255, 255});
    return obs;
}

// Creates a legacy lane-block component fixture. Active beatmaps and runtime
// obstacle factories reject LaneBlock; tests use this to cover old ECS data.
inline entt::entity make_lane_block(entt::registry& reg, uint8_t mask, float y) {
    const auto& song = reg.ctx().get<SongState>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[1], y}});
    reg.emplace<MotionVelocity>(obs, MotionVelocity{{0.0f, song.scroll_speed}});
    reg.emplace<Obstacle>(obs, int16_t{constants::PTS_LANE_BLOCK});
    reg.emplace<BlockedLanes>(obs, mask);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W / 3), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<TagWorldPass>(obs);
    reg.emplace<Color>(obs, Color{255, 60, 60, 255});
    return obs;
}


// Creates a legacy combo-gate component fixture. Active beatmaps and runtime
// obstacle factories reject ComboGate; tests use this to cover old ECS data.
inline entt::entity make_combo_gate(entt::registry& reg, Shape shape, uint8_t blocked_mask, float y) {
    const auto& song = reg.ctx().get<SongState>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[1], y}});
    reg.emplace<MotionVelocity>(obs, MotionVelocity{{0.0f, song.scroll_speed}});
    reg.emplace<Obstacle>(obs, int16_t{constants::PTS_COMBO_GATE});
    reg.emplace<RequiredShape>(obs, shape);
    reg.emplace<BlockedLanes>(obs, blocked_mask);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<TagWorldPass>(obs);
    reg.emplace<Color>(obs, Color{200, 100, 255, 255});
    return obs;
}

// Creates a split path requiring shape AND specific lane
inline entt::entity make_split_path(entt::registry& reg, Shape shape, int8_t lane, float y) {
    const auto& song = reg.ctx().get<SongState>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{constants::LANE_X[1], y}});
    reg.emplace<MotionVelocity>(obs, MotionVelocity{{0.0f, song.scroll_speed}});
    reg.emplace<Obstacle>(obs, int16_t{constants::PTS_SPLIT_PATH});
    reg.emplace<RequiredShape>(obs, shape);
    reg.emplace<RequiredLane>(obs, lane);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
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
