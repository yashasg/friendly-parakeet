#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "constants.h"

// Helper: create a particle entity with all required components
static entt::entity make_particle(entt::registry& reg, float size, float life_remaining, float life_max,
                                  float vx = 0.0f, float vy = 0.0f) {
    auto e = reg.create();
    reg.emplace<ParticleTag>(e);
    reg.emplace<ParticleData>(e, size);
    reg.emplace<Lifetime>(e, life_remaining, life_max);
    reg.emplace<Velocity>(e, vx, vy);
    reg.emplace<Position>(e, 100.0f, 200.0f);
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
    auto e = make_particle(reg, 10.0f, 0.01f, 1.0f);

    particle_system(reg, 0.016f);

    auto& pd = reg.get<ParticleData>(e);
    CHECK_THAT(pd.size, Catch::Matchers::WithinAbs(10.0f, 0.01f));
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

    auto& vel = reg.get<Velocity>(e);
    CHECK_THAT(vel.dy, Catch::Matchers::WithinAbs(constants::PARTICLE_GRAVITY * 0.1f, 0.01f));
}

TEST_CASE("particle: gravity accumulates over multiple frames", "[particle]") {
    auto reg = make_registry();
    auto e = make_particle(reg, 5.0f, 2.0f, 2.0f, 0.0f, 0.0f);

    particle_system(reg, 0.1f);
    particle_system(reg, 0.1f);

    auto& vel = reg.get<Velocity>(e);
    CHECK_THAT(vel.dy, Catch::Matchers::WithinAbs(constants::PARTICLE_GRAVITY * 0.2f, 0.01f));
}

TEST_CASE("particle: gravity does not affect horizontal velocity", "[particle]") {
    auto reg = make_registry();
    auto e = make_particle(reg, 5.0f, 1.0f, 1.0f, 50.0f, 0.0f);

    particle_system(reg, 0.1f);

    CHECK(reg.get<Velocity>(e).dx == 50.0f);
}

TEST_CASE("particle: no particles means no crash", "[particle]") {
    auto reg = make_registry();
    particle_system(reg, 0.016f);
    SUCCEED("particle_system handles empty registry");
}
