#include <catch2/catch_test_macros.hpp>
#include <type_traits>
#include "test_helpers.h"
#include "components/audio.h"
#include "systems/audio_events.h"
#include "entities/obstacle_entity.h"
#include "entities/obstacle_render_entity.h"

// Verify component defaults and basic ECS operations

TEST_CASE("components: WorldTransform defaults to origin position", "[components][transform]") {
    WorldTransform transform{};
    CHECK(transform.position.x == 0.0f);
    CHECK(transform.position.y == 0.0f);
}

TEST_CASE("components: MotionVelocity defaults to zero vector", "[components][transform]") {
    MotionVelocity velocity{};
    CHECK(velocity.value.x == 0.0f);
    CHECK(velocity.value.y == 0.0f);
}

TEST_CASE("components: rendering components are safely default constructible",
          "[components][rendering]") {
    ModelTransform model{};
    CHECK(model.tint.r == 255);
    CHECK(model.tint.g == 255);
    CHECK(model.tint.b == 255);
    CHECK(model.tint.a == 255);
    CHECK(model.mesh_index == uint8_t{0});
    CHECK(model.mesh_type == MeshType::Shape);

    MeshChild child{};
    const bool parent_is_null = child.parent == entt::null;
    CHECK(parent_is_null);
    CHECK(child.x == 0.0f);
    CHECK(child.z_offset == 0.0f);
    CHECK(child.width == 0.0f);
    CHECK(child.depth == 0.0f);
    CHECK(child.height == 0.0f);
    CHECK(child.tint.r == 255);
    CHECK(child.tint.g == 255);
    CHECK(child.tint.b == 255);
    CHECK(child.tint.a == 255);
    CHECK(child.mesh_index == uint8_t{0});
    CHECK(child.mesh_type == MeshType::Shape);

    ObstacleChildren children{};
    CHECK(children.count == 0);
    for (const auto entity : children.children) {
        CHECK(entity == entt::entity{});
    }
}

TEST_CASE("components: UIPosition is distinct screen-space placement", "[components][transform][ui]") {
    UIPosition position{};
    CHECK(position.value.x == 0.0f);
    CHECK(position.value.y == 0.0f);
    CHECK_FALSE((std::is_same_v<UIPosition, WorldTransform>));
    CHECK_FALSE((std::is_same_v<UIPosition, ScreenPosition>));
}

TEST_CASE("components: PlayerShape defaults to Circle", "[components]") {
    PlayerShape ps{};
    CHECK(ps.current == Shape::Circle);
    CHECK(ps.morph_t == 1.0f);
}

TEST_CASE("components: Lane defaults to center", "[components]") {
    Lane l{};
    CHECK(l.current == 1);
    CHECK(l.target == -1);
    CHECK(l.lerp_t == 1.0f);
}

TEST_CASE("components: VerticalState defaults to grounded", "[components]") {
    VerticalState vs{};
    CHECK(vs.mode == VMode::Grounded);
    CHECK(vs.timer == 0.0f);
    CHECK(vs.y_offset == 0.0f);
}

TEST_CASE("components: InputState stores transient input flags", "[components]") {
    InputState is{};
    is.touch_down = true;
    is.touch_up = true;
    CHECK(is.touch_down);
    CHECK(is.touch_up);
}

TEST_CASE("components: PlaySfxEvent wraps SFX clip", "[components]") {
    PlaySfxEvent e{SFX::ShapeShift};
    CHECK(e.clip == SFX::ShapeShift);
}

TEST_CASE("components: PlayHapticEvent wraps HapticEvent", "[components]") {
    PlayHapticEvent e{HapticEvent::DeathCrash};
    CHECK(e.evt == HapticEvent::DeathCrash);
}

TEST_CASE("components: ScoreState defaults to zero", "[components]") {
    ScoreState s{};
    CHECK(s.score == 0);
    CHECK(s.high_score == 0);
    CHECK(s.chain_count == 0);
}

TEST_CASE("components: GameState defaults to title", "[components]") {
    GameState gs{};
    CHECK(gs.phase == GamePhase::Title);
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("ecs: make_registry creates all singletons", "[ecs]") {
    auto reg = make_registry();
    // These should not throw — assert every ctx singleton required by systems.
    // Input / event transport
    static_cast<void>(reg.ctx().get<InputState>());
    static_cast<void>(reg.ctx().get<entt::dispatcher>());   // #332: must be present
    // Gameplay state
    static_cast<void>(reg.ctx().get<GameState>());
    static_cast<void>(reg.ctx().get<ScoreState>());
    // Audio / haptics / settings (queues replaced by dispatcher events)
    static_cast<void>(settings_state(reg));
    // Level / song / rhythm
    static_cast<void>(reg.ctx().get<LevelSelectState>());
    static_cast<void>(beat_map(reg));
    static_cast<void>(reg.ctx().get<SongState>());
    static_cast<void>(reg.ctx().get<EnergyState>());
    static_cast<void>(reg.ctx().get<SongResults>());
    // Scoring persistence / end-screen
    static_cast<void>(reg.ctx().get<HighScoreState>());
    static_cast<void>(reg.ctx().get<HighScorePersistence>());
    static_cast<void>(reg.ctx().get<GameOverState>());
    SUCCEED("all required ctx singletons exist in registry context");
}

TEST_CASE("ecs: make_registry dispatcher is wired — GoEvent listeners registered", "[ecs][dispatcher]") {
    // Verifies that wire_input_dispatcher() was called during make_registry(),
    // so any system can immediately enqueue+update GoEvents without separate setup.
    auto reg = make_registry();

    // Promote to Playing so player_input_handle_go has observable effect.
    reg.ctx().get<GameState>().phase = GamePhase::Playing;
    auto player = make_player(reg);
    auto& lane  = reg.get<Lane>(player);

    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.enqueue(GoEvent{Direction::Right});
    disp.update<GoEvent>();

    // If dispatcher listeners were NOT wired, lane.target would remain -1.
    CHECK(lane.target == 2);   // listener wired: player_input_handle_go fired
}

TEST_CASE("ecs: make_registry dispatcher is wired — ButtonPressEvent listeners registered", "[ecs][dispatcher]") {
    // Verifies ButtonPressEvent sink is wired: a press event on a valid button
    // in Playing phase reaches player_input_handle_press.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw    = reg.get<ShapeWindow>(player);
    REQUIRE(sw.phase == WindowPhase::Idle);

    auto btn = make_shape_button(reg, Shape::Triangle);

    auto& disp = reg.ctx().get<entt::dispatcher>();
    press_button(reg, btn);
    disp.update<ButtonPressEvent>();

    // If dispatcher listeners were NOT wired, sw.phase would stay Idle.
    CHECK(sw.phase        == WindowPhase::MorphIn);  // listener wired: handle_press fired
    CHECK(sw.target_shape == Shape::Triangle);
}

TEST_CASE("ecs: make_registry dispatcher ctx — second update is a no-op (no replay)", "[ecs][dispatcher]") {
    // Contract: after an authoritative drain, a subsequent update<T>() with no
    // new enqueues must not re-deliver the previously drained event.
    // Applies to both GoEvent and ButtonPressEvent pools.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane  = reg.get<Lane>(player);

    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.enqueue(GoEvent{Direction::Right});
    disp.update<GoEvent>();
    CHECK(lane.target == 2);

    // Second drain — must be a no-op.
    disp.update<GoEvent>();
    CHECK(lane.target == 2);   // not replayed
}

TEST_CASE("ecs: wire_input_dispatcher is idempotent", "[ecs][dispatcher]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto btn = make_shape_button(reg, Shape::Triangle);

    wire_input_dispatcher(reg);
    press_button(reg, btn);
    reg.ctx().get<entt::dispatcher>().update<ButtonPressEvent>();

    CHECK(reg.get<ShapeWindow>(player).phase == WindowPhase::MorphIn);
    CHECK(drain_sfx_events(reg).count == 1);
}

struct ExternalGoListener {
    int count = 0;
    void on_go(const GoEvent&) { ++count; }
};

TEST_CASE("ecs: unwire_input_dispatcher preserves external listeners", "[ecs][dispatcher]") {
    auto reg = make_registry();
    auto& disp = reg.ctx().get<entt::dispatcher>();
    ExternalGoListener listener;
    disp.sink<GoEvent>().connect<&ExternalGoListener::on_go>(listener);

    unwire_input_dispatcher(reg);
    disp.trigger(GoEvent{Direction::Left});

    CHECK(listener.count == 1);
}

TEST_CASE("ecs: make_player creates proper entity", "[ecs]") {
    auto reg = make_registry();
    auto p = make_player(reg);

    CHECK(reg.all_of<PlayerTag>(p));
    CHECK(reg.all_of<WorldTransform>(p));
    CHECK(reg.all_of<PlayerShape>(p));
    CHECK(reg.all_of<ShapeWindow>(p));
    CHECK(reg.all_of<Lane>(p));
    CHECK(reg.all_of<VerticalState>(p));
    CHECK(reg.all_of<Color>(p));
    CHECK(reg.all_of<DrawSize>(p));
    CHECK(reg.all_of<DrawLayer>(p));
    CHECK(reg.all_of<TagWorldPass>(p));
}

TEST_CASE("components: MotionVelocity explicit construction", "[components][transform]") {
    MotionVelocity mv{{5.0f, 10.0f}};
    CHECK(mv.value.x == 5.0f);
    CHECK(mv.value.y == 10.0f);
}

TEST_CASE("components: Color construction", "[components]") {
    Color c{255, 128, 64, 200};
    CHECK(c.r == 255);
    CHECK(c.g == 128);
    CHECK(c.b == 64);
    CHECK(c.a == 200);
}

TEST_CASE("components: DrawSize construction", "[components]") {
    DrawSize ds{100.0f, 50.0f};
    CHECK(ds.w == 100.0f);
    CHECK(ds.h == 50.0f);
}

TEST_CASE("components: Obstacle construction", "[components]") {
    Obstacle obs{int16_t{200}};
    CHECK(obs.base_points == 200);
}

TEST_CASE("components: ParticleData construction", "[components]") {
    ParticleData pd{10.0f};
    CHECK(pd.size == 10.0f);
}

TEST_CASE("ecs: make_combo_gate creates proper entity", "[ecs]") {
    auto reg = make_registry();
    auto obs = make_combo_gate(reg, Shape::Circle, 0b101, 500.0f);

    CHECK(reg.all_of<ObstacleTag>(obs));
    CHECK(reg.all_of<WorldTransform>(obs));
    CHECK(reg.all_of<Obstacle>(obs));
    CHECK(reg.all_of<RequiredShape>(obs));
    CHECK(reg.all_of<BlockedLanes>(obs));
    CHECK(obstacle_kind_from_components(reg.all_of<RequiredShape>(obs),
                                        reg.all_of<BlockedLanes>(obs),
                                        reg.all_of<RequiredLane>(obs)) == ObstacleKind::ComboGate);
    CHECK(reg.get<RequiredShape>(obs).shape == Shape::Circle);
    CHECK(reg.get<BlockedLanes>(obs).mask == 0b101);
}

TEST_CASE("ecs: make_split_path creates proper entity", "[ecs]") {
    auto reg = make_registry();
    auto obs = make_split_path(reg, Shape::Triangle, 2, 500.0f);

    CHECK(reg.all_of<ObstacleTag>(obs));
    CHECK(reg.all_of<Obstacle>(obs));
    CHECK(reg.all_of<RequiredShape>(obs));
    CHECK(reg.all_of<RequiredLane>(obs));
    CHECK(obstacle_kind_from_components(reg.all_of<RequiredShape>(obs),
                                        reg.all_of<BlockedLanes>(obs),
                                        reg.all_of<RequiredLane>(obs)) == ObstacleKind::SplitPath);
    CHECK(reg.get<RequiredShape>(obs).shape == Shape::Triangle);
    CHECK(reg.get<RequiredLane>(obs).lane == 2);
}

// ── GamePhaseBit typed mask tests ─────────────────────────

TEST_CASE("GamePhaseBit: single-phase bit does not overlap others", "[phase_mask]") {
    auto playing = GamePhaseBit::Playing;
    CHECK( (playing & GamePhaseBit::Playing) == GamePhaseBit::Playing);
    CHECK( (playing & GamePhaseBit::Title) != GamePhaseBit::Title);
    CHECK( (playing & GamePhaseBit::LevelSelect) != GamePhaseBit::LevelSelect);
    CHECK( (playing & GamePhaseBit::Paused) != GamePhaseBit::Paused);
    CHECK( (playing & GamePhaseBit::GameOver) != GamePhaseBit::GameOver);
    CHECK( (playing & GamePhaseBit::SongComplete) != GamePhaseBit::SongComplete);
}

TEST_CASE("GamePhaseBit: multi-phase OR mask covers both phases", "[phase_mask]") {
    auto mask = GamePhaseBit::GameOver | GamePhaseBit::SongComplete;
    CHECK((mask & GamePhaseBit::GameOver) == GamePhaseBit::GameOver);
    CHECK((mask & GamePhaseBit::SongComplete) == GamePhaseBit::SongComplete);
    CHECK((mask & GamePhaseBit::Playing) != GamePhaseBit::Playing);
    CHECK((mask & GamePhaseBit::Title) != GamePhaseBit::Title);
    CHECK((mask & GamePhaseBit::LevelSelect) != GamePhaseBit::LevelSelect);
    CHECK((mask & GamePhaseBit::Paused) != GamePhaseBit::Paused);
}

TEST_CASE("GamePhaseBit: all eight phases have distinct bits", "[phase_mask]") {
    // Each GamePhaseBit value must be a distinct power-of-two
    CHECK(GamePhaseBit::Title        != GamePhaseBit::LevelSelect);
    CHECK(GamePhaseBit::LevelSelect  != GamePhaseBit::Playing);
    CHECK(GamePhaseBit::Playing      != GamePhaseBit::Paused);
    CHECK(GamePhaseBit::Paused       != GamePhaseBit::GameOver);
    CHECK(GamePhaseBit::GameOver     != GamePhaseBit::SongComplete);
    CHECK(GamePhaseBit::SongComplete != GamePhaseBit::Settings);
    CHECK(GamePhaseBit::Settings     != GamePhaseBit::Tutorial);
    // Combined mask must not equal any single bit
    GamePhaseBit combined = GamePhaseBit::GameOver | GamePhaseBit::SongComplete;
    CHECK(combined != GamePhaseBit::GameOver);
    CHECK(combined != GamePhaseBit::SongComplete);
}

TEST_CASE("GamePhaseBit: to_phase_bit round-trips all GamePhase values", "[phase_mask]") {
    using P = GamePhase;
    using B = GamePhaseBit;
    CHECK(to_phase_bit(P::Title)        == B::Title);
    CHECK(to_phase_bit(P::LevelSelect)  == B::LevelSelect);
    CHECK(to_phase_bit(P::Playing)      == B::Playing);
    CHECK(to_phase_bit(P::Paused)       == B::Paused);
    CHECK(to_phase_bit(P::GameOver)     == B::GameOver);
    CHECK(to_phase_bit(P::SongComplete) == B::SongComplete);
    CHECK(to_phase_bit(P::Settings)     == B::Settings);
    CHECK(to_phase_bit(P::Tutorial)     == B::Tutorial);
}

TEST_CASE("GamePhaseBit: zero mask has no phase bits set", "[phase_mask]") {
    auto mask = static_cast<GamePhaseBit>(0);
    CHECK((mask & GamePhaseBit::Title) != GamePhaseBit::Title);
    CHECK((mask & GamePhaseBit::Playing) != GamePhaseBit::Playing);
    CHECK((mask & GamePhaseBit::GameOver) != GamePhaseBit::GameOver);
}

namespace {

int mesh_child_count(entt::registry& reg) {
    int count = 0;
    for ([[maybe_unused]] auto entity : reg.view<MeshChild>()) {
        ++count;
    }
    return count;
}


}  // namespace

TEST_CASE("ecs: dispatcher rewire-after-unwire does not duplicate semantic delivery",
          "[ecs][dispatcher][regression]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto button = make_shape_button(reg, Shape::Square);

    unwire_input_dispatcher(reg);
    wire_input_dispatcher(reg);

    auto& sw = reg.get<ShapeWindow>(player);
    REQUIRE(sw.phase == WindowPhase::Idle);

    press_button(reg, button);
    reg.ctx().get<entt::dispatcher>().update<ButtonPressEvent>();

    CHECK(sw.phase == WindowPhase::MorphIn);
    CHECK(sw.target_shape == Shape::Square);
    CHECK(drain_sfx_events(reg).count == 1);
}

TEST_CASE("ecs: repeated unwire_input_dispatcher is idempotent before rewire",
          "[ecs][dispatcher][shutdown]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto button = make_shape_button(reg, Shape::Triangle);

    unwire_input_dispatcher(reg);
    unwire_input_dispatcher(reg);
    wire_input_dispatcher(reg);

    auto& sw = reg.get<ShapeWindow>(player);
    REQUIRE(sw.phase == WindowPhase::Idle);

    press_button(reg, button);
    reg.ctx().get<entt::dispatcher>().update<ButtonPressEvent>();

    CHECK(sw.phase == WindowPhase::MorphIn);
    CHECK(sw.target_shape == Shape::Triangle);
    CHECK(drain_sfx_events(reg).count == 1);
}

TEST_CASE("collision: SongState ctx singleton identity is stable across collision ticks",
          "[collision][song_state][regression]") {
    auto reg = make_registry();
    make_player(reg);

    auto& song = reg.ctx().get<SongState>();
    song.song_time = 12.5f;
    song.scroll_speed = 321.0f;
    SongState* const initial_song = &song;

    collision_system(reg, 0.016f);
    collision_system(reg, 0.016f);

    CHECK(&reg.ctx().get<SongState>() == initial_song);
    CHECK(song.song_time == 12.5f);
    CHECK(song.scroll_speed == 321.0f);
}

TEST_CASE("ecs: obstacle mesh lifecycle explicit cleanup is idempotent",
          "[ecs][obstacle][lifecycle]") {
    entt::registry reg;

    auto parent = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f, Shape::Circle});
    REQUIRE(mesh_child_count(reg) > 0);

    destroy_obstacle_with_children(reg, parent);
    destroy_obstacle_with_children(reg, parent);

    CHECK(mesh_child_count(reg) == 0);
}
