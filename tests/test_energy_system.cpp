#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "components/gameplay_intents.h"

// ── energy_system ────────────────────────────────────────────

TEST_CASE("energy: no action when not in Playing phase", "[energy]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.0f;

    energy_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("energy: no action when energy is positive", "[energy]") {
    auto reg = make_rhythm_registry();
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.5f;

    energy_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
    CHECK(reg.ctx().get<SongState>().playing);
}

TEST_CASE("energy: pending miss drain is applied by energy_system", "[energy]") {
    auto reg = make_rhythm_registry();
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.8f;
    auto& pending = reg.ctx().emplace<PendingEnergyEffects>();
    pending.delta = -constants::ENERGY_DRAIN_MISS;
    pending.flash = true;

    energy_system(reg, 0.016f);

    CHECK_THAT(energy.energy, Catch::Matchers::WithinAbs(0.8f - constants::ENERGY_DRAIN_MISS, 0.0001f));
    CHECK(energy.flash_timer > 0.0f);
    CHECK(energy.flash_timer < constants::ENERGY_FLASH_DURATION);
    CHECK(pending.delta == 0.0f);
    CHECK_FALSE(pending.flash);
}

TEST_CASE("energy: pending perfect recovery is applied by energy_system", "[energy]") {
    auto reg = make_rhythm_registry();
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.2f;
    auto& pending = reg.ctx().emplace<PendingEnergyEffects>();
    pending.delta = constants::ENERGY_RECOVER_PERFECT;

    energy_system(reg, 0.016f);

    CHECK_THAT(energy.energy,
               Catch::Matchers::WithinAbs(0.2f + constants::ENERGY_RECOVER_PERFECT, 0.0001f));
    CHECK(pending.delta == 0.0f);
}

TEST_CASE("energy: pending events clamp each step in order", "[energy]") {
    auto reg = make_rhythm_registry();
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.0f;
    auto& pending = reg.ctx().emplace<PendingEnergyEffects>();
    pending.events.push_back({-constants::ENERGY_DRAIN_MISS, true});
    pending.events.push_back({constants::ENERGY_RECOVER_PERFECT, false});

    energy_system(reg, 0.016f);

    CHECK_THAT(energy.energy, Catch::Matchers::WithinAbs(constants::ENERGY_RECOVER_PERFECT, 0.0001f));
    CHECK(energy.flash_timer > 0.0f);
    CHECK(pending.events.empty());
}

TEST_CASE("energy: game_state triggers game over when energy reaches zero", "[energy]") {
    auto reg = make_rhythm_registry();
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.0f;

    game_state_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::GameOver);
    CHECK(reg.ctx().get<GameOverState>().cause == DeathCause::EnergyDepleted);
}

TEST_CASE("energy: no action when SongState not present", "[energy]") {
    entt::registry bare_reg;
    bare_reg.ctx().emplace<InputState>();
    
    bare_reg.ctx().emplace<GameState>(GameState{
        GamePhase::Playing, GamePhase::Playing, 0.0f, false, GamePhase::Playing, 0.0f
    });
    bare_reg.ctx().emplace<EnergyState>(EnergyState{0.0f, 0.0f, 0.0f});

    energy_system(bare_reg, 0.016f);

    auto& gs = bare_reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("energy: no action when song not playing", "[energy]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<SongState>().playing = false;
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.0f;

    energy_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("energy: no action when EnergyState not present", "[energy]") {
    entt::registry bare_reg;
    bare_reg.ctx().emplace<InputState>();
    
    bare_reg.ctx().emplace<GameState>(GameState{
        GamePhase::Playing, GamePhase::Playing, 0.0f, false, GamePhase::Playing, 0.0f
    });
    auto& song = bare_reg.ctx().emplace<SongState>();
    song.playing = true;

    energy_system(bare_reg, 0.016f);

    auto& gs = bare_reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("energy: small positive energy does not trigger game over", "[energy]") {
    auto reg = make_rhythm_registry();
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.01f;

    game_state_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
    CHECK(reg.ctx().get<SongState>().playing);
}

TEST_CASE("energy: display smooths toward energy value", "[energy]") {
    auto reg = make_rhythm_registry();
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.5f;
    energy.display = 1.0f;

    energy_system(reg, 0.016f);

    // display should have moved toward 0.5 from 1.0
    CHECK(energy.display < 1.0f);
    CHECK(energy.display > 0.5f);
}

TEST_CASE("energy: flash timer decrements", "[energy]") {
    auto reg = make_rhythm_registry();
    auto& energy = reg.ctx().get<EnergyState>();
    energy.flash_timer = 0.2f;

    energy_system(reg, 0.016f);

    CHECK(energy.flash_timer < 0.2f);
    CHECK(energy.flash_timer > 0.0f);
}

TEST_CASE("energy: enter_game_over owns song stop", "[energy][gamestate]") {
    auto reg = make_rhythm_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    auto& song = reg.ctx().get<SongState>();
    CHECK(song.finished);
    CHECK_FALSE(song.playing);
}

TEST_CASE("energy: flash timer clamps to zero", "[energy]") {
    auto reg = make_rhythm_registry();
    auto& energy = reg.ctx().get<EnergyState>();
    energy.flash_timer = 0.005f;

    energy_system(reg, 0.016f);

    CHECK(energy.flash_timer == 0.0f);
}
