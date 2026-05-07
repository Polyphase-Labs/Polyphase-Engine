#!/usr/bin/env bash
# Drive a headless Polyphase editor build of an external project for one
# platform, then verify the expected artifact landed under
# <project>/Packaged/<Platform>/.
#
# Usage:
#   bash Tools/CI/verify_project_build.sh <editor-binary> <project-dir> <platform>
#
# <platform> ∈ { Windows, Linux, Wii, GameCube, 3DS }
#
# Exit codes:
#   0  build succeeded and artifact is present
#   1  build failed or artifact missing/empty
#   2  bad arguments / pre-flight failure

set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "Usage: $0 <editor-binary> <project-dir> <platform>" >&2
  exit 2
fi

EDITOR="$1"
PROJECT_DIR="$2"
PLATFORM="$3"

# Resolve absolute paths so we don't depend on $PWD across nested make calls.
EDITOR_ABS="$(cd "$(dirname "$EDITOR")" && pwd)/$(basename "$EDITOR")"
PROJECT_ABS="$(cd "$PROJECT_DIR" && pwd)"

case "$PLATFORM" in
  Windows)  EXT=".exe"  ;;
  Linux)    EXT=".elf"  ;;
  Wii)      EXT=".dol"  ;;
  GameCube) EXT=".dol"  ;;
  3DS)      EXT=".3dsx" ;;
  *)
    echo "Unknown platform '$PLATFORM' (expected: Windows, Linux, Wii, GameCube, 3DS)" >&2
    exit 2
    ;;
esac

# Pre-flight checks.
if [[ ! -x "$EDITOR_ABS" ]]; then
  echo "ERROR: editor binary not executable: $EDITOR_ABS" >&2
  exit 2
fi
if [[ ! -d "$PROJECT_ABS" ]]; then
  echo "ERROR: project directory not found: $PROJECT_ABS" >&2
  exit 2
fi
if ! ls "$PROJECT_ABS"/*.octp >/dev/null 2>&1; then
  echo "ERROR: no .octp project file found in $PROJECT_ABS" >&2
  exit 2
fi

echo "==> verify_project_build"
echo "    editor:   $EDITOR_ABS"
echo "    project:  $PROJECT_ABS"
echo "    platform: $PLATFORM"
echo "    expect:   Packaged/$PLATFORM/*$EXT"
echo

# Capture editor output so we can dump the tail on failure.
LOG_FILE="$(mktemp -t polyphase-verify-XXXXXX.log)"
trap 'rm -f "$LOG_FILE"' EXIT

set +e
"$EDITOR_ABS" -headless -project "$PROJECT_ABS" -build "$PLATFORM" embedded \
  2>&1 | tee "$LOG_FILE"
EDITOR_RC=${PIPESTATUS[0]}
set -e

if [[ $EDITOR_RC -ne 0 ]]; then
  echo
  echo "FAIL: editor exited with code $EDITOR_RC building $PLATFORM" >&2
  echo "----- last 200 lines of editor output -----" >&2
  tail -n 200 "$LOG_FILE" >&2 || true
  exit 1
fi

PACKAGED_DIR="$PROJECT_ABS/Packaged/$PLATFORM"
if [[ ! -d "$PACKAGED_DIR" ]]; then
  echo "FAIL: $PACKAGED_DIR was not created by the build." >&2
  exit 1
fi

# Locate a non-empty artifact with the expected extension.
ARTIFACT="$(find "$PACKAGED_DIR" -maxdepth 2 -type f -name "*$EXT" -size +0c -print -quit)"
if [[ -z "$ARTIFACT" ]]; then
  echo "FAIL: no non-empty *$EXT artifact found in $PACKAGED_DIR" >&2
  echo "----- contents of $PACKAGED_DIR -----" >&2
  ls -la "$PACKAGED_DIR" >&2 || true
  exit 1
fi

ARTIFACT_SIZE=$(stat -c%s "$ARTIFACT" 2>/dev/null || stat -f%z "$ARTIFACT")
echo
echo "OK: $ARTIFACT ($ARTIFACT_SIZE bytes)"
