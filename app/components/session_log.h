#pragma once

#include <cstdio>
#include <cstdint>
#include <string>

struct SessionLog {
    FILE*       file  = nullptr;
    uint32_t    frame = 0;
    std::string buffer;        // buffered log lines, flushed once per frame
    int         last_logged_beat = -1;  // beat_log_system tracks last emitted beat
};
