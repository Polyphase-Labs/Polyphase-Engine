#!/usr/bin/env bash
# One-shot local replica of the verify-test-project CI matrix for game-build
# verification. Assumes the Polyphase editor is already built — does NOT
# rebuild it. Installs missing devkitPro toolchains (with sudo), clones the
# configured test project, runs the headless game build for each requested
# platform, and cleans up.
#
# Usage:
#   bash Tools/CI/TestBuildProject/run_local_verify.sh [options]
#
# Options:
#   --platforms <list>     Comma list of: Linux,Wii,GameCube,3DS  (default: all)
#   --repo <url>           Test project git URL  (default: https://github.com/mholtkamp/octo-bombers)
#   --subdir <name>        Project subdir inside clone  (default: Bomber)
#   --dest <dir>           Where to clone   (default: ./test-project)
#   --editor <path>        Editor binary path. If omitted, auto-locates in this order:
#                            1. PolyphaseEditor.elf at solution root (post-build copy)
#                            2. Standalone/Build/Linux/PolyphaseEditor.elf
#   --skip-toolchain       Don't try to install devkitPro (assume it's set up)
#   --skip-cleanup         Leave the clone in place for inspection after the run
#   -h, --help             Show this message
#
# Build the Linux editor first if you don't have one:
#   make -C Standalone -f Makefile_Linux_Editor -j$(nproc)
#
# Exit code: 0 on full success; non-zero if any platform failed (still attempts
# every platform to surface independent failures).

set -uo pipefail

# Anchor at the repo root so relative paths work regardless of $PWD.
# This script lives at Tools/CI/TestBuildProject/, so the repo root is three levels up.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
TOOLS_CI="$REPO_ROOT/Tools/CI"
cd "$REPO_ROOT"

PLATFORMS="Linux,Wii,GameCube,3DS"
REPO="https://github.com/mholtkamp/octo-bombers"
SUBDIR="Bomber"
DEST="./test-project"
# EDITOR resolved later (after arg parsing) so --editor takes precedence over auto-locate.
EDITOR=""
SKIP_TOOLCHAIN=0
SKIP_CLEANUP=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --platforms)        PLATFORMS="$2"; shift 2 ;;
    --platforms=*)      PLATFORMS="${1#*=}"; shift ;;
    --repo)             REPO="$2"; shift 2 ;;
    --repo=*)           REPO="${1#*=}"; shift ;;
    --subdir)           SUBDIR="$2"; shift 2 ;;
    --subdir=*)         SUBDIR="${1#*=}"; shift ;;
    --dest)             DEST="$2"; shift 2 ;;
    --dest=*)           DEST="${1#*=}"; shift ;;
    --editor)           EDITOR="$2"; shift 2 ;;
    --editor=*)         EDITOR="${1#*=}"; shift ;;
    --skip-toolchain)   SKIP_TOOLCHAIN=1; shift ;;
    --skip-cleanup)     SKIP_CLEANUP=1; shift ;;
    -h|--help)          sed -n '2,27p' "$0"; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; exit 2 ;;
  esac
done

# --- 1. Resolve / verify the editor binary ----------------------------------
# Search order (only if --editor wasn't supplied):
#   1. PolyphaseEditor.elf at solution root (post-build copy)
#   2. Standalone/Build/Linux/PolyphaseEditor.elf (raw build output)
if [[ -z "$EDITOR" ]]; then
  for candidate in \
    "PolyphaseEditor.elf" \
    "Standalone/Build/Linux/PolyphaseEditor.elf"
  do
    if [[ -x "$candidate" ]]; then
      EDITOR="$candidate"
      break
    fi
  done
fi

if [[ -z "$EDITOR" ]]; then
  echo "ERROR: no editor binary found. Looked in:" >&2
  echo "         PolyphaseEditor.elf (solution root)" >&2
  echo "         Standalone/Build/Linux/PolyphaseEditor.elf" >&2
  echo "       Build it first:  make -C Standalone -f Makefile_Linux_Editor -j\$(nproc)" >&2
  echo "       Or pass --editor <path> to point at an existing binary." >&2
  exit 1
fi
if [[ ! -x "$EDITOR" ]]; then
  echo "ERROR: editor binary not found or not executable: $EDITOR" >&2
  exit 1
fi
echo "Using editor: $EDITOR"

# --- 2. Determine which devkit toolchains the requested platforms need ------

NEED_PPC=0
NEED_ARM=0
IFS=',' read -ra PLAT_ARR <<<"$PLATFORMS"
for p in "${PLAT_ARR[@]}"; do
  case "$p" in
    Linux)              ;;
    Windows)            ;;
    Wii|GameCube)       NEED_PPC=1 ;;
    3DS)                NEED_ARM=1 ;;
    "")                 ;;
    *) echo "ERROR: unknown platform '$p' in --platforms" >&2; exit 2 ;;
  esac
done

# --- 3. Install missing toolchains (with sudo) ------------------------------

install_args=()
if [[ $SKIP_TOOLCHAIN -eq 0 ]]; then
  if [[ $NEED_PPC -eq 1 && ! -x /opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc ]]; then
    install_args+=("ppc")
  fi
  if [[ $NEED_ARM -eq 1 && ! -x /opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc ]]; then
    install_args+=("arm")
  fi
fi

if [[ ${#install_args[@]} -gt 0 ]]; then
  joined="$(IFS=,; echo "${install_args[*]}")"
  echo "==> Installing devkitPro toolchains: $joined  (sudo required)"
  sudo bash "$TOOLS_CI/install_devkitpro.sh" --platforms "$joined"
fi

# Surface env vars for any make invocations the editor shells out to.
[[ -d /opt/devkitpro ]]            && export DEVKITPRO=/opt/devkitpro
[[ -d /opt/devkitpro/devkitPPC ]]  && export DEVKITPPC=/opt/devkitpro/devkitPPC
[[ -d /opt/devkitpro/devkitARM ]]  && export DEVKITARM=/opt/devkitpro/devkitARM

# --- 4. Clone the test project ----------------------------------------------

echo "==> Cloning $REPO -> $DEST"
bash "$SCRIPT_DIR/clone_test_project.sh" "$REPO" "$DEST"

PROJECT_DIR="$DEST/$SUBDIR"
if [[ ! -d "$PROJECT_DIR" ]]; then
  echo "ERROR: project directory not found: $PROJECT_DIR" >&2
  echo "       Check --subdir or the repo's layout." >&2
  exit 1
fi

# --- 5. Run each platform; aggregate failures so we still try every one -----

FAILED=()
for p in "${PLAT_ARR[@]}"; do
  [[ -z "$p" ]] && continue
  echo
  echo "================================================================"
  echo "  $p"
  echo "================================================================"
  if bash "$SCRIPT_DIR/verify_project_build.sh" "$EDITOR" "$PROJECT_DIR" "$p"; then
    echo "  -> $p PASSED"
  else
    echo "  -> $p FAILED"
    FAILED+=("$p")
  fi
done

# --- 6. Cleanup --------------------------------------------------------------

if [[ $SKIP_CLEANUP -eq 0 ]]; then
  echo
  echo "==> Cleaning up clone"
  bash "$SCRIPT_DIR/cleanup_test_project.sh" "$DEST" || true
else
  echo
  echo "Clone preserved at: $DEST"
fi

# --- 7. Final summary -------------------------------------------------------

echo
echo "================================================================"
if [[ ${#FAILED[@]} -eq 0 ]]; then
  echo "  All requested platforms passed: $PLATFORMS"
  exit 0
else
  echo "  FAILED platforms: ${FAILED[*]}"
  exit 1
fi
