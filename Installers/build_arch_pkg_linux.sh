#!/bin/bash
# Builds an Arch Linux package (.pkg.tar.zst) from the release tarball.
#
# Must run on Arch (needs makepkg) as a non-root user (makepkg refuses root).
# Usage: bash Installers/build_arch_pkg_linux.sh [path/to/PolyphaseEditor-linux-x64.tar.gz]
# Default tarball: dist/PolyphaseEditor-linux-x64.tar.gz (build_tarball_linux.sh output).
# Output: dist/polyphase-editor-<pkgver>-1-x86_64.pkg.tar.zst

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

TARBALL="$(realpath "${1:-dist/PolyphaseEditor-linux-x64.tar.gz}")"

echo "============================================"
echo " Polyphase Engine - Arch Package Builder"
echo "============================================"
echo ""

if ! command -v makepkg > /dev/null 2>&1; then
    echo "ERROR: makepkg not found. Run this on Arch Linux (pacman -S base-devel)."
    exit 1
fi
if [ "$(id -u)" -eq 0 ]; then
    echo "ERROR: makepkg refuses to run as root. Run as a regular user."
    exit 1
fi
if [ ! -f "$TARBALL" ]; then
    echo "ERROR: tarball not found: $TARBALL"
    echo "       Build it first with: bash Installers/build_tarball_linux.sh"
    exit 1
fi

# version.txt only carries the major POLYPHASE_VERSION (the Windows installer
# reads it), so take the full string from Constants.h. pkgver may not contain
# '-', so 6.2.0-beta.18.5 becomes 6.2.0_beta.18.5 (still sorts below 6.2.0).
VERSION="$(grep -oP '#define\s+POLYPHASE_VERSION_STRING\s+"\K[^"]+' Engine/Source/Engine/Constants.h)"
if [ -z "$VERSION" ]; then
    echo "ERROR: POLYPHASE_VERSION_STRING not found in Engine/Source/Engine/Constants.h"
    exit 1
fi
PKGVER="${VERSION//-/_}"

BUILD_DIR="dist/arch-pkg"
echo "[1/2] Preparing PKGBUILD (version ${VERSION} -> pkgver ${PKGVER})..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
sed "s/@PKGVER@/${PKGVER}/" Installers/Linux/PKGBUILD.in > "$BUILD_DIR/PKGBUILD"
ln -s "$TARBALL" "$BUILD_DIR/PolyphaseEditor-linux-x64.tar.gz"

echo "[2/2] Running makepkg..."
# -d: runtime deps are only needed to run the editor, not to repackage it.
# C.UTF-8: containers default to the C locale, where bsdtar can't encode the
# non-ASCII filenames in the staged tree and warns while compressing.
(cd "$BUILD_DIR" && LANG=C.UTF-8 PKGDEST="$REPO_ROOT/dist" makepkg -f -d --noconfirm)

PKG_FILE="$(ls dist/polyphase-editor-"${PKGVER}"-*-x86_64.pkg.tar.zst)"

echo ""
echo "============================================"
echo " Arch package built successfully!"
echo " Output: $PKG_FILE"
echo ""
echo " Install:   sudo pacman -U $(basename "$PKG_FILE")"
echo " Uninstall: sudo pacman -R polyphase-editor"
echo "============================================"
