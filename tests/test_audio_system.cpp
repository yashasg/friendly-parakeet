#include <catch2/catch_test_macros.hpp>

#include <raylib.h>

#include "components/audio.h"
#include "components/loaded_sfx.h"
#include "systems/sfx_bank.h"
#include "test_helpers.h"

TEST_CASE("audio_system: drains queued SFX safely without playable sounds", "[audio]") {
    auto reg = make_registry();
    auto& disp = reg.ctx().get<entt::dispatcher>();

    // No `LoadedSfx` rows exist: every emit must miss the view scan
    // safely. This replaces the former `SFXBank{loaded=true}` fake
    // (#1616 — array-column eradication).
    constexpr int sfx_count = static_cast<int>(SFX::Count);

    // Enqueue sounds the same way production code does.
    disp.enqueue<PlaySfxEvent>({SFX::Crash});            // unloaded sound
    disp.enqueue<PlaySfxEvent>({static_cast<SFX>(255)}); // invalid enum payload
    for (int i = 0; i < sfx_count; ++i) {
        disp.enqueue<PlaySfxEvent>({static_cast<SFX>(i % sfx_count)});
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

    // Without a real audio device, `sfx_bank_init` short-circuits before
    // creating any `LoadedSfx` rows (the row is the moral equivalent of
    // the former `SFXBank::loaded` flag — presence ⇔ loaded, #1616).
    if (!IsAudioDeviceReady()) {
        CHECK(reg.view<LoadedSfx>().empty());
    }

    reg.ctx().get<entt::dispatcher>().enqueue<PlaySfxEvent>({SFX::GameStart});
    audio_system(reg);

    // After drain, queue is empty.
    auto cap = drain_sfx_events(reg);
    CHECK(cap.count == 0);

    sfx_bank_unload(reg);
    CHECK(reg.view<LoadedSfx>().empty());
}
