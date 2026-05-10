// Regression tests for #478: SettingsState::reduce_motion is now consumed by
// the energy-bar HUD (and any other site that calls into app/util/motion.h).
// With reduce_motion=true the helpers return the static/baseline value,
// suppressing decorative oscillation while preserving informational state.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "util/motion.h"

TEST_CASE("motion::energy_bar_bounce returns 0 when reduce_motion is on (#478)",
          "[motion][reduce_motion][issue478]") {
    // Without reduce_motion, an in-progress beat phase produces a bounce > 0.
    const float bounce_on  = motion::energy_bar_bounce(0.10f, 0.50f, /*reduce_motion=*/false);
    const float bounce_off = motion::energy_bar_bounce(0.10f, 0.50f, /*reduce_motion=*/true);
    CHECK(bounce_on > 0.0f);
    CHECK(bounce_off == 0.0f);
}

TEST_CASE("motion::energy_bar_bounce is also 0 with no beat clock", "[motion][reduce_motion]") {
    CHECK(motion::energy_bar_bounce(1.0f, 0.0f, false) == 0.0f);
    CHECK(motion::energy_bar_bounce(1.0f, -1.0f, false) == 0.0f);
}

TEST_CASE("motion::energy_critical_pulse pins to midpoint when reduce_motion is on (#478)",
          "[motion][reduce_motion][issue478]") {
    // With reduce_motion the value is the static midpoint at every time.
    CHECK_THAT(motion::energy_critical_pulse(0.0f,  /*reduce_motion=*/true),
               Catch::Matchers::WithinAbs(0.5f, 1e-6f));
    CHECK_THAT(motion::energy_critical_pulse(1.234f, /*reduce_motion=*/true),
               Catch::Matchers::WithinAbs(0.5f, 1e-6f));
    CHECK_THAT(motion::energy_critical_pulse(99.7f,  /*reduce_motion=*/true),
               Catch::Matchers::WithinAbs(0.5f, 1e-6f));

    // Without reduce_motion the pulse oscillates: at t = π/(2*10) the sine
    // hits +1, so the pulse must be 1.0 (within a tiny floating tolerance).
    constexpr float t_peak = 3.14159265358979f / (2.0f * 10.0f);
    CHECK_THAT(motion::energy_critical_pulse(t_peak, /*reduce_motion=*/false),
               Catch::Matchers::WithinAbs(1.0f, 1e-3f));
}

TEST_CASE("motion::flash_overlay_strength suppresses decorative flash overlay (#478)",
          "[motion][reduce_motion][issue478]") {
    CHECK(motion::flash_overlay_strength(0.8f, /*reduce_motion=*/false) == 0.8f);
    CHECK(motion::flash_overlay_strength(0.8f, /*reduce_motion=*/true)  == 0.0f);
    // Even at full intensity, the overlay is suppressed.
    CHECK(motion::flash_overlay_strength(1.0f, /*reduce_motion=*/true)  == 0.0f);
}

// ── #534: extend reduce_motion beyond the energy bar ───────────────────────

TEST_CASE("motion::approach_ring_envelope lerps radius and fades when reduce_motion is off (#534)",
          "[motion][reduce_motion][issue534]") {
    // ratio=0 → ring at btn_radius, full alpha; ratio=1 → max radius, half alpha.
    const auto at_perfect = motion::approach_ring_envelope(0.0f, 10.0f, 30.0f, false);
    CHECK_THAT(at_perfect.radius,      Catch::Matchers::WithinAbs(10.0f, 1e-4f));
    CHECK_THAT(at_perfect.alpha_scale, Catch::Matchers::WithinAbs(1.0f,  1e-4f));

    const auto at_outer = motion::approach_ring_envelope(1.0f, 10.0f, 30.0f, false);
    CHECK_THAT(at_outer.radius,      Catch::Matchers::WithinAbs(30.0f, 1e-4f));
    CHECK_THAT(at_outer.alpha_scale, Catch::Matchers::WithinAbs(0.5f,  1e-4f));

    // Mid-window the envelope must be strictly increasing in radius and
    // decreasing in alpha — i.e. it actually animates.
    const auto mid = motion::approach_ring_envelope(0.5f, 10.0f, 30.0f, false);
    CHECK(mid.radius      > at_perfect.radius);
    CHECK(mid.radius      < at_outer.radius);
    CHECK(mid.alpha_scale < at_perfect.alpha_scale);
    CHECK(mid.alpha_scale > at_outer.alpha_scale);
}

TEST_CASE("motion::approach_ring_envelope snaps to a static imminent indicator with reduce_motion (#534)",
          "[motion][reduce_motion][issue534]") {
    // Outside the imminent window → ring is suppressed entirely.
    const auto far = motion::approach_ring_envelope(0.6f, 10.0f, 30.0f, true);
    CHECK(far.alpha_scale == 0.0f);

    // Just inside the imminent window → ring snaps to max_ring_radius
    // at full alpha (no continuous lerp across the window).
    const auto near_a = motion::approach_ring_envelope(0.10f, 10.0f, 30.0f, true);
    const auto near_b = motion::approach_ring_envelope(0.25f, 10.0f, 30.0f, true);
    CHECK_THAT(near_a.radius, Catch::Matchers::WithinAbs(30.0f, 1e-4f));
    CHECK_THAT(near_b.radius, Catch::Matchers::WithinAbs(30.0f, 1e-4f));
    CHECK(near_a.alpha_scale == 1.0f);
    CHECK(near_b.alpha_scale == 1.0f);
    // Static: radius does not change with ratio inside the window.
    CHECK(near_a.radius == near_b.radius);
}

TEST_CASE("motion::popup_drift_scale and particle_velocity_scale zero kinetic envelope (#534)",
          "[motion][reduce_motion][issue534]") {
    CHECK(motion::popup_drift_scale(false)        == 1.0f);
    CHECK(motion::popup_drift_scale(true)         == 0.0f);
    CHECK(motion::particle_velocity_scale(false)  == 1.0f);
    CHECK(motion::particle_velocity_scale(true)   == 0.0f);
}
