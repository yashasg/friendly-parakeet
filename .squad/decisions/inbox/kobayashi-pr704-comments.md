# Kobayashi Decision — PR #704 Review Comment Resolution

- **Date:** 2026-05-11
- **Context:** Replacing placeholder CI workflows with real build/test steps for C++ + vcpkg projects.
- **Decision:** Do **not** shallow-clone vcpkg in GitHub Actions when using manifest mode; clone full history (or fetch required baseline commits) so manifest baselines resolve reliably.
- **Why:** vcpkg manifest baseline resolution may need historical commits not present in shallow clones, causing hard CI failures even when local builds pass.
- **Applied in:** `.github/workflows/squad-ci.yml` on branch `squad/restore-stashed-squad-state`.
