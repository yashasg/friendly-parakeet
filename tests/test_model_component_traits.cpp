// Model component type-trait and lifecycle pattern tests.
//
// These tests compile and pass TODAY (no OpenGL context, no GPU functions).
// They lock in structural invariants that the Model/Transform migration slice
// depends on. See also: test_obstacle_model_slice.cpp for obstacle-specific
// future-state specs (guarded with #if 0 until Keaton's slices land).
//
// Coverage:
//   1. Model is trivially copyable/standard-layout → EnTT can store it safely
//      as a value component without special-casing.
//   2. Model{} zero-initialises all pointer fields → the planned on_destroy
//      null guard (if m.meshes) is well-defined on default-constructed values.
//   3. on_destroy<Model> fires before the entity is invalidated and before the
//      component is removed → the listener body can safely call r.get<Model>(e).
//   4. on_destroy<Model> null-mesh guard: if meshes == nullptr, the UnloadModel
//      branch is skipped (no GPU call). Validates the headless-safe path.
//   5. on_destroy<Model> non-null-mesh guard: if meshes != nullptr, the
//      UnloadModel branch IS taken. Validates ownership detection logic.

#include <catch2/catch_test_macros.hpp>
#include <type_traits>
#include <raylib.h>
#include <entt/entt.hpp>

// ── 1. Type-trait static assertions ──────────────────────────────────────────

// Model is a plain C struct — EnTT stores components by value and may relocate
// them; trivial copyability is required for safe internal move.
static_assert(std::is_trivially_copyable_v<Model>,
    "Model must be trivially copyable: EnTT may relocate component storage "
    "by bitwise copy. A non-trivial Model would corrupt moved state.");

static_assert(std::is_standard_layout_v<Model>,
    "Model must be standard layout: required for predictable C-struct field "
    "access and C-interop with raylib GPU functions.");

// ── 2. Zero-initialisation guarantees ────────────────────────────────────────

TEST_CASE("Model: zero-initialised has null pointers and zero counts",
          "[model_traits]") {
    Model m{};
    // The planned on_destroy<Model> guard (if m.meshes) depends on these:
    CHECK(m.meshes       == nullptr);
    CHECK(m.materials    == nullptr);
    CHECK(m.meshMaterial == nullptr);
    CHECK(m.meshCount    == 0);
    CHECK(m.materialCount == 0);
    // Bonus: transform zero-init (all zeros — NOT identity, but well-defined)
    // The archetype is responsible for setting model.transform at spawn.
    CHECK(m.boneCount    == 0);
    CHECK(m.bones        == nullptr);
}

// ── 3 + 4 + 5. on_destroy<Model> listener pattern ────────────────────────────
//
// The contract (keyser-model-contract-amendment.md §2) specifies:
//
//   reg.on_destroy<Model>().connect<listener>();
//   // Inside listener:
//   auto& m = r.get<Model>(e);
//   if (m.meshes) UnloadModel(m);
//
// We test this using a mock listener that records observations without making
// GPU calls, keeping all tests headless-safe.

struct MockModelDestroyListener {
    bool was_called            = false;
    bool mesh_ptr_was_nonnull  = false;
    bool component_readable    = false;  // can we r.get<Model>(e) in the listener?

    void on_destroy(entt::registry& r, entt::entity e) {
        was_called = true;
        auto& m = r.get<Model>(e);   // must not throw/crash before removal
        component_readable = true;
        if (m.meshes) {
            mesh_ptr_was_nonnull = true;
            // Production code calls UnloadModel(m) here.
            // We skip that call to stay headless-safe.
        }
    }
};

TEST_CASE("on_destroy<Model>: listener fires before entity is removed",
          "[model_lifecycle]") {
    entt::registry reg;
    MockModelDestroyListener listener;
    reg.on_destroy<Model>().connect<&MockModelDestroyListener::on_destroy>(listener);

    auto entity = reg.create();
    reg.emplace<Model>(entity, Model{});
    REQUIRE(reg.all_of<Model>(entity));

    reg.destroy(entity);

    CHECK(listener.was_called);
    CHECK(listener.component_readable);  // r.get<Model>(e) succeeded inside listener
    CHECK_FALSE(reg.valid(entity));
}

TEST_CASE("on_destroy<Model>: null-mesh guard skips UnloadModel branch",
          "[model_lifecycle]") {
    entt::registry reg;
    MockModelDestroyListener listener;
    reg.on_destroy<Model>().connect<&MockModelDestroyListener::on_destroy>(listener);

    auto entity = reg.create();
    reg.emplace<Model>(entity, Model{});  // Model{}: meshes == nullptr

    reg.destroy(entity);

    CHECK(listener.was_called);
    CHECK_FALSE(listener.mesh_ptr_was_nonnull);  // null path: UnloadModel NOT called
}

TEST_CASE("on_destroy<Model>: non-null mesh pointer triggers UnloadModel branch",
          "[model_lifecycle]") {
    entt::registry reg;
    MockModelDestroyListener listener;
    reg.on_destroy<Model>().connect<&MockModelDestroyListener::on_destroy>(listener);

    auto entity = reg.create();
    Model m{};
    // Simulate an entity that "owns" meshes by pointing to a stack sentinel.
    // We do NOT call UploadMesh (no GPU context), and our mock listener does NOT
    // call UnloadModel. This only validates that the if-branch is reachable.
    Mesh sentinel_mesh{};
    m.meshes    = &sentinel_mesh;  // non-null: signals ownership
    m.meshCount = 1;
    reg.emplace<Model>(entity, m);

    reg.destroy(entity);

    CHECK(listener.was_called);
    CHECK(listener.mesh_ptr_was_nonnull);  // non-null path: UnloadModel WOULD fire
}

TEST_CASE("on_destroy<Model>: listener fires once per entity destruction",
          "[model_lifecycle]") {
    entt::registry reg;
    int call_count = 0;
    struct CountListener {
        int& count;
        void on_destroy(entt::registry& r, entt::entity e) {
            (void)r; (void)e;
            ++count;
        }
    };
    CountListener cl{call_count};
    reg.on_destroy<Model>().connect<&CountListener::on_destroy>(cl);

    auto e1 = reg.create();
    auto e2 = reg.create();
    reg.emplace<Model>(e1, Model{});
    reg.emplace<Model>(e2, Model{});

    reg.destroy(e1);
    CHECK(call_count == 1);
    reg.destroy(e2);
    CHECK(call_count == 2);
}
