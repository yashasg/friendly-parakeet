# Session Log: Issue #56 — vcpkg Overlay Cache Keys

**Date:** 2026-04-26T09:12:58Z  
**Issue:** #56  
**Status:** ✅ COMPLETE

## Summary

Ralph updated the CI cache-key policy so vcpkg overlay port changes invalidate the exact dependency cache keys. Coordinator validated all workflow cache entries and Kujan approved.

## Final Artifacts

**Files:**
- `.github/workflows/ci-linux.yml`
- `.github/workflows/ci-macos.yml`
- `.github/workflows/ci-wasm.yml`
- `.github/workflows/ci-windows.yml`
- `.github/workflows/copilot-setup-steps.yml`
- `.github/workflows/squad-ci.yml`
- `.github/workflows/squad-preview.yml`
- `.github/workflows/squad-release.yml`
- `.github/workflows/squad-insider-release.yml`

**Key changes:**
- Added `vcpkg-overlay/**` to every vcpkg/CMake `hashFiles(...)` cache key
- Preserved platform/compiler prefixes and restore-key behavior
- Removed the stale exact `hashFiles('CMakeLists.txt', 'vcpkg.json')` cache-key shape

## Validation Gates

✓ YAML parse for all `.github/workflows/*.yml`  
✓ Every `hashFiles(...)` entry containing `vcpkg.json` also contains `vcpkg-overlay/**`  
✓ No stale exact `hashFiles('CMakeLists.txt', 'vcpkg.json')` pattern remains  
✓ `git diff --check -- .github/workflows`  
✓ Kujan review approved

## Outcome

✅ Ralph → Coordinator (validated) → Kujan (APPROVED)  
→ GitHub issue comment posted: https://github.com/yashasg/friendly-parakeet/issues/56#issuecomment-4321698445
