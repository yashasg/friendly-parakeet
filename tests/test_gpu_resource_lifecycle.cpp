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
#include <cstdio>
#include <entt/entt.hpp>
#include <type_traits>

#include "components/audio.h"
#include "components/loaded_sfx.h"
#include "systems/audio_resources.h"
#include "components/camera_resources.h"
#include "systems/camera_resources.h"
#include "entities/camera_entity.h"  // GameCamera, UICamera
#include "systems/text_resources.h"
#include "systems/session_logger_system.h"

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

// ── Runtime context type traits ───────────────────────────────────────────────

static_assert(!std::is_copy_constructible_v<TextContext>,
    "TextContext must not be copy-constructible: copying Font handles would "
    "cause double-unload on destruction.");

static_assert(!std::is_copy_assignable_v<TextContext>,
    "TextContext must not be copy-assignable.");

static_assert(std::is_move_constructible_v<TextContext>,
    "TextContext must be move-constructible for registry ctx emplace.");

static_assert(std::is_move_assignable_v<TextContext>,
    "TextContext must be move-assignable.");

static_assert(!std::is_copy_constructible_v<LoadedSfx>,
    "LoadedSfx must not be copy-constructible: copying Sound handles would "
    "cause double-unload on destruction.");

static_assert(!std::is_copy_assignable_v<LoadedSfx>,
    "LoadedSfx must not be copy-assignable.");

static_assert(std::is_move_constructible_v<LoadedSfx>,
    "LoadedSfx must be move-constructible for registry storage.");

static_assert(std::is_move_assignable_v<LoadedSfx>,
    "LoadedSfx must be move-assignable.");

static_assert(!std::is_copy_constructible_v<MusicContext>,
    "MusicContext must not be copy-constructible: copying Music handles would "
    "cause double-unload on destruction.");

static_assert(!std::is_copy_assignable_v<MusicContext>,
    "MusicContext must not be copy-assignable.");

static_assert(std::is_move_constructible_v<MusicContext>,
    "MusicContext must be move-constructible for registry ctx emplace.");

static_assert(std::is_move_assignable_v<MusicContext>,
    "MusicContext must be move-assignable.");

static_assert(!std::is_copy_constructible_v<SessionLog>,
    "SessionLog must not be copy-constructible: copying FILE* would "
    "cause double-close on destruction.");

static_assert(!std::is_copy_assignable_v<SessionLog>,
    "SessionLog must not be copy-assignable.");

static_assert(std::is_move_constructible_v<SessionLog>,
    "SessionLog must be move-constructible for registry ctx emplace.");

static_assert(std::is_move_assignable_v<SessionLog>,
    "SessionLog must be move-assignable.");

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
    CHECK_FALSE(sm.owned);
    sm.release();  // second call also safe
    CHECK_FALSE(sm.owned);
}

TEST_CASE("RenderTargets: release is idempotent when not owned", "[gpu_resource_lifecycle]") {
    RenderTargets rt{};
    rt.release();
    CHECK_FALSE(rt.owned);
    rt.release();
    CHECK_FALSE(rt.owned);
}

TEST_CASE("runtime contexts: registry erase releases default resources safely",
          "[resource_lifecycle][issue648]") {
    entt::registry reg;

    reg.ctx().emplace<TextContext>();
    reg.ctx().emplace<MusicContext>();
    reg.ctx().emplace<SessionLog>();

    // Sanity: emplace actually installed the contexts before we erase them
    // (otherwise the erase-then-contains check below would be vacuously true).
    REQUIRE(reg.ctx().contains<TextContext>());
    REQUIRE(reg.ctx().contains<MusicContext>());
    REQUIRE(reg.ctx().contains<SessionLog>());

    reg.ctx().erase<TextContext>();
    reg.ctx().erase<MusicContext>();
    reg.ctx().erase<SessionLog>();

    CHECK_FALSE(reg.ctx().contains<TextContext>());
    CHECK_FALSE(reg.ctx().contains<MusicContext>());
    CHECK_FALSE(reg.ctx().contains<SessionLog>());
}

TEST_CASE("runtime contexts: explicit release is idempotent without live handles",
          "[resource_lifecycle][issue648]") {
    TextContext text;
    MusicContext music;
    SessionLog log;

    music.loaded = true;
    music.started = true;
    music.paused = true;

    text.release();
    text.release();
    music.release();
    music.release();
    log.release();
    log.release();

    CHECK_FALSE(music.loaded);
    CHECK_FALSE(music.started);
    CHECK_FALSE(music.paused);
    CHECK(log.file == nullptr);
}

// Replaces the former `SFXBank::release()` idempotence test (#1616).
// `LoadedSfx` is the row-table normalization of the eradicated array
// column; default-constructed `Sound{}` fails `IsSoundValid`, so the
// destructor is a no-op without an audio device. Move semantics transfer
// the (zero-init) handle and zero out the source.
TEST_CASE("LoadedSfx: default-constructed destruct + move are headless-safe",
          "[resource_lifecycle][audio]") {
    {
        LoadedSfx sfx{};
        CHECK_FALSE(IsSoundValid(sfx.sound));
        // Destructor fires at end of scope; must not call UnloadSound.
    }

    LoadedSfx src{};
    src.key = SFX::ShapeShift;
    LoadedSfx dst{std::move(src)};
    CHECK(dst.key == SFX::ShapeShift);
    CHECK_FALSE(IsSoundValid(src.sound));
    CHECK_FALSE(IsSoundValid(dst.sound));
}

TEST_CASE("SessionLog: move transfers file ownership",
          "[resource_lifecycle][session_log][issue648]") {
    SessionLog first;
#ifdef _WIN32
    first.file = std::fopen("NUL", "w");
#else
    first.file = std::fopen("/dev/null", "w");
#endif
    REQUIRE(first.file != nullptr);
    first.buffer.append("pending\n");

    SessionLog second{std::move(first)};

    CHECK(first.file == nullptr);
    CHECK(second.file != nullptr);
    second.release();
    CHECK(second.file == nullptr);
}
