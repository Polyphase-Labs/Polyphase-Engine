#!/usr/bin/env bash
# Install devkitPro toolchains needed by Polyphase console targets.
#
# Usage:
#   sudo bash Tools/CI/install_devkitpro.sh --platforms <list>
#
# <list> is a comma-separated subset of: ppc, arm, all
#   ppc -> wii-dev gamecube-tools-git libogc2 libogc2-libdvm
#   arm -> 3ds-dev
#   all -> ppc + arm
#
# Designed to be safe to run repeatedly. When run under GitHub Actions
# (detected via $GITHUB_ENV), exports DEVKITPRO/DEVKITPPC/DEVKITARM and
# the bin paths to subsequent steps.

set -euo pipefail

if [[ "$(id -u)" -ne 0 ]]; then
  echo "Need root privilege to install devkitPro packages." >&2
  echo "Re-run with sudo: sudo bash $0 $*" >&2
  exit 1
fi

PLATFORMS=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --platforms)
      PLATFORMS="$2"
      shift 2
      ;;
    --platforms=*)
      PLATFORMS="${1#*=}"
      shift
      ;;
    -h|--help)
      sed -n '2,15p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$PLATFORMS" ]]; then
  echo "Missing --platforms argument (use ppc, arm, all, or a comma list)" >&2
  exit 2
fi

WANT_PPC=0
WANT_ARM=0
IFS=',' read -ra PARTS <<<"$PLATFORMS"
for p in "${PARTS[@]}"; do
  case "$p" in
    ppc) WANT_PPC=1 ;;
    arm) WANT_ARM=1 ;;
    all) WANT_PPC=1; WANT_ARM=1 ;;
    "")  ;;
    *) echo "Unknown platform '$p' (expected ppc, arm, or all)" >&2; exit 2 ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Step 1: install devkitPro pacman (idempotent — the bundled script no-ops if
# the apt repo and key already exist).
#
# Failure mode we tolerate: pkg.devkitpro.org sits behind Cloudflare and
# occasionally serves a 403 to apt's default user-agent. The
# `apt-get install devkitpro-pacman` step in the bundled installer has a
# post-install hook that runs `dkp-pacman -Sy` against pkg.devkitpro.org;
# when that 403's, dpkg fails with exit 1 even though the binary itself was
# unpacked successfully (apt unpacks before running postinst).
#
# Workaround: if the installer fails, check whether dkp-pacman is callable.
# If yes, retry the database sync ourselves with backoff (Cloudflare's bot
# challenge usually resolves within a few seconds), then `dpkg --configure -a`
# to mark the package configured so apt is happy on subsequent runs.
if ! bash "$SCRIPT_DIR/thirdparty/install-devkitpro-pacman"; then
  echo "WARNING: install-devkitpro-pacman exited non-zero (likely Cloudflare 403)." >&2
  if ! command -v dkp-pacman >/dev/null 2>&1; then
    echo "ERROR: dkp-pacman binary is not on PATH — install genuinely failed." >&2
    exit 1
  fi

  echo "Retrying database sync directly (up to 5 attempts with backoff)..." >&2
  sync_ok=0
  for attempt in 1 2 3 4 5; do
    sleep $(( attempt * 3 ))
    if dkp-pacman -Syy --noconfirm; then
      sync_ok=1
      break
    fi
    echo "  attempt $attempt failed, retrying..." >&2
  done

  if [[ $sync_ok -ne 1 ]]; then
    echo "ERROR: dkp-pacman database sync failed after 5 attempts." >&2
    echo "       pkg.devkitpro.org may be returning 403 from Cloudflare." >&2
    exit 1
  fi

  # Resync apt's view: postinst left dpkg with the package half-configured.
  dpkg --configure -a || true
fi

# Step 2: register the libogc2 keyring + repo (mirrors Docker/Dockerfile:86-92).
# libogc2 packages are signed by a key not in the default devkitpro keyring.
LIBOGC2_KEY="C8A2759C315CFBC3429CC2E422B803BA8AA3D7CE"
if ! dkp-pacman-key --list-keys "$LIBOGC2_KEY" >/dev/null 2>&1; then
  dkp-pacman-key --recv-keys "$LIBOGC2_KEY" --keyserver keyserver.ubuntu.com
  dkp-pacman-key --lsign-key "$LIBOGC2_KEY"
fi

PACMAN_CONF="/opt/devkitpro/pacman/etc/pacman.conf"
if [[ -f "$PACMAN_CONF" ]] && ! grep -q '\[libogc2-devkitpro\]' "$PACMAN_CONF"; then
  cat >>"$PACMAN_CONF" <<'EOF'

[libogc2-devkitpro]
Server = https://packages.libogc2.org/devkitpro/linux/$arch
Server = https://packages.extremscorner.org/devkitpro/linux/$arch
EOF
fi

# Step 3: install requested packages. Retry on transient failure because
# pkg.devkitpro.org / packages.libogc2.org sit behind Cloudflare and can
# 403 individual fetches even after a successful database sync.
run_dkp_pacman() {
  local install_ok=0
  for attempt in 1 2 3 4 5; do
    if [[ $attempt -gt 1 ]]; then
      echo "Retrying dkp-pacman (attempt $attempt of 5)..." >&2
      sleep $(( attempt * 3 ))
    fi
    if dkp-pacman "$@"; then
      install_ok=1
      break
    fi
  done
  if [[ $install_ok -ne 1 ]]; then
    echo "ERROR: dkp-pacman failed after 5 attempts: $*" >&2
    exit 1
  fi
}

# Phase A: libogc2 first. A single-pass install of `wii-dev gamecube-tools-git
# libogc2 libogc2-libdvm` pulls the legacy `ogc-cmake` (member of the
# `wii-dev` group) into the same transaction as `libogc2-cmake` (provided by
# `libogc2`); pacman drops ogc-cmake with a conflict warning, and the
# libogc2-cmake <-> wii-cmake / gamecube-cmake deps form a cycle it has to
# break by picking an arbitrary install order. Installing libogc2 first
# means its cmake helper is on disk before Phase B resolves the dev groups,
# so `ogc-cmake` is never selected and the cycle is already broken.
if [[ $WANT_PPC -eq 1 ]]; then
  run_dkp_pacman -Syyu --noconfirm libogc2 libogc2-libdvm
fi

# Phase B: the dev groups themselves. `-Syyu` again is cheap (DB is already
# current when Phase A ran) and keeps the call self-contained for the
# 3DS-only path where Phase A is skipped.
PHASE_B=()
if [[ $WANT_PPC -eq 1 ]]; then PHASE_B+=(wii-dev gamecube-tools-git); fi
if [[ $WANT_ARM -eq 1 ]]; then PHASE_B+=(3ds-dev); fi
if [[ ${#PHASE_B[@]} -gt 0 ]]; then
  run_dkp_pacman -Syyu --noconfirm "${PHASE_B[@]}"
fi

# Step 4: surface env vars. Under GitHub Actions, write to $GITHUB_ENV and
# $GITHUB_PATH; otherwise just print so a dev can `source` or eval them.
DEVKITPRO_DIR="/opt/devkitpro"
DEVKITPPC_DIR="$DEVKITPRO_DIR/devkitPPC"
DEVKITARM_DIR="$DEVKITPRO_DIR/devkitARM"

# Detect the silent-fail mode: running under Actions (sudo elevated) but the
# Actions env-var paths weren't preserved through sudo. Without this warning
# the install completes "successfully" but the next step's make can't find
# powerpc-eabi-gcc / arm-none-eabi-gcc and crashes with a confusing error.
if [[ "${GITHUB_ACTIONS:-}" == "true" && -z "${GITHUB_ENV:-}" ]]; then
  echo "WARNING: GITHUB_ENV is empty even though we appear to be running under" >&2
  echo "         GitHub Actions. Toolchain env vars (DEVKITPRO/DEVKITPPC/" >&2
  echo "         DEVKITARM) won't be exported to subsequent steps. This usually" >&2
  echo "         means the workflow invoked us with 'sudo' instead of 'sudo -E'." >&2
  echo "         Update the workflow to: sudo -E bash $0 ..." >&2
fi

if [[ -n "${GITHUB_ENV:-}" ]]; then
  {
    echo "DEVKITPRO=$DEVKITPRO_DIR"
    [[ $WANT_PPC -eq 1 ]] && echo "DEVKITPPC=$DEVKITPPC_DIR"
    [[ $WANT_ARM -eq 1 ]] && echo "DEVKITARM=$DEVKITARM_DIR"
  } >>"$GITHUB_ENV"
fi
if [[ -n "${GITHUB_PATH:-}" ]]; then
  {
    echo "$DEVKITPRO_DIR/tools/bin"
    [[ $WANT_PPC -eq 1 ]] && echo "$DEVKITPPC_DIR/bin"
    [[ $WANT_ARM -eq 1 ]] && echo "$DEVKITARM_DIR/bin"
  } >>"$GITHUB_PATH"
fi

echo
echo "devkitPro install complete."
echo "  DEVKITPRO=$DEVKITPRO_DIR"
if [[ $WANT_PPC -eq 1 ]]; then echo "  DEVKITPPC=$DEVKITPPC_DIR"; fi
if [[ $WANT_ARM -eq 1 ]]; then echo "  DEVKITARM=$DEVKITARM_DIR"; fi

# Explicit exit 0: bash's exit status is the last command's status. The
# `[[ ]] && echo` form above would short-circuit to status 1 when WANT_ARM=0
# (i.e., --platforms ppc), failing the entire workflow step despite the
# install having succeeded. The if/fi blocks above already handle that case
# correctly, but anchor the success here so a future tail-line refactor can't
# silently regress it. (3DS worked, Wii/GameCube didn't, until this.)
exit 0
