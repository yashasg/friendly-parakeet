# SHAPESHIFTER — iOS TestFlight Readiness

> **Status:** Draft — awaiting user-provided values marked `⚠ USER-PROVIDED`  
> **Owner:** Hockney (Platform)  
> **Covers:** Issues #180, #182, #183, #184, #186  
> **Milestone:** test-flight

---

## Table of Contents

1. [Audio Session Interruption Policy (#180)](#1-audio-session-interruption-policy-180)
2. [App Background / Foreground Lifecycle (#182)](#2-app-background--foreground-lifecycle-182)
3. [Version and Build Number Scheme (#183)](#3-version-and-build-number-scheme-183)
4. [Bundle Identifier, Team, and Code Signing (#184)](#4-bundle-identifier-team-and-code-signing-184)
5. [Device and OS Support Matrix (#186)](#5-device-and-os-support-matrix-186)

---

## 1. Audio Session Interruption Policy (#180)

### 1.1 AVAudioSession Category

**Decision: `AVAudioSessionCategoryPlayback`**

Rationale: SHAPESHIFTER is a rhythm game where audio is the primary, intentional content — not ambient background sound. `Playback` ensures the game audio respects the hardware mute switch, coexists correctly with Control Center Now Playing, and produces the correct interruption-begin/end callbacks. `Ambient` would allow the game to mix under other audio (wrong for a rhythm game) and loses interruption-end callbacks needed to resync `song_time`.

```objc
// ios/AppDelegate.mm (to be created)
AVAudioSession *session = [AVAudioSession sharedInstance];
[session setCategory:AVAudioSessionCategoryPlayback error:nil];
[session setActive:YES error:nil];
```

**Mode:** `AVAudioSessionModeDefault` (no special processing needed for game audio).

**Options:** None for v1 (no `MixWithOthers` — the game owns the audio focus).

### 1.2 Interruption Handling Policy

All OS-driven audio interruptions trigger the **same state machine as a manual pause** (see issue #74 / `design-docs/game-flow.md` § Pause Screen). The run is **not ended** — the player returns to an explicit resume prompt.

| Interruption Type | Begin | End |
|---|---|---|
| Incoming call (begin) | `PauseMusicStream()` → game enters Paused state | — |
| Incoming call (declined / ends) | — | Game stays Paused; player must tap Resume |
| Siri activation | `PauseMusicStream()` → Paused state | Player must tap Resume |
| FaceTime (begin) | `PauseMusicStream()` → Paused state | Player must tap Resume |
| Alarm / timer fires | `PauseMusicStream()` → Paused state | Player must tap Resume |
| Control Center audio takeover | `PauseMusicStream()` → Paused state | Player must tap Resume |
| Headphones/AirPods unplug (`AVAudioSessionRouteChangeReasonOldDeviceUnavailable`) | `PauseMusicStream()` → Paused state | Player must tap Resume |
| AirPods disconnect | `PauseMusicStream()` → Paused state | Player must tap Resume |
| Route change (non-removal) | No pause (e.g., Bluetooth reconnect) | No action |

**Auto-resume is disabled.** Rationale: a rhythm game cannot auto-resume without a countdown/grace period (not in scope for v1). The player must explicitly tap Resume to re-sync their attention with `song_time`. This matches the #74 manual pause contract.

### 1.3 `song_time` Sync on Resume

On resume (player taps Resume after any interruption):

1. `ResumeMusicStream()` is called.
2. `song_time` resumes from the exact frozen value (same as manual pause — no drift because the music stream was paused, not playing silently).
3. The obstacle scheduler (`beat_scheduler_system`) re-reads `song_time` on the next frame — no reset needed.
4. No grace window / input clearing is added for v1 (consistent with #74 decision: resume is immediate, stale input cleared).

> **Platform note:** raylib's `PauseMusicStream` / `ResumeMusicStream` wraps the miniaudio backend. On iOS with `AVAudioSessionCategoryPlayback`, miniaudio correctly stops/resumes the audio device. If drift is observed in testing, a `SetMusicTimePlayed(song_time_at_pause)` seek may be needed — flag as a bug not a policy change.

### 1.4 UAT — Interruption Checklist (Physical Device)

| # | Scenario | Expected Result | Pass / Fail |
|---|---|---|---|
| I-01 | Phone call arrives mid-song | Game pauses immediately; music stops | |
| I-02 | Call declined → game resumed | Tap Resume → music continues from correct beat | |
| I-03 | Call accepted, then ended → game resumed | Same as I-02 | |
| I-04 | Siri activated (hold side button) | Game pauses; music stops | |
| I-05 | Alarm fires mid-song | Game pauses; music stops | |
| I-06 | AirPods removed mid-song | Game pauses; music stops | |
| I-07 | Headphone jack unplugged (Lightning adapter) | Game pauses; music stops | |
| I-08 | Another app takes audio focus (Spotify) | Game pauses; music stops | |
| I-09 | Control Center swipe → audio controls shown | Game pauses; music stops | |
| I-10 | Resume after any of above | Score/HP/song_time preserved; music re-syncs | |

---

## 2. App Background / Foreground Lifecycle (#182)

### 2.1 Lifecycle Events and Actions

SHAPESHIFTER maps iOS app lifecycle notifications to the same pause state machine as manual pause and audio interruption (#180):

| iOS Lifecycle Event | Game Action | Audio Action |
|---|---|---|
| `applicationWillResignActive` | Enter Paused state; freeze `song_time` | `PauseMusicStream()` |
| `applicationDidEnterBackground` | Game already Paused (from `WillResignActive`) | `StopMusicStream()` — release audio resources |
| `applicationWillEnterForeground` | No action (still Paused) | — |
| `applicationDidBecomeActive` | Show Resume prompt; call `PlayMusicStream()` → seek to `song_time_at_pause` | `PlayMusicStream()` + `SetMusicTimePlayed(song_time_at_pause)` |
| Low Power Mode entry (mid-song) | Pause; notify player via pause screen subtitle "Low Power Mode" | `PauseMusicStream()` |

**Rationale for `StopMusicStream` on background (not just pause):** iOS may suspend the process, and a paused audio device still holds system resources. Stopping and re-seeking on foreground is safer. This requires `SetMusicTimePlayed` on resume; if raylib's implementation is unreliable on iOS, a fallback is to restart the song from the beginning and show a "song restarted" notice.

### 2.2 Preserved State on Background

All game state lives in the `entt::registry` (per architecture). The following is preserved across background/foreground:

| State | Preserved | Notes |
|---|---|---|
| `song_time` (frozen at pause) | ✅ Yes | Stored as singleton in registry context |
| Score | ✅ Yes | `ScoreState` component |
| HP / Energy | ✅ Yes | `EnergyBar` component |
| Beatmap index / beat cursor | ✅ Yes | `BeatSchedulerState` singleton |
| Current shape | ✅ Yes | `PlayerShape` component |
| Current lane | ✅ Yes | `PlayerLane` component |
| Active obstacles (entities) | ✅ Yes | Registry intact in memory |
| Chain count | ✅ Yes | `ScoreState.chain_count` |

**Not preserved (acceptable for v1):**

- Active particle effects — cleared on resume.
- In-flight haptic feedback — cancelled.

### 2.3 iOS Process Suspension

If iOS terminates the process while backgrounded (memory pressure):

- The run is **lost**. No "recover run" mechanic for v1.
- High scores already written to disk (JSON persistence, issue #71) are preserved.
- On next launch, the player sees the Title Screen (normal cold-start flow).

This matches common rhythm game behavior (Beat Saber, Cytus II). A "resume saved run" mechanic is out of scope for v1.

### 2.4 UAT — Lifecycle Checklist (Physical Device)

| # | Scenario | Expected Result | Pass / Fail |
|---|---|---|---|
| L-01 | Home gesture mid-song | Game pauses immediately; music stops | |
| L-02 | Return to game after Home gesture | Resume prompt shown; music re-syncs | |
| L-03 | Lock screen mid-song | Game pauses; music stops | |
| L-04 | Unlock and return | Resume prompt shown; no desync | |
| L-05 | App switcher (swipe up, hold) | Game pauses | |
| L-06 | Switch to another app and return | Same as L-02 | |
| L-07 | Low Power Mode enabled mid-song | Game pauses with subtitle note | |
| L-08 | Leave backgrounded for 60+ seconds, return | Resume prompt shown; score/HP intact | |
| L-09 | Process killed by OS (memory), relaunch | Title Screen shown; no crash | |
| L-10 | High score before background is on Title Screen after cold relaunch | High score persisted | |

---

## 3. Version and Build Number Scheme (#183)

### 3.1 Fields

| Field | Key | Example | Source of Truth |
|---|---|---|---|
| Marketing version | `CFBundleShortVersionString` | `0.1.0` | `CMakeLists.txt` `project(VERSION ...)` |
| Runtime version macros | `SHAPESHIFTER_VERSION*` | `0.1.<git-hash>` | `app/version.h.in` configured by CMake from `project(VERSION ...)` + git hash |
| Build number | `CFBundleVersion` | `42` | Set in Xcode/CI release configuration (no repo file source today) |

### 3.2 Marketing Version (`CFBundleShortVersionString`)

**Scheme:** `MAJOR.MINOR.PATCH` — aligned with [Semantic Versioning](https://semver.org/).

**Source of truth:** The `VERSION` field in `CMakeLists.txt`:

```cmake
project(shapeshifter VERSION 0.1.0 LANGUAGES CXX)
```

Currently `0.1.0` (pre-release). Bump rules:

- **PATCH** (`0.1.x`): TestFlight builds, hotfixes.
- **MINOR** (`0.x.0`): Feature-complete milestone (e.g., v1 App Store release).
- **MAJOR** (`x.0.0`): Reserved for post-v1 significant rewrites.

The runtime version propagates to `generated/version.h` via CMake `configure_file` (already wired in `CMakeLists.txt`). `app/version.h.in` is the template that defines `SHAPESHIFTER_VERSION_MAJOR`, `SHAPESHIFTER_VERSION_MINOR`, and `SHAPESHIFTER_VERSION` using CMake project version + git hash.

### 3.3 Build Number (`CFBundleVersion`)

**Scheme:** Monotonically increasing integer. Must never decrease for any build submitted to the same Apple ID / bundle identifier.

**Current state:** There is no dedicated build-number source file in the repo. `app/ios/build_number.txt` was removed because it is not consumed by CMake or build scripts.

**Bump policy:**
- Bump before every TestFlight upload (even if marketing version is unchanged).
- Bump is managed in Xcode (`CURRENT_PROJECT_VERSION`) or CI at archive/upload time.
- The integer never resets, even across marketing version bumps.

**Preflight note:** There is currently no repo-backed preflight script for `CFBundleVersion`. If future tooling is added, it should use the actual Xcode/CI build setting source rather than a standalone text file.

### 3.4 When Each Bumps

| Event | `CFBundleShortVersionString` | `CFBundleVersion` |
|---|---|---|
| Every TestFlight upload | Only if feature milestone changed | **Always bump** |
| Hotfix TestFlight build | Bump PATCH | Bump |
| App Store submission | Bump MINOR or MAJOR | Bump |

### 3.5 Changelog Policy

**Format:** [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) (`CHANGELOG.md` at repo root).

**Sections per release:** `Added`, `Changed`, `Fixed`, `Removed`.

**Update cadence:** Every TestFlight/App Store upload that bumps `CFBundleVersion` should include a CHANGELOG entry. This is a team convention, not a CI gate for v1 (add CI gate post-TF).

`CHANGELOG.md` does not yet exist — it must be created before the first TestFlight submission. Template:

```markdown
# Changelog

## [Unreleased]
### Added
- Initial TestFlight build.
```

---

## 4. Bundle Identifier, Team, and Code Signing (#184)

### 4.1 Bundle Identifier

**Proposed:** `com.yashasg.shapeshifter`

This follows the reverse-domain convention using the GitHub handle as the domain segment. It is consistent with the repo name (`friendly-parakeet` is the internal codename; `shapeshifter` is the product name).

> **⚠ USER-PROVIDED:** The bundle identifier must be registered in Apple Developer → Certificates, Identifiers & Profiles by the account owner (`yashasg`). The proposed value `com.yashasg.shapeshifter` is a squad recommendation — the owner may use any valid identifier tied to their Apple Developer account.

### 4.2 Apple Developer Program

| Item | Value |
|---|---|
| Program type | ⚠ USER-PROVIDED: Individual or Organization (paid, $99/yr USD) |
| Team ID | ⚠ USER-PROVIDED: 10-character string from developer.apple.com |
| Team name | ⚠ USER-PROVIDED |
| Primary account holder | `yashasg` (assumed) |

**Capabilities for v1 (minimal):**
- No Game Center (not in scope, #192).
- No iCloud (not in scope, #192).
- No Push Notifications.
- No Associated Domains.
- No In-App Purchase.

Entitlements file (`ios/Entitlements.plist`) will be minimal:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict/>
</plist>
```

### 4.3 Code Signing Approach

**v1 (TestFlight): Local/Xcode Automatic Signing**

- Xcode Automatic Signing with a personal team is sufficient for TestFlight.
- `yashasg` runs `xcodebuild` locally (or via Xcode IDE) with Automatic signing enabled.
- No CI signing for v1 — CI signing (GitHub Actions + App Store Connect API key) is a post-TF improvement.

**Build steps (local, macOS with Xcode 16+):**

1. Install CMake iOS toolchain dependencies.
2. Generate Xcode project:
   ```bash
   cmake -B build-ios -S . \
     -G Xcode \
     -DCMAKE_SYSTEM_NAME=iOS \
     -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
     -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=<TEAM_ID> \     # ⚠ USER-PROVIDED
     -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic \
     -DCMAKE_XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER=com.yashasg.shapeshifter
   ```
3. Open `build-ios/shapeshifter.xcodeproj` in Xcode.
4. Set target device to your registered iPhone (or use generic iOS device for archive).
5. Product → Archive → Distribute App → TestFlight.

> The iOS CMake toolchain integration requires additional work not yet in the repo (raylib iOS build config, asset bundling into `.app`, `Info.plist` template). This document specifies the decisions; implementation tracking is a separate issue.

### 4.4 Info.plist Template (Minimum Required Keys)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC ...>
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key>
    <string>com.yashasg.shapeshifter</string>
    <key>CFBundleName</key>
    <string>SHAPESHIFTER</string>
    <key>CFBundleDisplayName</key>
    <string>Shapeshifter</string>
    <key>CFBundleShortVersionString</key>
    <string>$(MARKETING_VERSION)</string>
    <key>CFBundleVersion</key>
    <string>$(CURRENT_PROJECT_VERSION)</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>LSRequiresIPhoneOS</key>
    <true/>
    <key>UIRequiredDeviceCapabilities</key>
    <array>
        <string>arm64</string>
    </array>
    <key>UISupportedInterfaceOrientations</key>
    <array>
        <string>UIInterfaceOrientationPortrait</string>
    </array>
    <key>UIStatusBarHidden</key>
    <true/>
    <key>UIRequiresFullScreen</key>
    <true/>
    <key>NSMicrophoneUsageDescription</key>
    <!-- Not requesting microphone; omit if not used -->
    <!-- Add NSMicrophoneUsageDescription only if audio recording is added -->
</dict>
</plist>
```

`$(MARKETING_VERSION)` and `$(CURRENT_PROJECT_VERSION)` are Xcode build settings; set them to match `CFBundleShortVersionString` and `CFBundleVersion` from §3.

### 4.5 `ios/` Directory Structure (to be created)

```
ios/
  README.md                  ← Build + signing instructions (this doc summarises)
  Info.plist                 ← Template (above)
  Entitlements.plist         ← Minimal (above)
  LaunchScreen.storyboard    ← Required by App Store (solid color + logo)
  Assets.xcassets/
    AppIcon.appiconset/      ← Required icons (1024×1024 + all sizes)
```

> None of these files exist yet. Creating them is required before first TestFlight upload.

---

## 5. Device and OS Support Matrix (#186)

### 5.1 Minimum iOS Version

**Decision: iOS 16.0**

Rationale:
- iOS 16 covers ~95%+ of active devices (as of 2025).
- `AVAudioSession` category/interruption APIs used in §1 are stable since iOS 7; no version constraint from audio.
- Safe Area API (`safeAreaInsets`) stable since iOS 11; `UIWindowScene` stable since iOS 13.
- Avoids needing iOS 17+ APIs (MetricKit v3, ActivityKit, etc.) not in scope.
- `CMake` iOS deployment target: `-DCMAKE_OSX_DEPLOYMENT_TARGET=16.0`.

### 5.2 Supported Device Classes

**v1 scope: iPhone only (portrait, 60Hz+)**

| Class | Supported | Notes |
|---|---|---|
| iPhone SE (2nd gen, 2020, A13) | ✅ Yes | Minimum performance baseline; 4.7" 750×1334 |
| iPhone SE (3rd gen, 2022, A15) | ✅ Yes | |
| iPhone 11 (notch era) | ✅ Yes | |
| iPhone 12 / 13 / 14 (notch) | ✅ Yes | |
| iPhone 14 Pro / 15 Pro (Dynamic Island) | ✅ Yes | Safe area handles Dynamic Island cutout |
| iPhone 15 / 16 (all) | ✅ Yes | |
| iPad (all) | ❌ No for v1 | Scaled iPhone UI approach deferred; iPad multitasking/Stage Manager out of scope |
| iPod Touch | ❌ No (discontinued) | |
| Apple TV | ❌ No | |

**Portrait-only:** `UISupportedInterfaceOrientations` = `UIInterfaceOrientationPortrait` only.
`UIRequiresFullScreen = YES` disables Split View and Stage Manager.

### 5.3 Viewport Scaling

The logical viewport is fixed at **720×1280** (portrait, ~16:9 baseline). Physical device resolutions vary:

| Device | Logical Points | Physical | Approach |
|---|---|---|---|
| iPhone SE 2/3 | 375×667 pt | 750×1334 | Pillarbox (horizontal letterbox) |
| iPhone 14 (non-Pro) | 390×844 pt | 1170×2532 | Letterbox (top/bottom bars) |
| iPhone 14 Pro / 15 Pro | 393×852 pt | 1179×2556 | Letterbox |
| iPhone 16 Pro Max | 440×956 pt | 1320×2868 | Letterbox |

**Scaling policy:** Render the 720×1280 logical scene to a centered `RenderTexture2D`; draw it onto the physical framebuffer with uniform scaling (fit, no stretch). Black bars fill unused space. This avoids layout reflow for v1.

**Safe area:** All interactive HUD elements (shape buttons, energy bar, score) must be inset from the safe area on all devices. The safe area policy from issue #146 applies.

**Minimum aspect ratio:** 9:19.5 (iPhone SE) through 9:21 (modern iPhones). The 720×1280 logical viewport is 9:16 — slightly wider than modern tall iPhones, so small letterbox bars on top/bottom are expected and acceptable.

### 5.4 Refresh Rate

**v1: Cap framerate at 60fps.** ProMotion (120Hz) timing fairness is tracked in issue #204. Until #204 is resolved, `SetTargetFPS(60)` prevents `dt` from halving on ProMotion devices and breaking obstacle scroll speed.

### 5.5 UAT Device List (Minimum)

| # | Device | iOS | Screen class | Required |
|---|---|---|---|---|
| D-01 | iPhone SE (2nd or 3rd gen) | ≥16.0 | Home button, small screen | ✅ Yes |
| D-02 | Any notch iPhone (11/12/13) | ≥16.0 | Notch, Face ID | ✅ Yes |
| D-03 | iPhone 14 Pro or 15 Pro | ≥16.0 | Dynamic Island | ✅ Yes |
| D-04 | iPad (any, if iPad support added later) | ≥16.0 | Large screen | ❌ v1 not required |

All three required devices must pass the full interruption (§1.4) and lifecycle (§2.4) UAT checklists.

---

## Appendix: User-Provided Values Summary

The following values are **not squad decisions** — they must be supplied by `yashasg` before the first TestFlight build can be submitted:

| # | Field | Where needed | Placeholder used |
|---|---|---|---|
| 1 | Apple Developer Team ID (10-char) | CMake `-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM` | `<TEAM_ID>` |
| 2 | Apple Developer Program type (individual/org) | Documentation only | _(unset)_ |
| 3 | Confirm bundle ID `com.yashasg.shapeshifter` | `Info.plist`, App ID registration | Proposed; may change |
| 4 | App icons (1024×1024 + all sizes) | `Assets.xcassets/AppIcon.appiconset/` | None |
| 5 | Set initial `CFBundleVersion` in Xcode/CI before first upload (no repo build-number file) | Release/archive configuration | _(unset)_ |

---

*Last updated by Hockney (Platform Engineer). Cross-reference: `design-docs/game-flow.md` (pause state machine), `docs/audio-pipeline-spec.md` (audio backend), issues #74, #68, #71.*
