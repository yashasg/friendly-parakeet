# Hockney Decision — #184 raylib iOS overlay unblock

Date: 2026-05-02  
Owner: Hockney (Platform)

## Decision

For iOS triplets in the repo raylib overlay, switch raylib CMake platform selection from `PLATFORM=Desktop` to `PLATFORM=SDL` with `OPENGL_VERSION=ES 2.0`, add an iOS-only `sdl2` dependency in the overlay port metadata, and patch raylib CMake to compile `raudio.c` as Objective-C under iOS (`enable_language(OBJC)` + source language override).

## Why

`PLATFORM=Desktop` in the iOS triplet drove raylib into macOS OpenGL/GLFW wiring, which failed configuration with `OPENGL_LIBRARY` unresolved and blocked `ios/testflight_archive.sh configure`. The SDL+ES2 path avoids the Desktop/OpenGL framework probe and produces a valid iOS raylib static library in the overlay build.

## Validation Evidence

- Reproduced blocker before fix: `TEAM_ID=ABCDE12345 BUILD_NUMBER=1 ios/testflight_archive.sh configure` failed in overlay raylib configure with `PLATFORM=PLATFORM_DESKTOP` and `OPENGL_LIBRARY` not found.
- After fix: `TEAM_ID=ABCDE12345 BUILD_NUMBER=1 ios/testflight_archive.sh configure` completed CMake generation for the iOS build tree (raylib overlay install succeeded).
- Remaining failures are owner-account signing/provisioning metadata only during archive/export.
