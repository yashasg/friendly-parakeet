# iOS Build & TestFlight Baseline

This directory defines the minimum iOS/TestFlight release workflow for
SHAPESHIFTER.

## Product Scope

- TestFlight target: **iOS only**
- Device scope for v1: **iPhone only, portrait**
- Signing mode for v1: **Xcode automatic signing**

## Required Inputs (owner-provided)

Before first upload, provide:
- Apple Developer Team ID
- Confirmed bundle identifier (default target: `com.yashasg.shapeshifter`)
- Initial monotonically increasing build number (`CFBundleVersion`)
- Xcode signed in with the owner Apple Developer account (automatic signing)

## Versioning Rules

- `CFBundleShortVersionString`: marketing version from `project(VERSION ...)`
  in `CMakeLists.txt`.
- `CFBundleVersion`: integer that increases on every TestFlight upload.

## Automated Build/Archive Path

```bash
chmod +x ios/testflight_archive.sh
TEAM_ID=<TEAM_ID> \
BUILD_NUMBER=<MONOTONIC_INT> \
BUNDLE_ID=com.yashasg.shapeshifter \
MARKETING_VERSION=0.1.0 \
ios/testflight_archive.sh all
```

Output artifacts:
- Archive: `build-ios/archive/SHAPESHIFTER.xcarchive`
- Exported IPA: `build-ios/export/*.ipa`

### Script modes

```bash
ios/testflight_archive.sh preflight   # toolchain + blocker checks
ios/testflight_archive.sh configure   # generate iOS Xcode project
ios/testflight_archive.sh archive     # configure + signed xcarchive
ios/testflight_archive.sh export      # export IPA from existing archive
ios/testflight_archive.sh all         # configure + archive + export
```

## Explicit Blocker Checklist (Issue #184)

These are the remaining prerequisites:

1. `TEAM_ID` provided by account owner (`yashasg`).
2. Monotonic `BUILD_NUMBER` selected for this upload (`yashasg`).
3. `com.yashasg.shapeshifter` registered under Apple Developer Identifiers (`yashasg`).
4. Apple Developer account signed into Xcode on the build machine (`yashasg`).
5. Automatic signing resolves an Apple Distribution certificate and provisioning profile (`yashasg`).
6. `VCPKG_ROOT` points to a vcpkg installation with iOS triplet resolution (`arm64-ios`) on the build machine.
7. `raylib` iOS triplet configure currently fails in the overlay (`OPENGL_LIBRARY` not found from `PLATFORM=Desktop` path), blocking full iOS archive until platform/CMake fix lands (`Hockney`).

## Repo Wiring Added

- `ios/Info.plist` (minimum iPhone portrait app metadata).
- `ios/Entitlements.plist` (empty v1 entitlements).
- `ios/testflight_archive.sh` automation script.
- iOS CMake bundle settings now wire `Info.plist` + entitlements for Xcode archives.

## Policy References

- `docs/testflight-product-baseline.md`
- `docs/ios-testflight-readiness.md`
- `docs/testflight-readiness.md`
