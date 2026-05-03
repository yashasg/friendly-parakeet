# Changelog

All notable changes to this project should be documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- TestFlight product/release baseline for issues #68, #183, #184, #185, #201.
- iOS TestFlight automation script (`ios/testflight_archive.sh`) for preflight, configure, archive, and IPA export.
- iOS bundle metadata files (`ios/Info.plist`, `ios/Entitlements.plist`) wired into CMake for archiveable iOS bundles.
