#pragma once

#include <raylib.h>

inline Music load_music_stream(const char* path, bool repeat) {
    Music stream = LoadMusicStream(path);
    stream.looping = repeat;
    return stream;
}
