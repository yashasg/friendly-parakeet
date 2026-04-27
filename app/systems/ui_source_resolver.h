#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <entt/entity/registry.hpp>

std::optional<std::int32_t> resolve_ui_int_source(entt::registry& reg,
                                                   std::string_view source);

// Resolve a UI source + format into a display-ready string.  Used by the
// JSON-driven `text_dynamic` element type and by buttons that bind their
// face text to a runtime source (e.g. settings toggles, game-over reason).
//
// Supported source/format combinations:
//   * "GameOverState.reason" + (any format)        → "ENERGY DEPLETED" /
//                                                    "MISSED A BEAT" /
//                                                    "HIT A BAR" / ""
//   * "SettingsState.haptics_enabled" + "haptics_button" → "HAPTICS: ON/OFF"
//   * "SettingsState.reduce_motion"   + "motion_button"  → "MOTION: ON/OFF"
//   * any int source + "on_off"                          → "ON" / "OFF"
//   * any int source + "signed_ms"                       → "+30 ms" / "-30 ms"
//   * any int source + "" (default)                      → "%d"
//
// Returns std::nullopt when the source cannot be resolved.
std::optional<std::string> resolve_ui_dynamic_text(entt::registry& reg,
                                                    std::string_view source,
                                                    std::string_view format);
