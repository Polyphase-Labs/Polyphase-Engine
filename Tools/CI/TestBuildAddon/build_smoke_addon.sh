#!/usr/bin/env bash
# CI smoke-test addon build (Linux).
#
# Mirrors the g++ flow in NativeAddonManager::GenerateBuildScript so the
# link surface matches what an installed-editor addon build would use.
# Fails non-zero if any engine symbol referenced by the fixture is missing
# from the staged distribution.
#
# Usage:
#   Tools/CI/TestBuildAddon/build_smoke_addon.sh [dist-dir]
#
# [dist-dir] defaults to dist/Editor (matches Installers/stage_distribution.py).

set -euo pipefail

DIST_DIR="${1:-dist/Editor}"

if [ ! -f "$DIST_DIR/lib/libLua.a" ]; then
    echo "ERROR: $DIST_DIR/lib/libLua.a not found." >&2
    echo "       Run Installers/stage_distribution.py --platform linux first." >&2
    exit 2
fi

if [ ! -f "$DIST_DIR/PolyphaseEditor" ] && [ ! -f "$DIST_DIR/PolyphaseEditor.elf" ]; then
    echo "ERROR: no editor binary found under $DIST_DIR/." >&2
    exit 2
fi

ADDON_ROOT="$(cd "$(dirname "$0")/com.polyphase.smoke.material" && pwd)"
OUT_DIR="$(cd "$(dirname "$0")" && pwd)/Build/Linux"
mkdir -p "$OUT_DIR"

echo "Building CI smoke addon against $DIST_DIR"

# Linux's POLYPHASE_API resolves to __attribute__((visibility("default")))
# which is a no-op when the engine isn't built with -fvisibility=hidden, so
# Linux can't catch a "missing POLYPHASE_API" regression via link failure
# the way Windows can. We still build the fixture: it catches signature
# drift and the broader "the headers compile against the staged tree"
# class of breakage. Symbol resolution at addon load time relies on the
# editor exe being linked with -rdynamic (see Standalone/Makefile_Linux_Editor).
g++ -shared -fPIC -O2 -std=c++17 \
    -DEDITOR=1 -DLUA_ENABLED=1 -DGLM_FORCE_RADIANS \
    -DPLATFORM_LINUX=1 -DAPI_VULKAN=1 \
    -DOCTAVE_PLUGIN_EXPORT -DPOLYPHASE_SMOKE_MATERIAL_EXPORT \
    -I"$DIST_DIR/Engine/Source" \
    -I"$DIST_DIR/Engine/Source/Engine" \
    -I"$DIST_DIR/Engine/Source/Plugins" \
    -I"$DIST_DIR/External" \
    -I"$DIST_DIR/External/Lua" \
    -I"$DIST_DIR/External/glm" \
    -I"$DIST_DIR/External/Imgui" \
    -I"$DIST_DIR/External/ImGuizmo" \
    -I"$DIST_DIR/External/Bullet" \
    -I"$DIST_DIR/External/Vorbis" \
    -I"$DIST_DIR/External/Assimp" \
    -I"$ADDON_ROOT/Source" \
    "$ADDON_ROOT/Source/SmokeMaterial.cpp" \
    -o "$OUT_DIR/libcom.polyphase.smoke.material.so" \
    -L"$DIST_DIR/lib" \
    -lLua \
    -Wl,--unresolved-symbols=ignore-in-shared-libs

# Note on --unresolved-symbols=ignore-in-shared-libs:
# The addon will be dlopen'd into the editor process at runtime, so engine
# symbols (MaterialBase, Renderer, AssetManager, IsHeadless) are expected
# to be undefined at addon-link time and resolved against the running exe
# via -rdynamic at load time. This is the same model used by every other
# Polyphase native addon — see NativeAddonManager::GenerateBuildScript's
# Linux branch.

echo "[OK] CI smoke addon linked against $DIST_DIR"
