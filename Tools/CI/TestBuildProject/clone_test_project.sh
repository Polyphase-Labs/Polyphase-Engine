#!/usr/bin/env bash
# Clone (or refresh) an arbitrary git repository for CI-driven Polyphase
# project testing, and drop a marker file so cleanup_test_project.sh can
# verify the directory was created by us before deleting it.
#
# Usage:
#   bash Tools/CI/clone_test_project.sh <git-url> [dest-dir] [--ref <ref>]
#
# Defaults:
#   dest-dir = ./test-project
#   ref      = (default branch via --depth 1)
#
# The marker file (<dest>/.polyphase-ci-clone) records the remote URL so
# cleanup can confirm we're operating on the right directory.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <git-url> [dest-dir] [--ref <ref>]" >&2
  exit 2
fi

REMOTE="$1"; shift
DEST="./test-project"
REF=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --ref) REF="$2"; shift 2 ;;
    --ref=*) REF="${1#*=}"; shift ;;
    -*) echo "Unknown flag: $1" >&2; exit 2 ;;
    *) DEST="$1"; shift ;;
  esac
done

MARKER=".polyphase-ci-clone"

if [[ -d "$DEST/.git" ]]; then
  echo "Clone already present at $DEST — refreshing."
  git -C "$DEST" remote set-url origin "$REMOTE"
  if [[ -n "$REF" ]]; then
    git -C "$DEST" fetch --depth 1 origin "$REF"
    git -C "$DEST" reset --hard FETCH_HEAD
  else
    git -C "$DEST" fetch --depth 1 origin HEAD
    git -C "$DEST" reset --hard FETCH_HEAD
  fi
else
  echo "Cloning $REMOTE into $DEST..."
  if [[ -n "$REF" ]]; then
    git clone --depth 1 --branch "$REF" "$REMOTE" "$DEST"
  else
    git clone --depth 1 "$REMOTE" "$DEST"
  fi
fi

# Drop / refresh the marker file so cleanup can confirm we own this directory.
{
  echo "created_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "remote=$REMOTE"
  [[ -n "$REF" ]] && echo "ref=$REF"
  echo "script=Tools/CI/clone_test_project.sh"
} >"$DEST/$MARKER"

echo
echo "Test project ready at: $DEST"
