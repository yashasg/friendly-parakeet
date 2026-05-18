#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

namespace {

// Post-#1202/#1204: the per-tier discriminator is now a tag (one of
// TimingPerfect/Good/Ok/Bad), not an enum field on TimingGrade. We capture
// the resulting tier as 4 booleans so the test struct stays Fabian-clean
// (no discriminator enum, columns are independent existential bits).
struct ScoredTimingResult {
    bool perfect{false};
    bool good{false};
    bool ok{false};
    bool bad{false};
    SongResults results{};
};

ScoredTimingResult score_rhythm_shape_hit_at_offset(float arrival_offset_seconds) {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();

    song.song_time = 5.0f;
    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    reg.remove<WindowGraded>(player);
    reg.emplace_or_replace<Pressed>(player, Pressed{song.song_time});

    const float arrival_time = song.song_time + arrival_offset_seconds;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, arrival_time, arrival_time - song.lead_time);

    collision_system(reg, 0.016f);

    REQUIRE(reg.all_of<TimingGrade>(obs));
    ScoredTimingResult out{};
    out.perfect = reg.all_of<TimingPerfectTag>(obs);
    out.good    = reg.all_of<TimingGoodTag>(obs);
    out.ok      = reg.all_of<TimingOkTag>(obs);
    out.bad     = reg.all_of<TimingBadTag>(obs);

    scoring_system(reg, 0.016f);

    out.results = reg.ctx().get<SongResults>();
    return out;
}

}  // namespace

// ── collision_system: Hexagon rejection ──────────────────────

TEST_CASE("collision: Hexagon shape never matches shape gate", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    set_player_shape_tag(reg, p, Shape::Hexagon);
    // Gate requires Circle, player is Hexagon
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("collision: Hexagon fails even when matching gate shape", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    set_player_shape_tag(reg, p, Shape::Hexagon);
    // This should NOT pass — Hexagon is the neutral shape
    // Note: make_shape_gate with Hexagon isn't typical, but tests the guard
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ShapeGateTag>(obs);
    reg.emplace<WorldPosition>(obs, WorldPosition{{constants::LANE_X[1], constants::PLAYER_Y}});
    reg.emplace<Vector2>(obs, Vector2{0.0f, 400.0f});
    reg.emplace<Obstacle>(obs, int16_t{200});
    set_required_shape_tag(reg, obs, Shape::Hexagon);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
    reg.emplace<Color>(obs, Color{255, 255, 255, 255});

    collision_system(reg, 0.016f);
    // Hexagon should NEVER clear shape gates
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    // Hexagon should NEVER clear shape gates
    CHECK_FALSE(is_phase_transition_pending(reg));
    auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

// ── collision_system: rhythm mode timing grades ──────────────

TEST_CASE("collision: rhythm mode assigns Perfect for on-time hit", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    reg.remove<WindowGraded>(player);
    sw.window_start = song.song_time;
    reg.emplace_or_replace<Pressed>(player, Pressed{song.song_time});

    // obstacle arrives right at press time (perfect)
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    collision_system(reg, 0.016f);

    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.all_of<TimingPerfectTag>(obs));
}

TEST_CASE("collision: on-beat rhythm press clears matching gate during MorphIn",
          "[collision][rhythm][issue955]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();

    song.song_time = 5.0f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);
    update_press_events(reg);

    REQUIRE(reg.all_of<ShapeHexagonTag>(player));
    REQUIRE(window_phase_is_morph_in(reg, player));
    REQUIRE(reg.all_of<TargetShapeCircleTag>(player));

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.all_of<TimingPerfectTag>(obs));
}

TEST_CASE("collision: on-beat shape press requires player hitbox to reach target lane",
          "[collision][rhythm][issue892]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 5.0f;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.get<WorldPosition>(obs).position.x = constants::LANE_X[0];
    reg.get<int8_t>(obs) = int8_t{0};
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    auto& lane = reg.get<Lane>(player);
    lane.target = 0;
    lane.lerp_t = 0.0f;

    auto btn = make_shape_button(reg, Shape::Circle);
    press_button(reg, btn);
    update_press_events(reg);
    song.song_time += song.morph_duration + 0.001f;
    shape_window_system(reg, song.morph_duration + 0.001f);

    const auto& transform = reg.get<WorldPosition>(player);
    REQUIRE(lane.current == 1);
    REQUIRE(lane.target == 0);
    REQUIRE(transform.position.x == constants::LANE_X[1]);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK(reg.all_of<MissTag>(obs));
    CHECK_FALSE(reg.all_of<TimingGrade>(obs));
}

TEST_CASE("collision: rhythm mode assigns Bad for far-off hit", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    reg.remove<WindowGraded>(player);
    sw.window_start = song.song_time;
    reg.emplace_or_replace<Pressed>(player, Pressed{song.song_time});

    // obstacle arrival time far from press time -> Bad
    float bad_arrival = song.song_time + song.half_window * 1.2f;
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, bad_arrival, bad_arrival - song.lead_time);

    collision_system(reg, 0.016f);

    REQUIRE(reg.all_of<TimingGrade>(obs));
    CHECK(reg.all_of<TimingBadTag>(obs));
}

TEST_CASE("collision: rhythm shape gate only matches during Active window",
          "[collision][rhythm][issue765]") {
    auto active_reg = make_rhythm_registry();
    auto active_player = make_rhythm_player(active_reg);
    auto& active_song = active_reg.ctx().get<SongState>();

    active_song.song_time = 5.0f;
    set_player_shape_tag(active_reg, active_player, Shape::Circle);
    set_target_shape_tag(active_reg, active_player, Shape::Circle);
    set_window_phase_active(active_reg, active_player);
    active_reg.emplace_or_replace<Pressed>(active_player, Pressed{active_song.song_time});

    auto active_obs = make_shape_gate(active_reg, Shape::Circle, constants::PLAYER_Y);
    active_reg.emplace<BeatInfo>(active_obs, 0, active_song.song_time,
                                 active_song.song_time - active_song.lead_time);

    collision_system(active_reg, 0.016f);

    CHECK(active_reg.all_of<ScoredTag>(active_obs));
    CHECK_FALSE(active_reg.all_of<MissTag>(active_obs));
    CHECK(active_reg.all_of<TimingGrade>(active_obs));

    auto morphout_reg = make_rhythm_registry();
    auto morphout_player = make_rhythm_player(morphout_reg);
    auto& morphout_song = morphout_reg.ctx().get<SongState>();

    morphout_song.song_time = 5.0f;
    set_player_shape_tag(morphout_reg, morphout_player, Shape::Circle);
    set_target_shape_tag(morphout_reg, morphout_player, Shape::Circle);
    set_window_phase_morph_out(morphout_reg, morphout_player);
    morphout_reg.emplace_or_replace<Pressed>(
        morphout_player,
        Pressed{morphout_song.song_time - morphout_song.window_duration});

    auto morphout_obs = make_shape_gate(morphout_reg, Shape::Circle, constants::PLAYER_Y);
    morphout_reg.emplace<BeatInfo>(morphout_obs, 0, morphout_song.song_time,
                                   morphout_song.song_time - morphout_song.lead_time);

    collision_system(morphout_reg, 0.016f);

    CHECK(morphout_reg.all_of<ScoredTag>(morphout_obs));
    CHECK(morphout_reg.all_of<MissTag>(morphout_obs));
    CHECK_FALSE(morphout_reg.all_of<TimingGrade>(morphout_obs));
}

TEST_CASE("collision: finished song still requires active shape window",
          "[collision][rhythm][issue950]") {
    auto idle_reg = make_rhythm_registry();
    auto idle_player = make_rhythm_player(idle_reg);
    auto& idle_song = idle_reg.ctx().get<SongState>();

    idle_song.playing = false;
    idle_song.finished = true;
    set_player_shape_tag(idle_reg, idle_player, Shape::Circle);
    set_target_shape_tag(idle_reg, idle_player, Shape::Circle);
    set_window_phase_idle(idle_reg, idle_player);

    auto idle_obs = make_shape_gate(idle_reg, Shape::Circle, constants::PLAYER_Y);

    collision_system(idle_reg, 0.016f);

    CHECK(idle_reg.all_of<ScoredTag>(idle_obs));
    CHECK(idle_reg.all_of<MissTag>(idle_obs));
    CHECK_FALSE(idle_reg.all_of<TimingGrade>(idle_obs));

    auto active_reg = make_rhythm_registry();
    auto active_player = make_rhythm_player(active_reg);
    auto& active_song = active_reg.ctx().get<SongState>();

    active_song.playing = false;
    active_song.finished = true;
    active_song.song_time = 5.0f;
    set_player_shape_tag(active_reg, active_player, Shape::Circle);
    set_target_shape_tag(active_reg, active_player, Shape::Circle);
    set_window_phase_active(active_reg, active_player);
    active_reg.emplace_or_replace<Pressed>(active_player, Pressed{active_song.song_time});

    auto active_obs = make_shape_gate(active_reg, Shape::Circle, constants::PLAYER_Y);
    active_reg.emplace<BeatInfo>(active_obs, 0, active_song.song_time,
                                 active_song.song_time - active_song.lead_time);

    collision_system(active_reg, 0.016f);

    CHECK(active_reg.all_of<ScoredTag>(active_obs));
    CHECK_FALSE(active_reg.all_of<MissTag>(active_obs));
    CHECK(active_reg.all_of<TimingGrade>(active_obs));
}

TEST_CASE("collision: rhythm miss increments miss_count in SongResults", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);
    // Player is Hexagon, gate requires Circle → miss
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);

    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.miss_count == 1);
}

TEST_CASE("collision: rhythm perfect increments perfect_count in SongResults", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();

    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    reg.remove<WindowGraded>(player);
    reg.emplace_or_replace<Pressed>(player, Pressed{song.song_time});

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);

    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.perfect_count == 1);
}

TEST_CASE("collision/scoring: rhythm Good increments SongResults good_count (#214)",
          "[collision][rhythm][issue214]") {
    const auto result = score_rhythm_shape_hit_at_offset(
        (kTimingPerfectSeconds + kTimingGoodSeconds) * 0.5f);

    CHECK(result.good);
    CHECK_FALSE(result.perfect);
    CHECK_FALSE(result.ok);
    CHECK_FALSE(result.bad);
    CHECK(result.results.perfect_count == 0);
    CHECK(result.results.good_count == 1);
    CHECK(result.results.ok_count == 0);
    CHECK(result.results.bad_count == 0);
}

TEST_CASE("collision/scoring: rhythm Ok increments SongResults ok_count (#214)",
          "[collision][rhythm][issue214]") {
    const auto result = score_rhythm_shape_hit_at_offset(
        (kTimingGoodSeconds + kTimingOkSeconds) * 0.5f);

    CHECK(result.ok);
    CHECK_FALSE(result.perfect);
    CHECK_FALSE(result.good);
    CHECK_FALSE(result.bad);
    CHECK(result.results.perfect_count == 0);
    CHECK(result.results.good_count == 0);
    CHECK(result.results.ok_count == 1);
    CHECK(result.results.bad_count == 0);
}

TEST_CASE("collision/scoring: rhythm Bad increments SongResults bad_count (#214)",
          "[collision][rhythm][issue214]") {
    const auto result = score_rhythm_shape_hit_at_offset(kTimingOkSeconds + 0.001f);

    CHECK(result.bad);
    CHECK_FALSE(result.perfect);
    CHECK_FALSE(result.good);
    CHECK_FALSE(result.ok);
    CHECK(result.results.perfect_count == 0);
    CHECK(result.results.good_count == 0);
    CHECK(result.results.ok_count == 0);
    CHECK(result.results.bad_count == 1);
}

TEST_CASE("collision: multiple misses accumulate in miss_count", "[collision][rhythm]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);

    // First miss
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    // Reset game over for second collision
    clear_next_phase_tags(reg);
    set_test_phase<GamePhasePlayingTag>(reg);

    // Second miss
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.miss_count == 2);
}


// ── collision_system: split path ─────────────────────────────

TEST_CASE("collision: split path succeeds with correct shape and lane", "[collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    set_player_shape_tag(reg, p, Shape::Triangle);
    auto& lane = reg.get<Lane>(p);
    lane.current = 2;
    reg.get<WorldPosition>(p).position.x = constants::LANE_X[2];

    auto obs = make_split_path(reg, Shape::Triangle, 2, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("collision: Hexagon fails split path even when shape and lane match", "[collision][rhythm][issue219]") {
    auto reg = make_rhythm_registry();
    make_rhythm_player(reg);

    auto obs = make_split_path(reg, Shape::Hexagon, 1, constants::PLAYER_Y);

    collision_system(reg, 0.016f);

    REQUIRE(reg.all_of<MissTag>(obs));
    CHECK_FALSE(reg.all_of<TimingGrade>(obs));

    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    const auto& results = reg.ctx().get<SongResults>();
    CHECK(results.miss_count == 1);
    CHECK(results.perfect_count == 0);
    CHECK(results.good_count == 0);
    CHECK(results.ok_count == 0);
    CHECK(results.bad_count == 0);

    const auto& energy = reg.ctx().get<EnergyState>();
    CHECK(energy.energy < 1.0f);
    CHECK(energy.flash_timer > 0.0f);
}

// ── SRP observation: collision_system must NOT mutate SongResults ──────────
// After this move, perfect_count is owned by scoring_system.
// Running collision but withholding scoring must leave all counts at zero.
TEST_CASE("scoring: collision_system alone does not mutate SongResults counts", "[scoring][collision]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& song = reg.ctx().get<SongState>();

    set_player_shape_tag(reg, player, Shape::Circle);
    set_window_phase_active(reg, player);
    reg.remove<WindowGraded>(player);
    reg.emplace_or_replace<Pressed>(player, Pressed{song.song_time});

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obs, 0, song.song_time, song.song_time - song.lead_time);

    // Run collision only — scoring_system intentionally NOT called.
    collision_system(reg, 0.016f);

    // TimingGrade IS emplaced by collision (event component), but SongResults
    // counters must remain zero until scoring_system processes it.
    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.perfect_count == 0);
    CHECK(results.good_count    == 0);
    CHECK(results.ok_count      == 0);
    CHECK(results.bad_count     == 0);
}
