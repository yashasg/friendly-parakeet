# iOS TestFlight Readiness — Platform Decisions

**Author:** Hockney (Platform Engineer)  
**Date:** 2026-05-01  
**Covers:** Issues #180, #182, #183, #184, #186  
**Status:** PROPOSED — awaiting team awareness / user-provided values

---

## Decisions Made (Team-Relevant)

### Audio Session (#180)
- **AVAudioSession category: `AVAudioSessionCategoryPlayback`** — rhythm game is primary intentional audio. Gives interruption-begin/end callbacks; Ambient does not.
- All OS-driven audio interruptions trigger the same pause state machine as #74 (manual pause). No auto-resume.
- `song_time` resumes from frozen value on player-taps-Resume; no drift. If drift observed, `SetMusicTimePlayed` seek is the fix, not a policy change.

### App Lifecycle (#182)
- `applicationWillResignActive` → Paused + `PauseMusicStream()`.
- `applicationDidEnterBackground` → `StopMusicStream()` (releases audio device under iOS suspension).
- `applicationDidBecomeActive` → Resume prompt + `PlayMusicStream()` + `SetMusicTimePlayed(song_time_at_pause)`.
- Process-killed-by-OS: run lost, high scores preserved (per #71 persistence policy). No "recover run" for v1.

### Version Scheme (#183)
- `CFBundleShortVersionString`: `MAJOR.MINOR.PATCH` SemVer. Source of truth: `project(shapeshifter VERSION ...)` in `CMakeLists.txt`.
- `CFBundleVersion`: monotonic integer. Source of truth: `app/ios/build_number.txt` (created as `0`).
- Bump policy: build number bumps on every TF upload; short version bumps on feature milestones.
- `CHANGELOG.md` (Keep-a-Changelog) required before first TF upload.
- Preflight script `tools/ios_preflight.sh` blocks CI if build number not bumped vs last tag.

### Bundle ID / Signing (#184)
- Proposed bundle ID: `com.yashasg.shapeshifter`. **USER MUST CONFIRM AND REGISTER.**
- v1 signing: Local Xcode Automatic Signing. CI signing deferred.
- No capabilities for v1 (no Game Center, iCloud, Push, IAP).
- CMake iOS generate command documented in `docs/ios-testflight-readiness.md` §4.

### Device Matrix (#186)
- Minimum iOS: 16.0.
- iPhone-only, portrait-only for v1. iPad deferred.
- 720×1280 logical viewport, centered uniform scaling, black letterbox bars.
- Cap 60fps until #204 (ProMotion) resolved.
- UAT minimum: 3 devices (SE home-button, notch, Dynamic Island).

---

## User-Provided Values Blocking Progress

| # | Value | Blocks |
|---|---|---|
| 1 | Apple Developer Team ID | iOS Xcode build |
| 2 | Confirm bundle ID `com.yashasg.shapeshifter` | App ID registration |
| 3 | Program type (individual/org) | Documentation |
| 4 | App icons | TestFlight upload |
| 5 | Bump `app/ios/build_number.txt` 0 → 1 | First TF upload |
