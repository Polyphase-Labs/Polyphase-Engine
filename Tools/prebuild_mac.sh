#!/bin/bash
# Master prebuild script for macOS.
# Runs all prebuild steps needed before building with Makefile_Mac_Editor.
#
# Prerequisites (see Documentation/Development/SetupEnvironment/Mac.md):
#   - Xcode Command Line Tools      (xcode-select --install)
#   - Homebrew cmake + python3      (brew install cmake python3)
#   - LunarG Vulkan SDK for macOS   (VULKAN_SDK must point at ~/VulkanSDK/<ver>/macOS)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

echo "============================================"
echo " Polyphase Prebuild (macOS)"
echo "============================================"
echo ""

if [ -z "$VULKAN_SDK" ] || [ ! -x "$VULKAN_SDK/bin/glslc" ]; then
    echo "ERROR: VULKAN_SDK is not set or does not contain bin/glslc."
    echo "       Install the LunarG Vulkan SDK for macOS and export"
    echo "       VULKAN_SDK=\$HOME/VulkanSDK/<version>/macOS (or source"
    echo "       \$HOME/VulkanSDK/<version>/setup-env.sh)."
    echo "       See Documentation/Development/SetupEnvironment/Mac.md"
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: cmake not found. Install it with: brew install cmake"
    exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
    echo "ERROR: Xcode Command Line Tools not found. Run: xcode-select --install"
    exit 1
fi

# --- libgit2 ---
echo "[1/3] Building libgit2..."
bash "$SCRIPT_DIR/prebuild_libgit2.sh"
echo ""

# --- Shaders ---
echo "[2/3] Compiling shaders..."
(cd "$REPO_ROOT/Engine/Shaders/GLSL" && bash compile.sh)
echo ""

# --- Standalone embedded asset stubs ---
echo "[3/3] Generating Standalone embedded asset stubs..."
python3 "$SCRIPT_DIR/generate_embedded_stubs.py"
echo ""

echo "============================================"
echo " Prebuild complete."
echo "============================================"
