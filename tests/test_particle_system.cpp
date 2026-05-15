#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "systems/particle_system.h"
#include "test_helpers.h"
#include "constants.h"

// Helper: create a particle entity with all required components
static entt::entity make_particle(entt::registry& reg, float size, float life_remaining, float life_max,
                                  float vx = 0.0f, float vy = 0.0f) {
    auto e = reg.create();
    reg.emplace<ParticleTag>(e);
    reg.emplace<ParticleData>(e, size, life_remaining, life_max);
    reg.emplace<Vector2>(e, Vector2{vx, vy});
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<TagEffectsPass>(e);
    return e;
}

// ── particle_system: shrinking ──────────────────────────────

TEST_CASE("particle: shrinks proportional to remaining lifetime", "[particle]") {
    auto reg = make_registry();
    auto e = make_particle(reg, 10.0f, 0.5f, 1.0f);

    // Size is now immutable; shrinking is computed at render time.
    // Verify the particle data is unchanged after a system tick.
    particle_system(reg, 0.016f);

    auto& pd = reg.get<ParticleData>(e);
    CHECK_THAT(pd.size, Catch::Matchers::WithinAbs(10.0f, 0.01f));
}

TEST_CASE("particle: full lifetime means full size", "[particle]") {
    auto reg = make_registry();
    auto e = make_particle(reg, 8.0f, 1.0f, 1.0f);

    particle_system(reg, 0.016f);

    auto& pd = reg.get<ParticleData>(e);
    CHECK_THAT(pd.size, Catch::Matchers::WithinAbs(8.0f, 0.01f));
}

TEST_CASE("particle: size is immutable across ticks", "[particle]") {
    auto reg = make_registry();
    auto e = make_particle(reg, 10.0f, 0.5f, 1.0f);

    particle_system(reg, 0.016f);

    auto& pd = reg.get<ParticleData>(e);
    CHECK_THAT(pd.size, Catch::Matchers::WithinAbs(10.0f, 0.01f));
}

TEST_CASE("particle: expired particles are destroyed", "[particle]") {
    auto reg = make_registry();
    auto e = make_particle(reg, 10.0f, 0.01f, 1.0f);

    particle_system(reg, 0.016f);

    CHECK_FALSE(reg.valid(e));
}

TEST_CASE("particle: multiple particles retain original size", "[particle]") {
    auto reg = make_registry();
    auto e1 = make_particle(reg, 10.0f, 0.5f, 1.0f);
    auto e2 = make_particle(reg, 20.0f, 0.25f, 1.0f);

    particle_system(reg, 0.016f);

    CHECK_THAT(reg.get<ParticleData>(e1).size, Catch::Matchers::WithinAbs(10.0f, 0.01f));
    CHECK_THAT(reg.get<ParticleData>(e2).size, Catch::Matchers::WithinAbs(20.0f, 0.01f));
}

// ── particle_system: gravity ────────────────────────────────

TEST_CASE("particle: gravity increases downward velocity", "[particle]") {
    auto reg = make_registry();
    auto e = make_particle(reg, 5.0f, 1.0f, 1.0f, 0.0f, 0.0f);

    particle_system(reg, 0.1f);

    auto& vel = reg.get<Vector2>(e);
    CHECK_THAT(vel.y, Catch::Matchers::WithinAbs(constants::PARTICLE_GRAVITY * 0.1f, 0.01f));
}

TEST_CASE("particle: gravity accumulates over multiple frames", "[particle]") {
    auto reg = make_registry();
    auto e = make_particle(reg, 5.0f, 2.0f, 2.0f, 0.0f, 0.0f);

    particle_system(reg, 0.1f);
    particle_system(reg, 0.1f);

    auto& vel = reg.get<Vector2>(e);
    CHECK_THAT(vel.y, Catch::Matchers::WithinAbs(constants::PARTICLE_GRAVITY * 0.2f, 0.01f));
}

TEST_CASE("particle: gravity does not affect horizontal velocity", "[particle]") {
    auto reg = make_registry();
    auto e = make_particle(reg, 5.0f, 1.0f, 1.0f, 50.0f, 0.0f);

    particle_system(reg, 0.1f);

    CHECK(reg.get<Vector2>(e).x == 50.0f);
}

TEST_CASE("particle: no particles means no crash", "[particle]") {
    auto reg = make_registry();
    particle_system(reg, 0.016f);
    SUCCEED("particle_system handles empty registry");
}

// ── #534: reduce_motion suppresses decorative kinetic envelope ──────────────

TEST_CASE("particle: reduce_motion zeroes velocity and disables gravity (#534)",
          "[particle][reduce_motion][issue534]") {
    auto reg = make_registry();
    settings_state(reg).reduce_motion = true;

    auto e = make_particle(reg, 5.0f, 1.0f, 1.0f, 50.0f, -25.0f);

    particle_system(reg, 0.1f);

    // Velocity is clamped to zero — no continuous drift, no gravity
    // accumulation. Particle still ages and despawns on its lifetime
    // (functional state preserved).
    const auto& vel = reg.get<Vector2>(e);
    CHECK(vel.x == 0.0f);
    CHECK(vel.y == 0.0f);

    // Lifetime still ticks down per dt.
    CHECK(reg.get<ParticleData>(e).remaining < 1.0f);
}

TEST_CASE("particle: reduce_motion=false preserves gravity behavior (#534)",
          "[particle][reduce_motion][issue534]") {
    auto reg = make_registry();
    settings_state(reg).reduce_motion = false;

    auto e = make_particle(reg, 5.0f, 1.0f, 1.0f, 0.0f, 0.0f);
    particle_system(reg, 0.1f);
    CHECK_THAT(reg.get<Vector2>(e).y,
               Catch::Matchers::WithinAbs(constants::PARTICLE_GRAVITY * 0.1f, 0.01f));
}

// #1089 — ParticleSystemScratch::capacity_exceeded_count must stay at zero
// when a dense expiry pass fits inside the reserved expired buffer.
TEST_CASE("particle: dense expiry burst stays within reserved capacity",
          "[particle][issue1089]") {
    auto reg = make_registry();
    constexpr int dense_count = 6;
    runtime_system_scratch_reserve(reg, dense_count);

    auto& scratch = reg.ctx().get<ParticleSystemScratch>();
    const auto expired_capacity = scratch.expired.capacity();

    for (int i = 0; i < dense_count; ++i) {
        // Tiny remaining lifetime so the next tick expires every particle.
        make_particle(reg, 5.0f, 0.001f, 1.0f);
    }

    particle_system(reg, 0.016f);

    CHECK(scratch.expired.capacity() == expired_capacity);
    CHECK(scratch.capacity_exceeded_count == 0u);
}
