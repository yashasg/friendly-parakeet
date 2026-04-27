# Decision: PR #43 Template Workflow Fixes

**Author:** Hockney  
**Date:** 2026-05  
**Status:** IMPLEMENTED — pending coordinator merge

## Summary

Five unresolved review threads on PR #43 addressed in `.squad/templates/workflows/`. All threads resolved on the PR.

## Decisions Made

### 1. `.squad/` ship-gate narrowed to stateful subdirs

**File:** `.squad/templates/workflows/squad-preview.yml`  
**Decision:** The `Check no AI-team state files are tracked` step now checks specific stateful subdirectories (`.squad/agents/`, `.squad/decisions/`, `.squad/identity/`, `.squad/log/`, `.squad/orchestration-log/`, `.squad/sessions/`, `.squad/casting/`) instead of the entire `.squad/` tree. `.squad/templates/**` is explicitly permitted to be tracked.  
**Rationale:** Broad `.squad/` check broke any repo that legitimately tracks squad templates (like this one). Stateful state must not ship; template content can.

### 2. TEMPLATE header added to all three workflow templates

**Files:** `squad-ci.yml`, `squad-preview.yml` (implicitly), `squad-docs.yml`  
**Decision:** Added `# TEMPLATE — ...` comment block at the top of `squad-ci.yml` (the most likely to be confused with a live workflow) explaining that GitHub Actions does not execute files under `.squad/templates/workflows/` and they must be copied to `.github/workflows/`.  
**Rationale:** GitHub Actions' discovery is strictly `.github/workflows/` scoped. Without the comment, a new contributor could expect these templates to run automatically.

### 3. squad-docs.yml path filter — clarify deployment target

**File:** `.squad/templates/workflows/squad-docs.yml`  
**Decision:** Path filter `'.github/workflows/squad-docs.yml'` is CORRECT for the deployed workflow; it refers to the target path after the template is copied. Added inline TEMPLATE NOTE clarifying this so authors do not "fix" a working path.  
**Rationale:** The path is an intentional forward-reference to the deployed filename. The confusion arises only when reading the template in its source location.

### 4. squad-preview.yml package.json/CHANGELOG.md expectations explicit

**File:** `.squad/templates/workflows/squad-preview.yml`  
**Decision:** Added explicit file-existence checks for `package.json` and `CHANGELOG.md` with targeted `::error::` messages, plus a comment documenting the expected CHANGELOG format (`## [x.y.z]` — Keep a Changelog / standard-version).  
**Rationale:** Silent failure mode (`grep -q ... 2>/dev/null`) was unhelpful for repos missing these files. Fail fast with actionable messages.

### 5. team.md PR mismatch — no-op

**File:** `.squad/team.md`  
**Decision:** No change required. The reviewer artifact is a transient GitHub diff comment from the PR including `.squad/` files alongside a large C++ refactor. `team.md` content is correct.  
**Rationale:** PR review diff confusion, not a content error.
