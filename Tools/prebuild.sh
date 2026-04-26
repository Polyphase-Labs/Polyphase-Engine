#!/bin/bash
# Master prebuild script for Linux.
# Runs all prebuild steps needed before building.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

echo "============================================"
echo " Polyphase Prebuild (Linux)"
echo "============================================"
echo ""

# --- libgit2 ---
echo "[1/3] Building libgit2..."
bash "$SCRIPT_DIR/prebuild_libgit2.sh"
echo ""

# --- Shaders ---
echo "[2/3] Compiling shaders..."
pushd "$REPO_ROOT/Engine/Shaders/GLSL" > /dev/null
bash compile.sh
popd > /dev/null
echo ""

# --- Standalone embedded asset stubs ---
echo "[3/3] Generating Standalone embedded asset stubs..."
python3 "$SCRIPT_DIR/generate_embedded_stubs.py"
echo ""

echo "============================================"
echo " Prebuild complete."
echo "============================================"
