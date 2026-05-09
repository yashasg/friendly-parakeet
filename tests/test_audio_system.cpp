#include <catch2/catch_test_macros.hpp>

#include <raylib.h>

#include "audio/audio_types.h"
#include "audio/sfx_bank.h"
#include "test_helpers.h"

TEST_CASE("audio_system: drains queued SFX safely without playable sounds", "[audio]") {
    auto reg = make_registry();
    auto& disp = reg.ctx().get<entt::dispatcher>();
    auto& bank = reg.ctx().emplace<SFXBank>(SFXBank{});
    bank.loaded = true;

    // Enqueue sounds the same way production code does.
    disp.enqueue<PlaySfxEvent>({SFX::Crash});            // unloaded sound
    disp.enqueue<PlaySfxEvent>({static_cast<SFX>(255)}); // invalid enum payload
    for (int i = 0; i < SFXBank::SFX_COUNT; ++i) {
        disp.enqueue<PlaySfxEvent>({static_cast<SFX>(i % SFXBank::SFX_COUNT)});
    }

    // audio_system must drain without crashing.
    audio_system(reg);

    // After drain, no events remain (a second drain captures nothing).
    auto cap = drain_sfx_events(reg);
    CHECK(cap.count == 0);
}

TEST_CASE("sfx lifecycle: bank init and playback are headless-safe", "[audio]") {
    auto reg = make_registry();

    sfx_bank_init(reg);

    auto* bank = reg.ctx().find<SFXBank>();
    REQUIRE(bank != nullptr);
    if (!IsAudioDeviceReady()) {
        CHECK_FALSE(bank->loaded);
    }

    reg.ctx().get<entt::dispatcher>().enqueue<PlaySfxEvent>({SFX::GameStart});
    audio_system(reg);

    // After drain, queue is empty.
    auto cap = drain_sfx_events(reg);
    CHECK(cap.count == 0);

    sfx_bank_unload(reg);
    CHECK_FALSE(bank->loaded);
}
