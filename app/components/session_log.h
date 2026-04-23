#pragma once

#include <cstdio>
#include <cstdint>
#include <string>

struct SessionLog {
    FILE*       file  = nullptr;
    uint32_t    frame = 0;
    std::string buffer;        // buffered log lines, flushed once per frame
};
