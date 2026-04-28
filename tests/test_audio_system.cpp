#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "audio/audio_queue.h"
#include "audio/sfx_bank.h"
#include "test_helpers.h"

// ── SFX contiguity guard ─────────────────────────────────────────────────────
// All real SFX values in declaration order. If a new SFX value is added or
// removed, the static_assert below will fail, forcing an update here.
namespace {
constexpr SFX kAllSfx[] = {
    SFX::ShapeShift, SFX::Crash,    SFX::UITap,
    SFX::ChainBonus, SFX::ZoneRisky,   SFX::ZoneDanger, SFX::ZoneDead,
    SFX::ScorePopup, SFX::GameStart,
};
constexpr int kSfxCount = static_cast<int>(sizeof(kAllSfx) / sizeof(kAllSfx[0]));

static_assert(static_cast<int>(SFX::ShapeShift) == 0,
              "SFX must be zero-based (ShapeShift must equal 0)");
static_assert(static_cast<int>(magic_enum::enum_count<SFX>()) == kSfxCount,
              "SFX enum count mismatch — update kAllSfx when adding/removing SFX values");
}  // namespace

namespace {

struct BackendRecorder {
    std::vector<SFX> calls;
};

void record_sfx(entt::registry& reg, SFX sfx) {
    auto* recorder = reg.ctx().find<BackendRecorder>();
    if (recorder) recorder->calls.push_back(sfx);
}

SFXPlaybackBackend backend_for() {
    return SFXPlaybackBackend{record_sfx};
}

}  // namespace

TEST_CASE("audio_system: empty queue does not call backend", "[audio]") {
    auto reg = make_registry();
    reg.ctx().emplace<BackendRecorder>();
    reg.ctx().emplace<SFXPlaybackBackend>(backend_for());

    audio_system(reg);

    CHECK(reg.ctx().get<BackendRecorder>().calls.empty());
    CHECK(reg.ctx().get<AudioQueue>().count == 0);
}

TEST_CASE("audio_system: dispatches queued SFX to backend in order", "[audio]") {
    auto reg = make_registry();
    reg.ctx().emplace<BackendRecorder>();
    reg.ctx().emplace<SFXPlaybackBackend>(backend_for());

    auto& queue = reg.ctx().get<AudioQueue>();
    audio_push(queue, SFX::ShapeShift);
    audio_push(queue, SFX::Crash);
    audio_push(queue, SFX::UITap);

    audio_system(reg);

    auto& recorder = reg.ctx().get<BackendRecorder>();
    REQUIRE(recorder.calls.size() == 3);
    CHECK(recorder.calls[0] == SFX::ShapeShift);
    CHECK(recorder.calls[1] == SFX::Crash);
    CHECK(recorder.calls[2] == SFX::UITap);
    CHECK(queue.count == 0);
}

TEST_CASE("audio_system: dispatches every queued SFX up to queue capacity", "[audio]") {
    auto reg = make_registry();
    reg.ctx().emplace<BackendRecorder>();
    reg.ctx().emplace<SFXPlaybackBackend>(backend_for());

    auto& queue = reg.ctx().get<AudioQueue>();
    for (int i = 0; i < AudioQueue::MAX_QUEUED; ++i) {
        audio_push(queue, kAllSfx[i % kSfxCount]);
    }
    audio_push(queue, SFX::Crash);

    audio_system(reg);

    CHECK(reg.ctx().get<BackendRecorder>().calls.size() == static_cast<std::size_t>(AudioQueue::MAX_QUEUED));
    CHECK(queue.count == 0);
}

TEST_CASE("audio_system: clears queue without backend in headless mode", "[audio]") {
    auto reg = make_registry();

    auto& queue = reg.ctx().get<AudioQueue>();
    audio_push(queue, SFX::ShapeShift);
    audio_push(queue, SFX::Crash);

    audio_system(reg);

    CHECK(queue.count == 0);
}

TEST_CASE("sfx lifecycle: production backend is callable without audio hardware", "[audio]") {
    auto reg = make_registry();

    sfx_bank_init(reg);
    sfx_playback_backend_init(reg);

    auto* bank = reg.ctx().find<SFXBank>();
    auto* backend = reg.ctx().find<SFXPlaybackBackend>();
    REQUIRE(bank != nullptr);
    REQUIRE(backend != nullptr);
    REQUIRE(backend->dispatch != nullptr);

    auto& queue = reg.ctx().get<AudioQueue>();
    audio_push(queue, SFX::GameStart);
    audio_system(reg);

    CHECK(queue.count == 0);

    sfx_bank_unload(reg);
    CHECK_FALSE(bank->loaded);
}

TEST_CASE("sfx lifecycle: injected playback backend is preserved", "[audio]") {
    auto reg = make_registry();
    reg.ctx().emplace<BackendRecorder>();
    reg.ctx().emplace<SFXPlaybackBackend>(backend_for());

    sfx_playback_backend_init(reg);

    auto& queue = reg.ctx().get<AudioQueue>();
    audio_push(queue, SFX::Crash);
    audio_system(reg);

    auto& recorder = reg.ctx().get<BackendRecorder>();
    REQUIRE(recorder.calls.size() == 1);
    CHECK(recorder.calls[0] == SFX::Crash);
}
