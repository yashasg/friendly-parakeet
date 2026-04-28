#pragma once
#include <ctime>

// Cross-platform localtime wrapper.
// Returns true on success, false on failure.
inline bool safe_localtime(const std::time_t* t, std::tm* result) {
#ifdef _WIN32
    return localtime_s(result, t) == 0;
#else
    return localtime_r(t, result) != nullptr;
#endif
}
