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
bash "$SCRIPT_DIR/thirdparty/install-devkitpro-pacman"

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

# Step 3: install requested packages.
PACKAGES=()
[[ $WANT_PPC -eq 1 ]] && PACKAGES+=(wii-dev gamecube-tools-git libogc2 libogc2-libdvm)
[[ $WANT_ARM -eq 1 ]] && PACKAGES+=(3ds-dev)

if [[ ${#PACKAGES[@]} -gt 0 ]]; then
  dkp-pacman -Syyu --noconfirm "${PACKAGES[@]}"
fi

# Step 4: surface env vars. Under GitHub Actions, write to $GITHUB_ENV and
# $GITHUB_PATH; otherwise just print so a dev can `source` or eval them.
DEVKITPRO_DIR="/opt/devkitpro"
DEVKITPPC_DIR="$DEVKITPRO_DIR/devkitPPC"
DEVKITARM_DIR="$DEVKITPRO_DIR/devkitARM"

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
[[ $WANT_PPC -eq 1 ]] && echo "  DEVKITPPC=$DEVKITPPC_DIR"
[[ $WANT_ARM -eq 1 ]] && echo "  DEVKITARM=$DEVKITARM_DIR"
