#!/bin/bash
# Prebuild libgit2 static library for the Linux / macOS Makefile build paths.
# Run this once after cloning or updating the libgit2 submodule.
#
# Alternatively: sudo apt install libgit2-dev

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."
LIBGIT2_DIR="$REPO_ROOT/Engine/External/libgit2"

cd "$LIBGIT2_DIR"

# Job count: nproc is GNU-only; macOS uses sysctl.
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

# macOS: build arm64 against the same deployment target as Makefile_Mac, and
# use the system TLS stack (SecureTransport + CommonCrypto) so the editor links
# with -framework Security instead of needing OpenSSL. libgit2 on macOS also
# picks iconv for path normalisation, hence -liconv in Makefile_Mac_Editor.
EXTRA_CMAKE_ARGS=""
if [ "$(uname -s)" = "Darwin" ]; then
    EXTRA_CMAKE_ARGS="-DUSE_HTTPS=SecureTransport -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0"
fi

# Wipe any pre-existing build/ first. A stale CMakeCache.txt (e.g. from a
# host build that leaked into a Docker image via COPY, or a host-side build
# from a different source path) makes cmake abort with
#   "The current CMakeCache.txt directory ... is different than the
#    directory ... where CMakeCache.txt was created"
rm -rf build

echo "[libgit2] Configuring..."
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_CLI=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DUSE_SSH=OFF \
    -DUSE_BUNDLED_ZLIB=ON \
    -DREGEX_BACKEND=builtin \
    $EXTRA_CMAKE_ARGS

echo "[libgit2] Building..."
cmake --build build --target libgit2package -- -j"$JOBS"

echo ""
echo "[libgit2] Done. Static library at:"
echo "  $LIBGIT2_DIR/build/libgit2.a"
echo ""
echo "To use with Makefile_Linux / Makefile_Mac, either:"
echo "  1. Copy: sudo cp $LIBGIT2_DIR/build/libgit2.a /usr/local/lib/"
echo "  2. Or install system package: sudo apt install libgit2-dev"
