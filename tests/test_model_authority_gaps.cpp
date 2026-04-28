// Model.transform authority — ObstacleScrollZ bridge-state validation tests.
//
// STATUS: Active. All tests compile with zero warnings.
//         Tests in Part A will run and pass once Keaton's WIP stabilises
//         (collision_system.cpp has partial errors blocking the full build).
//
// CONTEXT:
//   Keaton's Slice 1 migration introduces ObstacleScrollZ as the authoritative
//   scroll position for Model-owned obstacles (LowBar, HighBar) in place of
//   Position.  The bridge-state approach (Option A from Baer's recommendations)
//   means EVERY lifecycle system runs TWO structural views — one for the legacy
//   Position path and one for the new ObstacleScrollZ path — giving a zero-
//   regression migration.
//
//   Systems updated by Keaton (Slice 1 WIP):
//     scroll_system:      model_beat_view <ObstacleTag, ObstacleScrollZ, BeatInfo>
//     obstacle_despawn_system:     model_view      <ObstacleTag, ObstacleScrollZ>
//     miss_detection:     model_view      <ObstacleTag, Obstacle, ObstacleScrollZ>
//     scoring_system:     hit_view2       <ObstacleTag, ScoredTag, Obstacle, ObstacleScrollZ>
//     collision_system:   view            <ObstacleTag, ObstacleScrollZ, Obstacle, RequiredVAction>
//                         (PARTIAL — other call sites still WIP at time of filing)
//
// STRUCTURE:
//   Part A  — Active, headless-safe.
//             Positive bridge-state tests: systems DO process ObstacleScrollZ entities.
//             Negative gap tests: bare ObstacleModel (no ObstacleScrollZ, no Position)
//             is still silently skipped — documents required entity invariant.
//
//   Part B  — GUARDED (#if 0).
//             Collision system tests for LowBar/HighBar with ObstacleScrollZ.
//             Enable once collision_system.cpp WIP errors are resolved.
//
// BLOCKER FOR PART B:
//   collision_system.cpp has partial WIP errors:
//     - resolve() lambda takes float obs_z but ShapeGate/LaneBlock/ComboGate/
//       SplitPath call sites still pass Position struct instead of pos.y.
//     - player_in_timing_window() signature updated but LanePush call site not.
//   Owner: Keaton.  Fix remaining call sites then enable Part B.
//
// See also:
//   test_obstacle_model_slice.cpp — Section C (guarded): post-migration archetype tests
//   .squad/decisions/inbox/baer-model-authority-tests.md — full blocker inventory

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "components/obstacle.h"
#include "entities/obstacle_render_entity.h"

// ════════════════════════════════════════════════════════════════════════════
// PART A — Active bridge-state validation (headless-safe, runs today)
// ════════════════════════════════════════════════════════════════════════════

// ── A1. scroll_system: ObstacleScrollZ path ──────────────────────────────────

TEST_CASE("bridge: scroll_system writes ObstacleScrollZ.z for BeatInfo+ObstacleScrollZ entities",
          "[model_authority][scroll]") {
    // Positive test: once Keaton's Slice 1 archetypes land, LowBar/HighBar entities
    // carry ObstacleScrollZ + BeatInfo (not Position). scroll_system's
    // model_beat_view must update oz.z each frame.
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 2.0f;

    // Simulate a migrated LowBar entity: ObstacleScrollZ + BeatInfo, no Position.
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ObstacleScrollZ>(obs, ObstacleScrollZ{0.0f});  // uninitialised
    reg.emplace<BeatInfo>(obs, 0, 4.0f, 0.0f);                // spawn_time=0

    scroll_system(reg, 0.016f);

    float expected_z = constants::SPAWN_Y + (song.song_time - 0.0f) * song.scroll_speed;
    CHECK_THAT(reg.get<ObstacleScrollZ>(obs).z,
               Catch::Matchers::WithinAbs(expected_z, 0.1f));
}

TEST_CASE("bridge: scroll_system legacy Position path unchanged after ObstacleScrollZ addition",
          "[model_authority][scroll]") {
    // Regression guard: the new ObstacleScrollZ view must not break existing
    // Position+BeatInfo obstacles (ShapeGate, LaneBlock, ComboGate, SplitPath).
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 2.0f;

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, 0.0f, 0.0f);
    reg.emplace<BeatInfo>(obs, 0, 4.0f, 0.0f);

    scroll_system(reg, 0.016f);

    float expected_y = constants::SPAWN_Y + (song.song_time - 0.0f) * song.scroll_speed;
    CHECK_THAT(reg.get<Position>(obs).y, Catch::Matchers::WithinAbs(expected_y, 0.1f));
}

TEST_CASE("bridge: entity with ObstacleModel only (no ObstacleScrollZ, no Position) is ignored by scroll",
          "[model_authority][scroll]") {
    // Negative gap: a bare ObstacleModel entity (no bridge-state component) is
    // invisible to scroll_system. This documents the invariant that every
    // Model-owned obstacle MUST have ObstacleScrollZ to participate in scroll.
    auto reg = make_rhythm_registry();
    auto& song = reg.ctx().get<SongState>();
    song.song_time = 2.0f;

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ObstacleModel>(obs);  // owned=false — headless-safe
    reg.emplace<BeatInfo>(obs, 0, 4.0f, 0.0f);
    // Deliberately NO ObstacleScrollZ and NO Position.

    scroll_system(reg, 0.016f);

    // Model.transform.m14 must remain at zero-init (scroll_system skipped it).
    CHECK(reg.get<ObstacleModel>(obs).model.transform.m14 == 0.0f);
}

// ── A2. obstacle_despawn_system: ObstacleScrollZ path ─────────────────────────────────

TEST_CASE("bridge: obstacle_despawn_system destroys ObstacleScrollZ entities past DESTROY_Y",
          "[model_authority][cleanup]") {
    auto reg = make_registry();

    // Migrated entity: ObstacleScrollZ past DESTROY_Y — must be destroyed.
    auto model_obs = reg.create();
    reg.emplace<ObstacleTag>(model_obs);
    reg.emplace<ObstacleScrollZ>(model_obs, ObstacleScrollZ{constants::DESTROY_Y + 10.0f});

    // Legacy entity: Position past DESTROY_Y — also destroyed (regression guard).
    auto pos_obs = reg.create();
    reg.emplace<ObstacleTag>(pos_obs);
    reg.emplace<Position>(pos_obs, 0.0f, constants::DESTROY_Y + 10.0f);

    // Still-live entity: ObstacleScrollZ below DESTROY_Y — must NOT be destroyed.
    auto live_obs = reg.create();
    reg.emplace<ObstacleTag>(live_obs);
    reg.emplace<ObstacleScrollZ>(live_obs, ObstacleScrollZ{constants::PLAYER_Y});

    obstacle_despawn_system(reg, 0.016f);

    CHECK_FALSE(reg.valid(model_obs));  // bridge-state path: destroyed
    CHECK_FALSE(reg.valid(pos_obs));    // legacy path: destroyed (no regression)
    CHECK(reg.valid(live_obs));         // not past DESTROY_Y: survives
}

TEST_CASE("bridge: entity with ObstacleModel only (no ObstacleScrollZ) is not destroyed by cleanup",
          "[model_authority][cleanup]") {
    // Negative gap: invariant test. Any entity that has ObstacleModel but
    // NOT ObstacleScrollZ (and NOT Position) will never be destroyed by
    // obstacle_despawn_system. Such entities should not exist in production — every
    // Model-owned obstacle spawned by apply_obstacle_archetype must also get
    // ObstacleScrollZ at spawn time.
    auto reg = make_registry();

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ObstacleModel>(obs);  // owned=false, no bridge-state
    // Simulate being "past" DESTROY_Y via a sentinel transform.
    auto& om = reg.get<ObstacleModel>(obs);
    om.model.transform.m14 = constants::DESTROY_Y + 100.0f;  // cleanup won't see this

    obstacle_despawn_system(reg, 0.016f);

    // Not destroyed: obstacle_despawn_system does not read Model.transform directly.
    // This confirms that ObstacleScrollZ is REQUIRED alongside ObstacleModel.
    CHECK(reg.valid(obs));
}

// ── A3. miss_detection_system: ObstacleScrollZ path ──────────────────────────

TEST_CASE("bridge: miss_detection tags ObstacleScrollZ entities past DESTROY_Y",
          "[model_authority][miss_detection]") {
    auto reg = make_registry();

    // Migrated entity: ObstacleScrollZ past DESTROY_Y — must be tagged missed.
    auto model_obs = reg.create();
    reg.emplace<ObstacleTag>(model_obs);
    reg.emplace<Obstacle>(model_obs, ObstacleKind::LowBar,
                          static_cast<int16_t>(constants::PTS_LOW_BAR));
    reg.emplace<ObstacleScrollZ>(model_obs, ObstacleScrollZ{constants::DESTROY_Y + 10.0f});

    // Legacy entity: Position past DESTROY_Y — also tagged missed (regression guard).
    auto pos_obs = reg.create();
    reg.emplace<ObstacleTag>(pos_obs);
    reg.emplace<Obstacle>(pos_obs, ObstacleKind::ShapeGate,
                          static_cast<int16_t>(constants::PTS_SHAPE_GATE));
    reg.emplace<Position>(pos_obs, 0.0f, constants::DESTROY_Y + 10.0f);

    // Still-live entity: ObstacleScrollZ before DESTROY_Y — must NOT be tagged.
    auto live_obs = reg.create();
    reg.emplace<ObstacleTag>(live_obs);
    reg.emplace<Obstacle>(live_obs, ObstacleKind::LowBar,
                          static_cast<int16_t>(constants::PTS_LOW_BAR));
    reg.emplace<ObstacleScrollZ>(live_obs, ObstacleScrollZ{constants::PLAYER_Y - 50.0f});

    miss_detection_system(reg, 0.016f);

    // Bridge-state path: tagged
    CHECK(reg.all_of<MissTag>(model_obs));
    CHECK(reg.all_of<ScoredTag>(model_obs));
    // Legacy path: tagged (no regression)
    CHECK(reg.all_of<MissTag>(pos_obs));
    CHECK(reg.all_of<ScoredTag>(pos_obs));
    // Live entity: untouched
    CHECK_FALSE(reg.all_of<MissTag>(live_obs));
    CHECK_FALSE(reg.all_of<ScoredTag>(live_obs));
}

TEST_CASE("bridge: miss_detection idempotent for ObstacleScrollZ entities (no double-tag)",
          "[model_authority][miss_detection]") {
    // Verifies collect-then-emplace pattern is safe for ObstacleScrollZ path.
    // Mirrors the existing Position-based idempotence tests in test_miss_detection_regression.cpp.
    auto reg = make_registry();

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Obstacle>(obs, ObstacleKind::LowBar,
                          static_cast<int16_t>(constants::PTS_LOW_BAR));
    reg.emplace<ObstacleScrollZ>(obs, ObstacleScrollZ{constants::DESTROY_Y + 10.0f});

    miss_detection_system(reg, 0.016f);
    CHECK(reg.all_of<MissTag, ScoredTag>(obs));

    // Second run: exclude<ScoredTag> must filter this entity out.
    miss_detection_system(reg, 0.016f);

    int miss_count = 0;
    for (auto e : reg.view<MissTag>()) { (void)e; ++miss_count; }
    CHECK(miss_count == 1);  // exactly one, not two
}

// ── A4. scoring_system: ObstacleScrollZ hit-pass ─────────────────────────────

TEST_CASE("bridge: scoring_system hit_view2 processes ObstacleScrollZ scored obstacles",
          "[model_authority][scoring]") {
    // Positive test: after collision_system emplace ScoredTag on a LowBar entity,
    // scoring_system's hit_view2 must process and remove Obstacle + ScoredTag.
    auto reg = make_registry();

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Obstacle>(obs, ObstacleKind::LowBar,
                          static_cast<int16_t>(constants::PTS_LOW_BAR));
    reg.emplace<ObstacleScrollZ>(obs, ObstacleScrollZ{constants::PLAYER_Y});
    reg.emplace<ScoredTag>(obs);  // emplace manually (collision_system would do this)

    scoring_system(reg, 0.016f);

    // hit_view2 processed this entity: Obstacle and ScoredTag removed.
    CHECK_FALSE(reg.all_of<Obstacle>(obs));
    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("bridge: scoring_system legacy Position hit-pass unchanged (no regression)",
          "[model_authority][scoring]") {
    auto reg = make_registry();

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Obstacle>(obs, ObstacleKind::ShapeGate,
                          static_cast<int16_t>(constants::PTS_SHAPE_GATE));
    reg.emplace<Position>(obs, 0.0f, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);

    scoring_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<Obstacle>(obs));
    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("bridge: entity with ObstacleModel only (no ObstacleScrollZ) is skipped by scoring hit-pass",
          "[model_authority][scoring]") {
    // Negative gap: a Model-owned entity that was somehow scored (e.g., future
    // edge case) but has no ObstacleScrollZ will not be processed by scoring_system.
    // Documents the invariant: ObstacleScrollZ is required for scoring.
    auto reg = make_registry();

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Obstacle>(obs, ObstacleKind::LowBar,
                          static_cast<int16_t>(constants::PTS_LOW_BAR));
    reg.emplace<ObstacleModel>(obs);  // owned=false, no ObstacleScrollZ
    reg.emplace<ScoredTag>(obs);

    scoring_system(reg, 0.016f);

    // Neither hit_view (needs Position) nor hit_view2 (needs ObstacleScrollZ) sees it.
    CHECK(reg.all_of<ScoredTag>(obs));  // ScoredTag not removed (skipped)
    CHECK(reg.all_of<Obstacle>(obs));   // Obstacle not removed (skipped)
}

// ════════════════════════════════════════════════════════════════════════════
// PART B — GUARDED: Collision system bridge-state tests (LowBar/HighBar)
//
// ENABLE AFTER: Keaton fixes remaining collision_system.cpp WIP errors:
//   1. resolve() lambda callers for ShapeGate/LaneBlock/ComboGate/SplitPath
//      must pass pos.y (float) instead of pos (Position struct).
//   2. LanePush player_in_timing_window() call must pass pos.y.
//   Once those call sites are updated, the build will stabilise and these
//   tests can be activated.
//
// See .squad/decisions/inbox/baer-model-authority-tests.md BLOCKER-5.
// ════════════════════════════════════════════════════════════════════════════

#if 1  // COLLISION_SCROLLZ_WIP — collision_system.cpp call sites fixed

TEST_CASE("bridge: collision_system resolves LowBar via ObstacleScrollZ (jump clears)",
          "[model_authority][collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Jumping;  // correct action for LowBar

    // Simulated migrated LowBar: ObstacleScrollZ at player Y, no Position.
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Obstacle>(obs, ObstacleKind::LowBar,
                          static_cast<int16_t>(constants::PTS_LOW_BAR));
    reg.emplace<RequiredVAction>(obs, VMode::Jumping);
    reg.emplace<ObstacleScrollZ>(obs, ObstacleScrollZ{constants::PLAYER_Y});
    // No Position — bridge-state entity

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
}

TEST_CASE("bridge: collision_system resolves LowBar via ObstacleScrollZ (grounded misses)",
          "[model_authority][collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Grounded;  // wrong action → miss

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Obstacle>(obs, ObstacleKind::LowBar,
                          static_cast<int16_t>(constants::PTS_LOW_BAR));
    reg.emplace<RequiredVAction>(obs, VMode::Jumping);
    reg.emplace<ObstacleScrollZ>(obs, ObstacleScrollZ{constants::PLAYER_Y});

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<MissTag>(obs));
    CHECK(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("bridge: collision_system resolves HighBar via ObstacleScrollZ (slide clears)",
          "[model_authority][collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Sliding;

    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Obstacle>(obs, ObstacleKind::HighBar,
                          static_cast<int16_t>(constants::PTS_HIGH_BAR));
    reg.emplace<RequiredVAction>(obs, VMode::Sliding);
    reg.emplace<ObstacleScrollZ>(obs, ObstacleScrollZ{constants::PLAYER_Y});

    collision_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
}

TEST_CASE("bridge: collision_system ignores LowBar+ObstacleScrollZ outside timing window",
          "[model_authority][collision]") {
    auto reg = make_registry();
    auto p = make_player(reg);
    reg.get<VerticalState>(p).mode = VMode::Jumping;

    // Obstacle far above the player — outside collision window.
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Obstacle>(obs, ObstacleKind::LowBar,
                          static_cast<int16_t>(constants::PTS_LOW_BAR));
    reg.emplace<RequiredVAction>(obs, VMode::Jumping);
    reg.emplace<ObstacleScrollZ>(obs, ObstacleScrollZ{constants::PLAYER_Y - 200.0f});

    collision_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
}

#endif  // COLLISION_SCROLLZ_WIP
