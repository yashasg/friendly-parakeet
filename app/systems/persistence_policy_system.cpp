#include "persistence_policy_system.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>

#include <raylib.h>

#if defined(__EMSCRIPTEN__)
    #include <emscripten/emscripten.h>
#endif

#ifndef _WIN32
    #include <pwd.h>
    #include <unistd.h>
#endif

#if defined(__APPLE__)
    #include <TargetConditionals.h>
#endif

namespace persistence {
namespace {

#if defined(__EMSCRIPTEN__)
constexpr const char* kWebPersistenceMount = "/persistent";
constexpr const char* kWebPersistenceRoot = "/persistent/shapeshifter";

Result sync_web_filesystem(const bool populate, const Status failure_status) {
    int sync_done = 0;
    int sync_failed = 0;
    EM_ASM(
        {
            FS.syncfs($0 != 0, function(err) {
                HEAP32[$1 >> 2] = err ? 1 : 0;
                HEAP32[$2 >> 2] = 1;
            });
        },
        populate ? 1 : 0,
        &sync_failed,
        &sync_done);

    while (sync_done == 0) {
        emscripten_sleep(1);
    }

    return sync_failed != 0 ? Result{failure_status, std::make_error_code(std::errc::io_error)} : Result{};
}

Result ensure_web_persistence_ready() {
    static bool initialized = false;

    if (initialized) {
        return Result{};
    }

    const int mount_status = EM_ASM_INT(
        {
            if (typeof FS === 'undefined' || typeof IDBFS === 'undefined') {
                return 1;
            }
            try {
                if (!FS.analyzePath(UTF8ToString($0)).exists) {
                    FS.mkdir(UTF8ToString($0));
                }
                if (!Module.shapeshifterPersistenceMounted) {
                    FS.mount(IDBFS, {}, UTF8ToString($0));
                    Module.shapeshifterPersistenceMounted = true;
                }
                return 0;
            } catch (e) {
                console.error('SHAPESHIFTER persistence mount failed', e);
                return 1;
            }
        },
        kWebPersistenceMount);
    if (mount_status != 0) {
        return Result{Status::PathUnavailable, std::make_error_code(std::errc::io_error)};
    }

    const auto init_result = sync_web_filesystem(true, Status::FileReadFailed);
    if (init_result.ok()) {
        initialized = true;
    }
    return init_result;
}

bool path_uses_web_persistence(const std::filesystem::path& path) {
    const std::string normalized = path.lexically_normal().generic_string();
    constexpr std::string_view root{kWebPersistenceRoot};
    return normalized == root || normalized.rfind(std::string(root) + "/", 0) == 0;
}
#endif

}  // namespace

std::error_code ensure_directory_exists(const std::filesystem::path& dir) {
    if (dir.empty()) return {};

    std::error_code ec;
    const bool path_exists = std::filesystem::exists(dir, ec);
    if (ec) return ec;
    if (path_exists) {
        if (std::filesystem::is_directory(dir, ec)) return {};
        return ec ? ec : std::make_error_code(std::errc::not_a_directory);
    }

    if (std::filesystem::create_directories(dir, ec)) return {};
    if (ec) return ec;

    return std::filesystem::is_directory(dir, ec)
        ? std::error_code{}
        : (ec ? ec : std::make_error_code(std::errc::io_error));
}

Result resolve_paths(Paths& out_paths, const std::filesystem::path& root_override) {
#if defined(__EMSCRIPTEN__)
    if (root_override.empty()) {
        const auto web_result = ensure_web_persistence_ready();
        if (!web_result.ok()) {
            return web_result;
        }
    }
#endif

    const auto root_dir = [&]() -> std::filesystem::path {
        if (!root_override.empty()) {
            return root_override;
        }

#ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        if (appdata) return std::filesystem::path(appdata) / "shapeshifter";
        return {};
#elif defined(__EMSCRIPTEN__)
        return std::filesystem::path(kWebPersistenceRoot);
#else
        const char* home = std::getenv("HOME");
        if (!home) {
            const passwd* pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (!home) return {};

#if defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
        return std::filesystem::path(home) / "Library" / "Application Support" / "shapeshifter";
#else
        return std::filesystem::path(home) / ".shapeshifter";
#endif
#endif
    }();
    if (root_dir.empty()) {
        return Result{Status::PathUnavailable, {}};
    }

    const auto ensure_error = ensure_directory_exists(root_dir);
    if (ensure_error) {
        return Result{Status::DirectoryCreateFailed, ensure_error};
    }

    out_paths.root_dir = root_dir;
    out_paths.settings_file = root_dir / "settings.json";
    out_paths.high_scores_file = root_dir / "high_scores.json";
    return Result{};
}

Result prepare_for_persistence_read(const std::filesystem::path& path) {
#if defined(__EMSCRIPTEN__)
    if (!path_uses_web_persistence(path)) {
        return Result{};
    }
    return ensure_web_persistence_ready();
#else
    (void)path;
    return Result{};
#endif
}

Result flush_persistence_writes(const std::filesystem::path& path) {
#if defined(__EMSCRIPTEN__)
    if (!path_uses_web_persistence(path)) {
        return Result{};
    }
    const auto web_result = ensure_web_persistence_ready();
    if (!web_result.ok()) {
        return web_result;
    }
    return sync_web_filesystem(false, Status::FileWriteFailed);
#else
    (void)path;
    return Result{};
#endif
}

const char* status_name(const Status status) {
    // Fabian Principle 1 / issue #1306: enum-as-lookup-key into a static
    // table. Row order must match the `Status` declaration in
    // `app/systems/persistence_policy_system.h`; the `static_assert` pins
    // the table size to the trailing enumerator (`FileWriteFailed`).
    static constexpr std::array<const char*, 8> kNameByStatus{{
        /* Success               */ "success",
        /* MissingFile           */ "missing_file",
        /* CorruptData           */ "corrupt_data",
        /* PathUnavailable       */ "path_unavailable",
        /* DirectoryCreateFailed */ "directory_create_failed",
        /* FileOpenFailed        */ "file_open_failed",
        /* FileReadFailed        */ "file_read_failed",
        /* FileWriteFailed       */ "file_write_failed",
    }};
    static_assert(kNameByStatus.size() ==
                  static_cast<std::size_t>(Status::FileWriteFailed) + 1,
                  "kNameByStatus must cover every Status enumerator");

    const auto idx = static_cast<std::size_t>(status);
    if (idx >= kNameByStatus.size()) return "unknown";
    return kNameByStatus[idx];
}

}  // namespace persistence
