#pragma once

#include "../audio/audio_types.h"
#include "../components/haptics.h"

// Dispatcher events for fire-and-forget audio/haptic playback.
//
// Production path:
//   enqueue: disp.enqueue<PlaySfxEvent>({SFX::ShapeShift})
//   drain:   audio_system calls disp.update<PlaySfxEvent>() → audio_handle_play_sfx()
//
// Listeners registered by wire_audio_haptic_dispatcher (app/audio/audio_routing.h).

struct PlaySfxEvent {
    SFX clip;
};

struct PlayHapticEvent {
    HapticEvent evt;
};
