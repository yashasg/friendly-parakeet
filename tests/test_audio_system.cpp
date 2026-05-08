#include <catch2/catch_test_macros.hpp>

#include <raylib.h>

#include "audio/audio_types.h"
#include "audio/sfx_bank.h"
#include "test_helpers.h"

TEST_CASE("audio_system: drains queued SFX safely without playable sounds", "[audio]") {
    auto reg = make_registry();
    auto& queue = reg.ctx().get<AudioQueue>();
    auto& bank = reg.ctx().emplace<SFXBank>(SFXBank{});
    bank.loaded = true;

    queue.queue[queue.count++] = SFX::Crash;            // unloaded sound
    queue.queue[queue.count++] = static_cast<SFX>(255); // invalid enum payload
    for (int i = queue.count; i < AudioQueue::MAX_QUEUED; ++i) {
        queue.queue[queue.count++] = static_cast<SFX>(i % SFXBank::SFX_COUNT);
    }

    audio_system(reg);

    CHECK(queue.count == 0);
}

TEST_CASE("sfx lifecycle: bank init and playback are headless-safe", "[audio]") {
    auto reg = make_registry();

    sfx_bank_init(reg);

    auto* bank = reg.ctx().find<SFXBank>();
    REQUIRE(bank != nullptr);
    if (!IsAudioDeviceReady()) {
        CHECK_FALSE(bank->loaded);
    }

    auto& queue = reg.ctx().get<AudioQueue>();
    queue.queue[queue.count++] = SFX::GameStart;
    audio_system(reg);

    CHECK(queue.count == 0);

    sfx_bank_unload(reg);
    CHECK_FALSE(bank->loaded);
}
