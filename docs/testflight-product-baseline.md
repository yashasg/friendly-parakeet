# TestFlight Product/Release Baseline (Issues #68, #183, #184, #185, #201)

**Owner:** Edie (PM)  
**Date:** 2026-05-02  
**Milestone:** `test-flight`

This document locks the product baseline for the assigned TestFlight issues and
defines what is complete now versus what still needs explicit owners.

## 1) Decision Summary

| Issue | Decision Status | Locked In |
|---|---|---|
| #68 Platform scope | **Locked** | TestFlight scope is **iOS only** (iPhone, portrait, touch-first). Desktop/WASM remain dev/test targets, not TestFlight distribution targets. |
| #183 Version/build scheme | **Locked (policy)** | `CFBundleShortVersionString` uses SemVer from CMake project version; `CFBundleVersion` is monotonically increasing per TestFlight upload. |
| #184 Bundle/team/signing | **Locked (policy + owner inputs pending)** | Bundle ID target is `com.yashasg.shapeshifter`; signing mode is Xcode automatic signing for v1 TestFlight; no extra capabilities for v1. |
| #185 Telemetry/crash | **Locked** | Apple-first telemetry: MetricKit + TestFlight crash logs, no third-party SDK, no PII, no ATT prompt for v1. |
| #201 Beta program | **Locked** | 3-phase TestFlight rollout, tester checklist, feedback channels, triage SLA, and go/no-go thresholds. |

## 2) Acceptance Criteria Snapshot

### #68 — Lock target platform scope for TestFlight and v1
- [x] Define required platforms for TestFlight: **iOS only**.
- [x] Define required platforms for App Store v1: **iOS (iPhone) only**.
- [x] Document platform-specific input/build/QA requirements (see `docs/ios-testflight-readiness.md` and this baseline).

### #183 — Define iOS app version and build number scheme
- [x] Versioning scheme selected and documented.
- [x] Version propagation path documented.
- [x] Bump policy documented.
- [x] Changelog policy defined.
- [x] Pre-flight build-number bump gate implemented (`tools/ios/preflight_cfbundle_version.sh`, wired in `.github/workflows/ci-macos.yml` release/notarize path).

### #184 — Lock iOS bundle identifier, team, and code-signing plan
- [x] Bundle identifier decision documented (pending account-owner confirmation at Apple registration).
- [x] Team ownership and required Apple account metadata documented.
- [x] Capabilities/entitlements scope documented.
- [x] Code-signing approach documented.
- [x] `ios/README.md` created with build/signing baseline.

### #185 — No crash reporting or telemetry plan
- [x] Minimum event set defined.
- [x] Backend decision locked (Apple-first).
- [x] Privacy/ATT implications documented.
- [x] Data schema/retention/access documented.
- [x] No-PII posture documented.

### #201 — TestFlight recruitment and feedback intake plan
- [x] Cohort and recruitment phases decided.
- [x] Tester welcome + checklist defined.
- [x] Feedback intake channels defined.
- [x] Triage SLA defined.
- [x] Promotion gate criteria defined.

## 3) What Is Complete Now

Complete and execution-ready now:
1. Product scope for TestFlight (iOS-only) and iOS release posture.
2. Version/build policy baseline for App Store Connect fields.
3. Signing/capabilities decision baseline for first TestFlight archive.
4. Telemetry/privacy policy for first cohort.
5. Beta operations model (recruiting, feedback, triage, go/no-go).

## 4) Follow-up Owners and Next Actions

| Follow-up | Owner | Target |
|---|---|---|
| Confirm/register final bundle identifier and Team ID in Apple Developer account | **yashasg** | Before first signed archive |
| Implement iOS project plumbing (`Info.plist`, entitlements wiring, archive path) | **Hockney (Platform)** | Before internal alpha upload |
| Implement MetricKit wiring + local telemetry event emission | **Hockney + McManus** | Before closed beta |
| Create TestFlight groups/public-link flow and execute cohort ops | **Kobayashi (Release) + Verbal (QA)** | Start at internal alpha |
| Add preflight guard for monotonically increasing `CFBundleVersion` | **Kobayashi (Release)** | Before external tester phase |

## 5) Source References

- `docs/testflight-readiness.md` (issues #185, #201 and cross-cutting release gates)
- `docs/ios-testflight-readiness.md` (issues #183, #184 and iOS operational details)
- `ios/README.md` (execution steps for signing/build archive)
- `CHANGELOG.md` (release-note policy baseline)
