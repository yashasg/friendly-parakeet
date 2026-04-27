#pragma once

#include <cstdio>
#include <cstdint>
#include <string>

struct SessionLog {
    // Maximum bytes buffered per frame before a flush. Pre-reserved at
    // construction so the hot write path never triggers heap reallocation.
    static constexpr std::size_t kMaxLogBufferBytes = 4096;

    FILE*       file  = nullptr;
    uint32_t    frame = 0;
    std::string buffer;        // buffered log lines, flushed once per frame
    int         last_logged_beat = -1;  // beat_log_system tracks last emitted beat

    SessionLog() { buffer.reserve(kMaxLogBufferBytes); }
};
