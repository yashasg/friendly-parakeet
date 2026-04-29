# Skill: CMake vcpkg Cache Guard

## Context

Use this when a CMake project relies on vcpkg manifest packages and local or CI builds fail at `find_package(...)` even though `vcpkg.json` contains the dependency.

## Pattern

`CMAKE_TOOLCHAIN_FILE` is only honored on the first configure of a CMake build tree. Re-running CMake with `-DCMAKE_TOOLCHAIN_FILE=...` against a build directory that was originally configured without vcpkg does not activate the toolchain; it may only record the cache variable.

Guard build scripts against this stale-cache state:

1. Verify `${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` exists before configuring.
2. If `build/CMakeCache.txt` exists but lacks vcpkg activation markers such as `VCPKG_INSTALLED_DIR`, remove only:
   - `build/CMakeCache.txt`
   - `build/CMakeFiles`
3. Preserve `build/vcpkg_installed` so restored CI/local package caches are not thrown away.
4. Re-run CMake with the vcpkg toolchain and manifest install enabled.

## Validation

Check that the regenerated cache contains package directories under `build/vcpkg_installed`, for example:

```bash
grep -E 'EnTT_DIR|VCPKG_INSTALLED_DIR' build/CMakeCache.txt
```

Then run the repository build/test path, e.g.:

```bash
bash run.sh test '~[bench]'
```
