# Session Log: Issue #46 — Release Workflows

**Date:** 2026-04-26T08:44:55Z  
**Agents:** Ralph (impl), Coordinator (tighten), Kujan (review)

## Summary

Ralph implemented GitHub release workflows for push-to-main (`squad-release.yml`) and insider-branch (`squad-insider-release.yml`). Both workflows build, test (non-benchmark), and create releases with `build/shapeshifter` as the sole artifact.

Coordinator tightened implementation: removed `|| echo` fallback, aligned checkout/cache to project CI pattern, validated YAML and sanity checks.

Kujan reviewed finalized workflows and approved. No blocking issues.

## Artifacts

- `.github/workflows/squad-release.yml`: main branch → GitHub release
- `.github/workflows/squad-insider-release.yml`: insider branch → prerelease

## Status

✅ APPROVED for merge.
