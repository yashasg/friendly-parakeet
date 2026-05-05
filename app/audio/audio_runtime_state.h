#pragma once

struct AudioDeviceRuntimeState {
    bool subsystem_started = false;
    bool mixer_ready = false;
    int mix_init_flags = 0;
};
