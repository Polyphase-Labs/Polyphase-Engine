#!/bin/bash
# Prebuild libgit2 static library for the Linux Makefile build path.
# Run this once after cloning or updating the libgit2 submodule.
#
# Alternatively: sudo apt install libgit2-dev

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."
LIBGIT2_DIR="$REPO_ROOT/Engine/External/libgit2"

cd "$LIBGIT2_DIR"

echo "[libgit2] Configuring..."
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_CLI=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DUSE_SSH=OFF \
    -DUSE_BUNDLED_ZLIB=ON \
    -DREGEX_BACKEND=builtin

echo "[libgit2] Building..."
cmake --build build --target libgit2package -- -j$(nproc)

echo ""
echo "[libgit2] Done. Static library at:"
echo "  $LIBGIT2_DIR/build/libgit2.a"
echo ""
echo "To use with Makefile_Linux, either:"
echo "  1. Copy: sudo cp $LIBGIT2_DIR/build/libgit2.a /usr/local/lib/"
echo "  2. Or install system package: sudo apt install libgit2-dev"
