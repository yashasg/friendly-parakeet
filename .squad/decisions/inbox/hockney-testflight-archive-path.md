# Hockney Decision — TestFlight archive path automation

**Date:** 2026-05-02  
**Owner:** Hockney (Platform)  
**Issue:** #184

## Decision

Adopt a repo-owned iOS archive automation path via `ios/testflight_archive.sh` with explicit modes:

- `preflight` (tool + blocker checks),
- `configure` (CMake iOS Xcode generation),
- `archive` (signed `.xcarchive`),
- `export` (IPA export),
- `all` (end-to-end).

Also wire iOS bundle metadata in CMake (`MACOSX_BUNDLE`, `Info.plist`, entitlements) so archive steps run against a concrete app bundle target rather than policy-only docs.

## Remaining external blockers

To execute signed archive/export fully, owner-provided Apple account inputs are still required:
`TEAM_ID`, monotonic `BUILD_NUMBER`, registered bundle identifier, Xcode Apple account sign-in, and automatic-signing certificate/profile resolution.
Additionally, the build machine must provide a valid `VCPKG_ROOT` toolchain with iOS triplet support (`arm64-ios`) for dependency resolution.
