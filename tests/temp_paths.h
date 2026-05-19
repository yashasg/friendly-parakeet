#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace test_paths {

inline std::string unique_suffix() {
    static std::atomic<std::uint64_t> counter{0};
    const auto ticks = std::chrono::high_resolution_clock::now()
        .time_since_epoch()
        .count();
    const auto thread_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return std::to_string(ticks) + "_" + std::to_string(thread_hash) + "_" +
           std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

inline std::filesystem::path unique_temp_path(std::string_view prefix) {
    return std::filesystem::temp_directory_path() /
           (std::string(prefix) + "_" + unique_suffix());
}

inline std::filesystem::path unique_relative_path(std::string_view prefix) {
    return std::filesystem::path{std::string(prefix) + "_" + unique_suffix()};
}

struct ScopedPath {
    explicit ScopedPath(std::filesystem::path p) : path(std::move(p)) {}
    ScopedPath(const ScopedPath&) = delete;
    ScopedPath& operator=(const ScopedPath&) = delete;
    ScopedPath(ScopedPath&&) = delete;
    ScopedPath& operator=(ScopedPath&&) = delete;

    ~ScopedPath() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::filesystem::path path;
};

}  // namespace test_paths
