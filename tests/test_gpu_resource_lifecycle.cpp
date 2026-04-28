// Tests for GPU resource RAII wrappers (ShapeMeshes, RenderTargets).
//
// Runtime lifecycle tests (move, release, double-release) would require an
// active OpenGL context because UnloadRenderTexture/UnloadMesh make GL calls.
// Without InitWindow(), those functions are no-ops or crash -- so we validate
// ownership semantics at the type-trait level here and rely on the zero-warning
// build to catch any regression in the struct definitions.
//
// The destructor / release() path is exercised indirectly by every full
// game_loop_init -> game_loop_shutdown integration run.

#include <catch2/catch_test_macros.hpp>
#include <type_traits>

#include "rendering/camera_resources.h"
#include "entities/camera_entity.h"  // GameCamera, UICamera

// ── ShapeMeshes type traits ──────────────────────────────────────────────────

static_assert(!std::is_copy_constructible_v<camera::ShapeMeshes>,
    "ShapeMeshes must not be copy-constructible: copying GPU handles would "
    "cause double-unload on destruction.");

static_assert(!std::is_copy_assignable_v<camera::ShapeMeshes>,
    "ShapeMeshes must not be copy-assignable.");

static_assert(std::is_move_constructible_v<camera::ShapeMeshes>,
    "ShapeMeshes must be move-constructible for registry ctx emplace.");

static_assert(std::is_move_assignable_v<camera::ShapeMeshes>,
    "ShapeMeshes must be move-assignable.");

// ── RenderTargets type traits ────────────────────────────────────────────────

static_assert(!std::is_copy_constructible_v<RenderTargets>,
    "RenderTargets must not be copy-constructible: copying GPU handles would "
    "cause double-unload on destruction.");

static_assert(!std::is_copy_assignable_v<RenderTargets>,
    "RenderTargets must not be copy-assignable.");

static_assert(std::is_move_constructible_v<RenderTargets>,
    "RenderTargets must be move-constructible for registry ctx emplace.");

static_assert(std::is_move_assignable_v<RenderTargets>,
    "RenderTargets must be move-assignable.");

// ── Default state: owned = false, so destructor is a no-op ──────────────────

TEST_CASE("ShapeMeshes: default-constructed is not owned", "[gpu_resource_lifecycle]") {
    camera::ShapeMeshes sm{};
    CHECK_FALSE(sm.owned);
    // Destructor fires here; must not call any GPU unload (owned == false).
}

TEST_CASE("RenderTargets: default-constructed is not owned", "[gpu_resource_lifecycle]") {
    RenderTargets rt{};
    CHECK_FALSE(rt.owned);
    // Destructor fires here; must not call any GPU unload (owned == false).
}

TEST_CASE("ShapeMeshes: move transfers ownership", "[gpu_resource_lifecycle]") {
    camera::ShapeMeshes sm{};
    sm.owned = true;  // simulate a live-resource state without a real GPU
    camera::ShapeMeshes sm2{std::move(sm)};
    CHECK_FALSE(sm.owned);   // moved-from: no longer owner
    CHECK(sm2.owned);        // moved-to: now the owner
    // Manually clear so the destructor does not call GPU unload
    // (we never had a real GPU context).
    sm2.owned = false;
}

TEST_CASE("RenderTargets: move transfers ownership", "[gpu_resource_lifecycle]") {
    RenderTargets rt{};
    rt.owned = true;  // simulate a live-resource state without a real GPU
    RenderTargets rt2{std::move(rt)};
    CHECK_FALSE(rt.owned);
    CHECK(rt2.owned);
    rt2.owned = false;
}

TEST_CASE("ShapeMeshes: release is idempotent when not owned", "[gpu_resource_lifecycle]") {
    camera::ShapeMeshes sm{};
    sm.release();  // must be no-op (owned == false), not crash
    sm.release();  // second call also safe
    SUCCEED("double release on unowned ShapeMeshes does not crash");
}

TEST_CASE("RenderTargets: release is idempotent when not owned", "[gpu_resource_lifecycle]") {
    RenderTargets rt{};
    rt.release();
    rt.release();
    SUCCEED("double release on unowned RenderTargets does not crash");
}
