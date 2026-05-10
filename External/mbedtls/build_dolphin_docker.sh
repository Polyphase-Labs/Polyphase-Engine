#!/usr/bin/env bash
# Cross-compile mbedTLS for Wii / GameCube inside a devkitpro/devkitppc
# Docker container. Wrap this around build_dolphin.sh when running natively
# is fighting you (msys2 Python tooling, rpds-py rust deps, mbedTLS framework
# project-root quirks, etc.).
#
# Prereq: Docker Desktop installed and running. Works from msys2, PowerShell,
# WSL, native Linux, or macOS.
#
# Usage:
#   ./build_dolphin_docker.sh wii         # → External/mbedtls/lib-wii/
#   ./build_dolphin_docker.sh gcn         # → External/mbedtls/lib-gcn/
#   ./build_dolphin_docker.sh both        # → both
#
# Output archives land back in lib-wii/ / lib-gcn/ on the host filesystem
# (the container's /work mount maps to External/mbedtls/), so they're
# immediately picked up by Standalone/Makefile_Wii / Makefile_GCN.

set -euo pipefail

TARGET="${1:-wii}"

if ! command -v docker >/dev/null 2>&1; then
    echo "docker not found. Install Docker Desktop and make sure the engine is running." >&2
    exit 1
fi

cd "$(dirname "$0")"

# 1. Sanity check: vendoring should already be done. The Docker container
#    can't help if the source tree isn't present on the host.
if [ ! -d library ] || [ ! -d include/mbedtls ] || [ ! -d framework/scripts/mbedtls_framework ]; then
    echo "mbedTLS source not fully vendored. Follow Step 1 in README.md first." >&2
    exit 2
fi

# 2. Build / refresh the builder image. Cached on subsequent runs.
IMAGE_TAG="polyphase-mbedtls-builder:latest"
docker build -f Dockerfile.dolphin -t "$IMAGE_TAG" .

# 3. Resolve the host path that maps into /work. On Windows + Docker Desktop
#    the mount needs a Windows-style path (C:/...), but msys2 reports POSIX
#    (/m/...). Detect and translate.
HOST_DIR="$(pwd)"
if command -v cygpath >/dev/null 2>&1; then
    HOST_DIR="$(cygpath -m "$HOST_DIR")"
fi

# 4. Pick targets.
case "$TARGET" in
    wii|gcn) TARGETS="$TARGET" ;;
    both)    TARGETS="wii gcn" ;;
    *)
        echo "usage: $0 [wii|gcn|both]" >&2
        exit 3
        ;;
esac

# 5. Run the existing build_dolphin.sh inside the container. The container
#    has python/perl/jsonschema/jinja2 + devkitPPC pre-installed.
for t in $TARGETS; do
    echo "==> Building mbedTLS for $t in container..."
    docker run --rm \
        -v "$HOST_DIR:/work" \
        -w /work \
        "$IMAGE_TAG" \
        bash -c "chmod +x build_dolphin.sh && ./build_dolphin.sh $t"
done

echo ""
echo "Done."
for t in $TARGETS; do
    if [ -d "lib-$t" ]; then
        echo "  lib-$t/:"
        ls -la "lib-$t/"
    fi
done
