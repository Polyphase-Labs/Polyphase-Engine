#!/bin/bash
# ==========================================================================
#  build_dmg_mac.sh
#  Builds a compressed .dmg (with an Applications shortcut) around
#  dist/Polyphase.app. Runs build_app_mac.sh first unless SKIP_APP=1.
#
#  Environment (optional):
#    MAC_SIGN_IDENTITY   passed through to build_app_mac.sh; also signs the dmg
#    SKIP_APP=1          reuse an existing dist/Polyphase.app
#
#  Usage: bash Installers/build_dmg_mac.sh
# ==========================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

echo "============================================"
echo " Polyphase Engine - macOS DMG Builder"
echo "============================================"
echo ""

if [ "${SKIP_APP:-0}" != "1" ]; then
    echo "[1/3] Building app bundle..."
    bash Installers/build_app_mac.sh
else
    echo "[1/3] Reusing dist/Polyphase.app (SKIP_APP=1)"
fi
echo ""

if [ ! -d dist/Polyphase.app ]; then
    echo "ERROR: dist/Polyphase.app not found." >&2
    exit 1
fi

VERSION="$(sed -n 's/^Version=//p' dist/Editor/version.txt | tr -d '[:space:]')"
VERSION_STRING="$(sed -n 's/^#define POLYPHASE_VERSION_STRING "\(.*\)"/\1/p' Engine/Source/Engine/Constants.h | head -n1)"
[ -z "$VERSION_STRING" ] && VERSION_STRING="$VERSION"
DMG="dist/PolyphaseEditor-${VERSION_STRING}-macos-arm64.dmg"
DMGROOT="dist/dmgroot"

echo "[2/3] Assembling image root..."
rm -rf "$DMGROOT" "$DMG"
mkdir -p "$DMGROOT"
cp -R dist/Polyphase.app "$DMGROOT/"
ln -s /Applications "$DMGROOT/Applications"

echo "[3/3] Creating ${DMG}..."
# hdiutil occasionally fails with "Resource busy" on CI; retry a few times.
for attempt in 1 2 3; do
    if hdiutil create -volname "Polyphase Engine" -srcfolder "$DMGROOT" -ov -format UDZO "$DMG"; then
        break
    fi
    if [ "$attempt" -eq 3 ]; then
        echo "ERROR: hdiutil create failed" >&2
        exit 1
    fi
    sleep 2
done
rm -rf "$DMGROOT"

if [ -n "${MAC_SIGN_IDENTITY:-}" ] && [ "${MAC_SIGN_IDENTITY}" != "-" ]; then
    codesign --force --sign "$MAC_SIGN_IDENTITY" --timestamp "$DMG"
fi

echo ""
echo "============================================"
echo " DMG built successfully!"
echo " Output: $DMG"
echo ""
echo " Install: open the image and drag Polyphase.app to Applications."
echo " First launch of an ad-hoc signed build: right-click > Open, or"
echo "   xattr -dr com.apple.quarantine /Applications/Polyphase.app"
echo "============================================"
