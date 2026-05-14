#include "all_systems.h"

#include "../components/energy_bar.h"
#include "../components/game_state.h"
#include "../components/song_state.h"
#include "../constants.h"
#include "../entities/settings.h"

#include <cmath>
#include <raymath.h>

namespace {

float energy_bar_bounce(float song_time, float beat_period, bool reduce_motion) {
    if (reduce_motion) return 0.0f;
    if (beat_period <= 0.0f) return 0.0f;
    float phase = std::fmod(song_time, beat_period) / beat_period;
    if (phase < 0.0f) phase = 0.0f;
    float bounce = 1.0f - phase;
    return bounce * bounce * bounce;
}

float energy_critical_pulse(float pulse_time, bool reduce_motion) {
    if (reduce_motion) return 0.5f;
    return 0.5f + 0.5f * std::sin(pulse_time * 10.0f);
}

float flash_overlay_strength(float flash_ratio, bool reduce_motion) {
    return reduce_motion ? 0.0f : flash_ratio;
}

} // namespace

void energy_bar_system(entt::registry& reg, [[maybe_unused]] float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    const auto* energy = reg.ctx().find<EnergyState>();
    if (!energy) return;

    const auto* settings = find_settings_state(reg);
    const bool reduce_motion = settings && settings->reduce_motion;
    const auto* song = reg.ctx().find<SongState>();
    const bool song_playing = song && song->playing;

    float bounce = 0.0f;
    if (song_playing) {
        bounce = energy_bar_bounce(song->song_time, song->beat_period, reduce_motion);
    }

    float flash_ratio = 0.0f;
    if (energy->flash_timer > 0.0f && constants::ENERGY_FLASH_DURATION > 0.0f) {
        flash_ratio = Clamp(energy->flash_timer / constants::ENERGY_FLASH_DURATION, 0.0f, 1.0f);
    }
    const float flash_overlay = flash_overlay_strength(flash_ratio, reduce_motion);

    const float fill = Clamp(energy->display, 0.0f, 1.0f);
    float critical_ratio = 0.0f;
    if (fill < constants::ENERGY_CRITICAL_THRESH && constants::ENERGY_CRITICAL_THRESH > 0.0f) {
        critical_ratio = Clamp((constants::ENERGY_CRITICAL_THRESH - fill)
            / constants::ENERGY_CRITICAL_THRESH, 0.0f, 1.0f);
    }

    const float pulse_time = song_playing ? song->song_time : 0.0f;
    const float critical_pulse = energy_critical_pulse(pulse_time, reduce_motion);
    const float critical_intensity = critical_ratio * (0.35f + 0.65f * critical_pulse);

    auto view = reg.view<EnergyBarTag, EnergyBarLayout, EnergyBarVisual>();
    for (auto [entity, layout, visual] : view.each()) {
        (void)entity;
        const int segment_count = effective_energy_bar_segment_count(layout);
        visual.fill = fill;
        visual.visible_level = std::min(fill + bounce * (5.0f / static_cast<float>(segment_count)), 1.0f);
        visual.flash_ratio = flash_ratio;
        visual.flash_overlay = flash_overlay;
        visual.critical_intensity = critical_intensity;
        visual.overflow_segments = 0;
        if (fill >= 0.99f) {
            visual.overflow_segments = static_cast<int>(bounce * 5.0f + 0.5f);
        }
    }
}
