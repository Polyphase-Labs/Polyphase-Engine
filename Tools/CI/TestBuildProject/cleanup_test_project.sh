#!/usr/bin/env bash
# Safely delete a directory previously created by clone_test_project.sh.
#
# Usage:
#   bash Tools/CI/cleanup_test_project.sh <dir> [--build-artifacts-only]
#
# Default: removes <dir> entirely. With --build-artifacts-only, removes only
# */Packaged/ and */Intermediate/ subdirectories under <dir> so the next build
# runs from a clean slate without re-cloning.
#
# Refuses to delete unless EVERY safety check passes:
#   1. <dir>/.polyphase-ci-clone marker exists.
#   2. Resolved absolute path has 3+ components and isn't /, $HOME, or $PWD.
#   3. Path is a real directory, not a symlink.
#   4. <dir>/.git/config (if present) references the same remote URL the
#      marker file recorded — proving we're operating on the right clone.

set -euo pipefail

DIR=""
ARTIFACTS_ONLY=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-artifacts-only) ARTIFACTS_ONLY=1; shift ;;
    -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
    -*) echo "Unknown flag: $1" >&2; exit 2 ;;
    *)
      if [[ -n "$DIR" ]]; then
        echo "Unexpected extra argument: $1" >&2
        exit 2
      fi
      DIR="$1"; shift
      ;;
  esac
done

if [[ -z "$DIR" ]]; then
  echo "Usage: $0 <dir> [--build-artifacts-only]" >&2
  exit 2
fi

if [[ ! -d "$DIR" ]]; then
  echo "Nothing to clean: $DIR does not exist."
  exit 0
fi

# Refuse symlinks.
if [[ -L "$DIR" ]]; then
  echo "REFUSE: $DIR is a symlink — refusing to delete." >&2
  exit 1
fi

ABS="$(cd "$DIR" && pwd -P)"

# Refuse obviously-dangerous paths.
case "$ABS" in
  /|"$HOME"|"$HOME/")
    echo "REFUSE: $ABS is too dangerous to delete." >&2
    exit 1
    ;;
esac
if [[ "$ABS" == "$(pwd -P)" ]]; then
  echo "REFUSE: $ABS is the current working directory." >&2
  exit 1
fi

# Require at least 3 path components (e.g. /a/b/c) so a typo can't nuke a root dir.
COMPONENTS=$(awk -F/ '{ c=0; for (i=1;i<=NF;i++) if ($i!="") c++; print c }' <<<"$ABS")
if [[ "$COMPONENTS" -lt 3 ]]; then
  echo "REFUSE: $ABS has fewer than 3 path components — refusing to delete." >&2
  exit 1
fi

# Require the marker file we drop during clone.
MARKER="$ABS/.polyphase-ci-clone"
if [[ ! -f "$MARKER" ]]; then
  echo "REFUSE: marker file '$MARKER' is missing — directory was not created by clone_test_project." >&2
  echo "        If you really want to delete this directory, do it manually." >&2
  exit 1
fi

# Cross-check: if .git/config exists, its remote URL must match the marker's.
GIT_CONFIG="$ABS/.git/config"
if [[ -f "$GIT_CONFIG" ]]; then
  EXPECTED_REMOTE="$(awk -F= '/^remote=/ { sub(/^remote=/, ""); print; exit }' "$MARKER")"
  if [[ -n "$EXPECTED_REMOTE" ]] && ! grep -Fq "$EXPECTED_REMOTE" "$GIT_CONFIG"; then
    echo "REFUSE: $GIT_CONFIG does not reference '$EXPECTED_REMOTE' from the marker — wrong directory?" >&2
    exit 1
  fi
fi

if [[ $ARTIFACTS_ONLY -eq 1 ]]; then
  # Remove every Packaged/ and Intermediate/ subtree under the clone.
  REMOVED=0
  while IFS= read -r -d '' sub; do
    rm -rf "$sub"
    echo "Cleaned: $sub"
    REMOVED=$((REMOVED + 1))
  done < <(find "$ABS" -depth -type d \( -name Packaged -o -name Intermediate \) -print0)
  if [[ $REMOVED -eq 0 ]]; then
    echo "No Packaged/ or Intermediate/ directories found under $ABS."
  fi
  echo "Build artifacts removed; clone preserved at $ABS."
  exit 0
fi

rm -rf "$ABS"
echo "Cleaned: $ABS"
