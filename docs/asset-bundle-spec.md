# SHAPESHIFTER: Asset Bundle (PAK) — Technical Specification

## Status: DRAFT — not yet implemented

---

## Problem Statement

The current build produces loose files next to the executable:

```
build/
  shapeshifter
  content/
    beatmaps/2_drama_beatmap.json
    audio/2_drama.flac
    fonts/LiberationMono-Regular.ttf
```

CI artifacts upload only the binary. Loose content files are missing,
causing the shipped binary to silently fall back to freeplay/silent mode.
The solution is a single-file asset bundle (PAK) that:
- ships as one artifact alongside the binary
- supports load-in-place via mmap (zero copy for audio)
- is tamper-evident via Ed25519 signature (CI signs, binary verifies)
- is bypassed entirely in local dev via `--dev` CLI flag

---

## Scope

| In scope                              | Out of scope                      |
|---------------------------------------|-----------------------------------|
| content/ bundle (beatmaps + audio + fonts) | Per-entry encryption          |
| Ed25519 PAK signature                 | Hot reload / live patching        |
| `--dev` loose-file mode               | Compression (add later if needed) |
| pak_tool CLI (pack + sign)            | Streaming / partial loads         |
| AssetLoader abstraction in game       | —                                 |

---

## PAK File Format

```
Offset      Size    Field
----------- ------- ----------------------------------------------------------
0           4       magic: 0x53 0x48 0x50 0x4B  ("SHPK")
4           2       version: uint16_le  (current: 1)
6           2       entry_count: uint16_le
8           32      sha256 of TOC region (bytes 0 .. header+TOC end)
40          64      Ed25519 signature of (magic+version+entry_count+toc_sha256)
                    signed with CI private key
104         N*120   TOC entries (see below)
104+N*120   ...     DATA blob (raw entry bytes, page-aligned)

TOC Entry (120 bytes each):
  Offset  Size  Field
  0       64    logical_path: null-terminated UTF-8
                e.g. "content/audio/2_drama.flac"
  64      8     offset: uint64_le  (byte offset from start of DATA blob)
  72      8     size:   uint64_le  (uncompressed size in bytes)
  80      32    sha256: SHA-256 of entry data
  112     8     reserved: zero (future: flags, compression type)
```

Page-alignment: each entry's data offset is aligned to 4096 bytes so
mmap pointers can be passed directly to raylib *FromMemory functions
without any copy.

---

## Signature Scheme

```
Key type:    Ed25519  (libsodium crypto_sign_ed25519)
Key size:    32-byte public key, 64-byte signature
Private key: CI secret (GitHub Actions secret: PAK_SIGNING_KEY)
             Never stored in repo, never shipped in binary
Public key:  Baked into binary at build time via CMake configure_file
             app/pak_pubkey.h.in -> build/generated/pak_pubkey.h
             Readable by reverse engineers -- expected and fine

Signed over: HEADER bytes 0..103 (magic, version, count, toc_sha256)
             DATA is covered transitively via per-entry sha256 in TOC,
             which is covered by the toc_sha256 in the signed header.

Verification at runtime (PAK mode only):
  1. Check magic == "SHPK"
  2. Verify Ed25519 signature using compiled-in public key
  3. Recompute SHA-256 of TOC region, compare to header field
  4. On load of each entry: recompute SHA-256, compare to TOC entry
  5. Any failure -> TraceLog(LOG_ERROR) + abort
```

---

## AssetLoader Abstraction

```cpp
// app/asset_loader.h

enum class AssetMode { Pak, Loose };

struct AssetConfig {
    AssetMode   mode         = AssetMode::Pak;
    std::string content_root = "";  // empty = exe-relative default
};

// Points into mmap region (PAK, owned=false) or heap buffer (Loose, owned=true)
struct AssetView {
    const uint8_t* data  = nullptr;
    size_t         size  = 0;
    bool           owned = false;
};

AssetView load_asset(const AssetConfig& cfg, const std::string& logical_path);
void      free_asset(AssetView& view);
```

**PAK mode:** mmap entire file, verify signature, return pointer into mapping.
Zero copies for audio -- the mmap IS the buffer passed to LoadMusicStreamFromMemory.

**Loose mode (--dev):** fread into heap buffer, no signature check.

---

## CLI Interface

```
./shapeshifter                          release: PAK mode, signature verified
./shapeshifter --dev                    local:   loose files, no sig check
./shapeshifter --dev --content ./src    override content root
```

Parsed in main() before InitWindow(). AssetConfig emplaced into reg.ctx().
run.sh passes --dev automatically. CI workflows never pass --dev.

---

## pak_tool CLI

Standalone CMake target in tools/pak_tool/. Not linked into the game.

```
pak_tool pack    --input content/ --output content.pak [--sign-key key.bin]
pak_tool verify  --input content.pak --pub-key pubkey.bin
pak_tool list    --input content.pak
pak_tool extract --input content.pak --entry content/audio/2_drama.flac
pak_tool keygen  --out-private pak_private.bin --out-public pak_public.bin
```

Key generation is a one-time developer operation:
- pak_private.bin -> base64 -> GitHub Actions secret PAK_SIGNING_KEY
- pak_public.bin  -> committed to repo -> baked into binary via configure_file

Unsigned PAK (no --sign-key): signature field zeroed.
Verification is skipped when compiled-in public key is the all-zero sentinel.
This is only possible in debug builds -- release build rejects zero key at
compile time via static_assert on the generated header.

---

## CMake Integration

```cmake
# tools/pak_tool/ -- new subdirectory
add_executable(pak_tool tools/pak_tool/main.cpp)
target_link_libraries(pak_tool PRIVATE sodium)

# Replace current content copy POST_BUILD with pak step:
add_custom_command(TARGET shapeshifter POST_BUILD
    COMMAND pak_tool pack
        --input  "${CMAKE_SOURCE_DIR}/content"
        --output "$<TARGET_FILE_DIR:shapeshifter>/content.pak"
    COMMENT "Packing content assets"
)
```

CI signing step (injected after pack):
```yaml
- name: Sign content.pak
  env:
    PAK_SIGN_KEY: ${{ secrets.PAK_SIGNING_KEY }}
  run: |
    printf '%s' "$PAK_SIGN_KEY" | base64 -d > /tmp/pak.key
    pak_tool pack --input content/ --output build/content.pak \
                  --sign-key /tmp/pak.key
    rm /tmp/pak.key
```

---

## Updated CI Artifact Upload

```yaml
# All three workflows (linux, macos, windows)
- uses: actions/upload-artifact@v5
  with:
    name: shapeshifter-macos        # or -linux / -windows
    path: |
      build/shapeshifter            # or shapeshifter.exe on Windows
      build/content.pak
```

---

## New Dependency: libsodium

Required by pak_tool (sign/verify) and game runtime (verify).

```json
// vcpkg.json -- add to dependencies
{ "name": "libsodium" }
```

libsodium is stable, well-audited, and vcpkg-available on all target
platforms including Emscripten (compiled to WASM).

---

## Migration Path

Steps are ordered for independent mergeability.
Steps 1-4 have zero runtime impact -- safe to land anytime.
Steps 5-9 are the cutover -- land together or behind a feature flag.

1. Add libsodium to vcpkg.json; verify CI still builds
2. Implement pak_tool (keygen, pack, verify, list, extract)
3. Implement AssetLoader (asset_loader.h/cpp, AssetConfig, AssetView)
4. Generate keypair; commit public key; add private key to CI secrets
5. Replace load_beat_map / LoadMusicStream direct calls with load_asset()
6. Update CMake: content copy POST_BUILD -> pak POST_BUILD
7. Update CI workflows: sign step + upload content.pak
8. Update run.sh: append --dev flag
9. Remove loose-file copy CMake rules (content.pak is the only runtime path)

---

## Open Questions

- Bundle fonts into the same PAK or keep separate?
  Recommendation: same PAK, one file to ship.

- Emscripten: swap --preload-file content@/content for
  --preload-file content.pak (simpler, single file).

- Compression: the reserved 8-byte TOC field can carry a compression_type
  flag (0=none, 1=zstd) with no format version bump. Add when WAV count
  grows and artifact size becomes a concern.
