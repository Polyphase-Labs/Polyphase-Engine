#!/bin/bash
# Installs the Arch package on a clean Arch system and launches the editor.
# Runs as root inside an archlinux container. Software Vulkan (lavapipe) and
# Xvfb stand in for a GPU and display. No ALSA device is configured, which
# also covers the no-audio startup path.
#
# Usage: bash Tools/CI/smoke_test_arch_pkg.sh path/to/polyphase-editor-*.pkg.tar.zst

set -euo pipefail

PKG="$1"
SECONDS_ALIVE=30

# Satisfy the package's vulkan-driver dependency with lavapipe up front so
# pacman doesn't have to pick a hardware driver provider.
pacman -S --noconfirm --needed vulkan-swrast xorg-server-xvfb > /dev/null
pacman -U --noconfirm "$PKG"

echo "==> Package contents check"
test -x /usr/bin/polyphase-editor
test -x /opt/polyphase/PolyphaseEditor
test -f /usr/share/applications/polyphase-editor.desktop

echo "==> Shared library check"
if ldd /opt/polyphase/PolyphaseEditor | grep "not found"; then
    echo "FAIL: editor has unresolved shared libraries on Arch" >&2
    exit 1
fi

echo "==> Launching editor for ${SECONDS_ALIVE}s under Xvfb + lavapipe"
Xvfb :99 -screen 0 1600x900x24 > /dev/null 2>&1 &
XVFB_PID=$!
sleep 2
export DISPLAY=:99
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json

set +e
timeout "$SECONDS_ALIVE" polyphase-editor > editor-smoke.log 2>&1
RC=$?
set -e
kill "$XVFB_PID" 2> /dev/null || true

# Still running when the timeout hit (124) is the pass condition.
if [ "$RC" -ne 124 ]; then
    echo "FAIL: editor exited with code $RC before the ${SECONDS_ALIVE}s window" >&2
    tail -n 100 editor-smoke.log >&2
    exit 1
fi

echo "PASS: editor installed from the Arch package and stayed up for ${SECONDS_ALIVE}s"

pacman -R --noconfirm polyphase-editor > /dev/null
test ! -e /usr/bin/polyphase-editor
echo "PASS: package uninstalls cleanly"
