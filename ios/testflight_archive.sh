#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-ios}"
ARCHIVE_PATH="${ARCHIVE_PATH:-$BUILD_DIR/archive/SHAPESHIFTER.xcarchive}"
EXPORT_DIR="${EXPORT_DIR:-$BUILD_DIR/export}"
EXPORT_OPTIONS_PLIST="${EXPORT_OPTIONS_PLIST:-$BUILD_DIR/ExportOptions-AppStore.plist}"

TEAM_ID="${TEAM_ID:-}"
BUNDLE_ID="${BUNDLE_ID:-com.yashasg.shapeshifter}"
BUILD_NUMBER="${BUILD_NUMBER:-}"
MARKETING_VERSION="${MARKETING_VERSION:-0.1.0}"
ALLOW_PROVISIONING_UPDATES="${ALLOW_PROVISIONING_UPDATES:-1}"
VCPKG_TRIPLET="${VCPKG_TRIPLET:-arm64-ios}"

MODE="${1:-all}"

usage() {
    cat <<'USAGE'
Usage: ios/testflight_archive.sh [preflight|configure|archive|export|all]

Environment:
  TEAM_ID             Apple Developer Team ID (required for configure/archive/export)
  BUNDLE_ID           Bundle ID (default: com.yashasg.shapeshifter)
  BUILD_NUMBER        Monotonic CFBundleVersion integer (required for configure/archive/export)
  MARKETING_VERSION   CFBundleShortVersionString (default: 0.1.0)
  BUILD_DIR           Xcode build dir (default: build-ios)
  ARCHIVE_PATH        xcarchive path (default: build-ios/archive/SHAPESHIFTER.xcarchive)
  EXPORT_DIR          IPA export dir (default: build-ios/export)
USAGE
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "ERROR: Missing required command: $1" >&2
        exit 1
    fi
}

require_signing_inputs() {
    local missing=0
    if [[ -z "$TEAM_ID" ]]; then
        echo "BLOCKER: TEAM_ID is required (owner: yashasg)." >&2
        missing=1
    fi
    if [[ -z "$BUILD_NUMBER" ]]; then
        echo "BLOCKER: BUILD_NUMBER is required and must monotonically increase (owner: yashasg)." >&2
        missing=1
    elif ! [[ "$BUILD_NUMBER" =~ ^[0-9]+$ ]]; then
        echo "ERROR: BUILD_NUMBER must be an integer, got: $BUILD_NUMBER" >&2
        missing=1
    fi
    if [[ "$missing" -ne 0 ]]; then
        exit 2
    fi
}

preflight() {
    require_cmd cmake
    require_cmd xcodebuild
    require_cmd xcrun
    if [[ "$(uname -s)" != "Darwin" ]]; then
        echo "ERROR: iOS archive workflow requires macOS (Darwin)." >&2
        exit 1
    fi

    echo "== Toolchain =="
    xcodebuild -version
    cmake --version | head -n 1

    local blockers=0
    [[ -n "$TEAM_ID" ]] || { echo "- BLOCKER: TEAM_ID missing (owner: yashasg)"; blockers=1; }
    [[ -n "$BUILD_NUMBER" ]] || { echo "- BLOCKER: BUILD_NUMBER missing (owner: yashasg)"; blockers=1; }
    local vcpkg_root="${VCPKG_ROOT:-}"
    if [[ -z "$vcpkg_root" && -d "$HOME/vcpkg" ]]; then
        vcpkg_root="$HOME/vcpkg"
    fi
    if [[ -z "$vcpkg_root" ]]; then
        echo "- BLOCKER: VCPKG_ROOT not set and \$HOME/vcpkg not found (owner: build machine)."
        blockers=1
    elif [[ ! -f "$vcpkg_root/scripts/buildsystems/vcpkg.cmake" ]]; then
        echo "- BLOCKER: vcpkg toolchain file not found under VCPKG_ROOT (owner: build machine)."
        blockers=1
    elif [[ ! -x "$vcpkg_root/vcpkg" ]]; then
        echo "- BLOCKER: vcpkg executable not found under VCPKG_ROOT (owner: build machine)."
        blockers=1
    fi
    echo "- BLOCKER: Apple Developer account must be signed into Xcode on this machine (owner: yashasg)."
    echo "- BLOCKER: Bundle ID '$BUNDLE_ID' must exist in Apple Developer identifiers (owner: yashasg)."
    echo "- BLOCKER: Signing certificate/provisioning must resolve via Automatic Signing (owner: yashasg)."

    if [[ "$blockers" -eq 0 ]]; then
        echo "Preflight inputs present for automated configure/archive/export."
    fi
}

ensure_ios_triplet_dependencies() {
    local vcpkg_root="${VCPKG_ROOT:-}"
    if [[ -z "$vcpkg_root" && -d "$HOME/vcpkg" ]]; then
        vcpkg_root="$HOME/vcpkg"
    fi
    local vcpkg_exe="$vcpkg_root/vcpkg"
    if [[ ! -x "$vcpkg_exe" ]]; then
        echo "ERROR: Missing vcpkg executable: $vcpkg_exe" >&2
        exit 2
    fi

    (
        cd "$ROOT_DIR"
        "$vcpkg_exe" install \
            --triplet "$VCPKG_TRIPLET" \
            --overlay-ports="$ROOT_DIR/vcpkg-overlay" \
            --allow-unsupported
    )
}

configure_ios() {
    require_signing_inputs
    local vcpkg_root="${VCPKG_ROOT:-}"
    if [[ -z "$vcpkg_root" && -d "$HOME/vcpkg" ]]; then
        vcpkg_root="$HOME/vcpkg"
    fi
    if [[ -z "$vcpkg_root" ]]; then
        echo "ERROR: VCPKG_ROOT is not set and \$HOME/vcpkg was not found." >&2
        exit 2
    fi
    local vcpkg_toolchain="$vcpkg_root/scripts/buildsystems/vcpkg.cmake"
    if [[ ! -f "$vcpkg_toolchain" ]]; then
        echo "ERROR: Missing vcpkg toolchain file: $vcpkg_toolchain" >&2
        exit 2
    fi
    ensure_ios_triplet_dependencies
    if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        rm -f "$BUILD_DIR/CMakeCache.txt"
        rm -rf "$BUILD_DIR/CMakeFiles"
    fi
    (
        cd "$ROOT_DIR"
        cmake -B "$BUILD_DIR" -S . \
            -G Xcode \
            -DCMAKE_SYSTEM_NAME=iOS \
            -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
            -DCMAKE_TOOLCHAIN_FILE="$vcpkg_toolchain" \
            -DVCPKG_OVERLAY_PORTS="$ROOT_DIR/vcpkg-overlay" \
            -DVCPKG_TARGET_TRIPLET="$VCPKG_TRIPLET" \
            -DVCPKG_MANIFEST_INSTALL=ON \
            -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$TEAM_ID" \
            -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic \
            -DCMAKE_XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER="$BUNDLE_ID" \
            -DCMAKE_XCODE_ATTRIBUTE_MARKETING_VERSION="$MARKETING_VERSION" \
            -DCMAKE_XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION="$BUILD_NUMBER"
    )
}

archive_ios() {
    require_signing_inputs
    local provisioning_args=()
    if [[ "$ALLOW_PROVISIONING_UPDATES" == "1" ]]; then
        provisioning_args+=(-allowProvisioningUpdates)
    fi

    xcodebuild \
        -project "$BUILD_DIR/shapeshifter.xcodeproj" \
        -scheme shapeshifter \
        -configuration Release \
        -destination generic/platform=iOS \
        -archivePath "$ARCHIVE_PATH" \
        DEVELOPMENT_TEAM="$TEAM_ID" \
        PRODUCT_BUNDLE_IDENTIFIER="$BUNDLE_ID" \
        MARKETING_VERSION="$MARKETING_VERSION" \
        CURRENT_PROJECT_VERSION="$BUILD_NUMBER" \
        "${provisioning_args[@]}" \
        clean archive
}

write_export_options() {
    mkdir -p "$(dirname "$EXPORT_OPTIONS_PLIST")"
    cat >"$EXPORT_OPTIONS_PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>destination</key>
    <string>export</string>
    <key>method</key>
    <string>app-store</string>
    <key>signingStyle</key>
    <string>automatic</string>
    <key>teamID</key>
    <string>$TEAM_ID</string>
    <key>uploadSymbols</key>
    <true/>
</dict>
</plist>
EOF
}

export_ipa() {
    require_signing_inputs
    if [[ ! -d "$ARCHIVE_PATH" ]]; then
        echo "ERROR: Archive not found at $ARCHIVE_PATH. Run configure+archive first." >&2
        exit 1
    fi
    write_export_options
    xcodebuild -exportArchive \
        -archivePath "$ARCHIVE_PATH" \
        -exportOptionsPlist "$EXPORT_OPTIONS_PLIST" \
        -exportPath "$EXPORT_DIR"
    echo "IPA export complete: $EXPORT_DIR"
}

case "$MODE" in
    preflight)
        preflight
        ;;
    configure)
        preflight
        configure_ios
        ;;
    archive)
        preflight
        configure_ios
        archive_ios
        ;;
    export)
        preflight
        export_ipa
        ;;
    all)
        preflight
        configure_ios
        archive_ios
        export_ipa
        ;;
    -h|--help|help)
        usage
        ;;
    *)
        echo "ERROR: Unknown mode '$MODE'" >&2
        usage
        exit 1
        ;;
esac
