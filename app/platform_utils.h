#pragma once

#include <ctime>
#include <cstdio>

// Portable wrappers for CRT functions that differ between MSVC and POSIX.
// On Windows the CRT marks localtime() and fopen() as deprecated;
// the _s variants exist only on Windows, while POSIX provides localtime_r.

inline std::tm safe_localtime(const std::time_t* t) {
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, t);
#else
    localtime_r(t, &result);
#endif
    return result;
}

inline FILE* safe_fopen(const char* path, const char* mode) {
#ifdef _WIN32
    FILE* fp = nullptr;
    fopen_s(&fp, path, mode);
    return fp;
#else
    return std::fopen(path, mode);
#endif
}
