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
echo "[1/4] Building libgit2..."
bash "$SCRIPT_DIR/prebuild_libgit2.sh"
echo ""

# --- Shaders ---
echo "[2/4] Compiling shaders..."
(cd "$REPO_ROOT/Engine/Shaders/GLSL" && bash compile.sh)
echo ""

# --- Standalone embedded asset stubs ---
echo "[3/4] Generating Standalone embedded asset stubs..."
python3 "$SCRIPT_DIR/generate_embedded_stubs.py"
echo ""

# --- Scripting API from the Lua bindings (C# reference assembly + LuaLS stubs) ---
echo "[4/4] Generating scripting API from Lua bindings..."
python3 "$SCRIPT_DIR/generate_csharp_api.py"
python3 "$SCRIPT_DIR/generate_lua_stubs.py"
echo ""

echo "============================================"
echo " Prebuild complete."
echo "============================================"
