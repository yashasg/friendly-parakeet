# Kobayashi Decision — TestFlight CFBundleVersion Preflight Gate (#183)

Date: 2026-05-02

## Decision
Implement `tools/ios/preflight_cfbundle_version.sh` as the release gate for build-number monotonicity, and wire it into `.github/workflows/ci-macos.yml` `notarize` job before any signing/notarization/upload steps.

## Rule
- Gate fails unless `current CFBundleVersion > previous`.
- Previous build source precedence:
  1. `workflow_dispatch` input `previous_uploaded_cfbundle_version` (for App Store Connect-known latest upload).
  2. Latest `ios-build-<n>` git tag in the repo.
  3. Baseline `0` if neither exists yet.

## Rationale
This gives a deterministic, automation-enforced check in the release path without introducing a fake repo build-number file. It supports both tagged-release and manually-dispatched release workflows while keeping the policy compatible with real upload state when operators provide the previous uploaded build number.
