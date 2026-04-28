#pragma once

#include <filesystem>
#include <system_error>

namespace fs_utils {

// Create all directories on the path, succeeding silently if they already
// exist.  Returns true on success or when the path is empty.
inline bool ensure_directory(const std::filesystem::path& dir) {
    if (dir.empty()) return true;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return !ec;
}

} // namespace fs_utils
