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

## Versioning Rules

- `CFBundleShortVersionString`: marketing version from `project(VERSION ...)`
  in `CMakeLists.txt`.
- `CFBundleVersion`: integer that increases on every TestFlight upload.

## Build/Archive Baseline

```bash
cmake -B build-ios -S . \
  -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=<TEAM_ID> \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic \
  -DCMAKE_XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER=com.yashasg.shapeshifter
```

Then in Xcode:
1. Open generated `build-ios` project.
2. Verify `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION`.
3. Archive and distribute to TestFlight.

## Policy References

- `docs/testflight-product-baseline.md`
- `docs/ios-testflight-readiness.md`
- `docs/testflight-readiness.md`
