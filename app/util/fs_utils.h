#pragma once

#include <filesystem>
#include <system_error>

namespace fs_utils {

struct EnsureDirectoryResult {
    bool ok{true};
    std::error_code error{};
};

// Create all directories on the path, succeeding silently if they already
// exist.  Returns true on success or when the path is empty.
inline EnsureDirectoryResult ensure_directory_result(const std::filesystem::path& dir) {
    if (dir.empty()) return EnsureDirectoryResult{};
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return EnsureDirectoryResult{false, ec};
    return EnsureDirectoryResult{};
}

inline bool ensure_directory(const std::filesystem::path& dir) {
    return ensure_directory_result(dir).ok;
}

} // namespace fs_utils
