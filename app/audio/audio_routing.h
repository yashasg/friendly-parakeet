#pragma once

#include <entt/entt.hpp>

struct PlaySfxEvent;
struct PlayHapticEvent;

// Listeners: dispatcher events → hardware calls.
void audio_handle_play_sfx(entt::registry& reg, const PlaySfxEvent& evt);
void haptic_handle_play(entt::registry& reg, const PlayHapticEvent& evt);

// Dispatcher wiring for audio and haptic playback listeners.
void wire_audio_haptic_dispatcher(entt::registry& reg);
void unwire_audio_haptic_dispatcher(entt::registry& reg);
