#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "test_helpers.h"
#include "components/energy_bar.h"
#include "entities/energy_bar_entity.h"

#include <cmath>
#include <stdexcept>

TEST_CASE("energy bar entity: factory creates singleton HUD entity",
          "[energy_bar][entity][issue752]") {
    entt::registry reg;

    const auto entity = create_energy_bar_entity(reg);

    CHECK(reg.valid(entity));
    CHECK(reg.all_of<EnergyBarTag>(entity));
    CHECK(reg.all_of<EnergyBarLayout>(entity));
    CHECK(reg.all_of<EnergyBarVisual>(entity));
    CHECK(reg.all_of<TagHUDPass>(entity));
    CHECK_THROWS_AS(create_energy_bar_entity(reg), std::logic_error);
}

TEST_CASE("energy_bar_system: beat bounce expands visible level when motion is enabled",
          "[energy_bar][reduce_motion][issue478][issue752]") {
    auto reg = make_registry();
    create_energy_bar_entity(reg);
    auto& song = reg.ctx().get<SongState>();
    song.playing = true;
    song.song_time = 0.10f;
    song.beat_period = 0.50f;
    auto& energy = reg.ctx().get<EnergyState>();
    energy.display = 0.50f;

    energy_bar_system(reg, 0.016f);

    const auto entity = *reg.view<EnergyBarTag>().begin();
    const auto& layout = reg.get<EnergyBarLayout>(entity);
    const auto& visual = reg.get<EnergyBarVisual>(entity);
    CHECK(layout.segment_count == 32);
    CHECK_THAT(visual.fill, Catch::Matchers::WithinAbs(0.50f, 1e-5f));
    CHECK(visual.visible_level > visual.fill);
}

TEST_CASE("energy_bar_system: reduce_motion suppresses decorative bounce and flash overlay",
          "[energy_bar][reduce_motion][issue478][issue752]") {
    auto reg = make_registry();
    create_energy_bar_entity(reg);
    settings_state(reg).reduce_motion = true;
    auto& song = reg.ctx().get<SongState>();
    song.playing = true;
    song.song_time = 0.10f;
    song.beat_period = 0.50f;
    auto& energy = reg.ctx().get<EnergyState>();
    energy.display = 0.50f;
    energy.flash_timer = constants::ENERGY_FLASH_DURATION;

    energy_bar_system(reg, 0.016f);

    const auto entity = *reg.view<EnergyBarTag>().begin();
    const auto& visual = reg.get<EnergyBarVisual>(entity);
    CHECK_THAT(visual.visible_level, Catch::Matchers::WithinAbs(visual.fill, 1e-5f));
    CHECK(visual.flash_ratio == 1.0f);
    CHECK(visual.flash_overlay == 0.0f);
}

TEST_CASE("energy_bar_system: reduce_motion pins critical pulse to a stable midpoint",
          "[energy_bar][reduce_motion][issue478][issue752]") {
    auto reg = make_registry();
    create_energy_bar_entity(reg);
    settings_state(reg).reduce_motion = true;
    auto& song = reg.ctx().get<SongState>();
    song.playing = true;
    song.song_time = 99.7f;
    auto& energy = reg.ctx().get<EnergyState>();
    energy.display = 0.0f;

    energy_bar_system(reg, 0.016f);

    const auto entity = *reg.view<EnergyBarTag>().begin();
    const auto& visual = reg.get<EnergyBarVisual>(entity);
    CHECK_THAT(visual.critical_intensity, Catch::Matchers::WithinAbs(0.675f, 1e-5f));
}

TEST_CASE("energy bar layout: invalid segment counts use a safe rendering minimum",
          "[energy_bar][issue886]") {
    EnergyBarLayout layout;

    layout.segment_count = 0;
    CHECK(effective_energy_bar_segment_count(layout) == 1);

    layout.segment_count = 1;
    CHECK(effective_energy_bar_segment_count(layout) == 1);

    layout.segment_count = 32;
    CHECK(effective_energy_bar_segment_count(layout) == 32);
}

TEST_CASE("energy_bar_system: invalid segment counts keep visual levels finite",
          "[energy_bar][issue886]") {
    auto reg = make_registry();
    const auto entity = create_energy_bar_entity(reg);
    auto& layout = reg.get<EnergyBarLayout>(entity);
    auto& energy = reg.ctx().get<EnergyState>();
    auto& song = reg.ctx().get<SongState>();
    song.playing = true;
    song.song_time = 0.0f;
    song.beat_period = 0.5f;
    energy.display = 0.25f;

    layout.segment_count = 0;
    energy_bar_system(reg, 0.016f);
    const auto& visual_zero = reg.get<EnergyBarVisual>(entity);
    CHECK(std::isfinite(visual_zero.fill));
    CHECK(std::isfinite(visual_zero.visible_level));

    layout.segment_count = 1;
    energy_bar_system(reg, 0.016f);
    const auto& visual_one = reg.get<EnergyBarVisual>(entity);
    CHECK(std::isfinite(visual_one.fill));
    CHECK(std::isfinite(visual_one.visible_level));
}
