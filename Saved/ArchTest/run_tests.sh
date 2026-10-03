#!/usr/bin/env bash
# Arch Linux build + smoke test for Polyphase. Each step logs to /out/<step>.log,
# results go to /out/summary.txt. Stops at the first failing build step.
set -u

REPO=${REPO:-https://github.com/Polyphase-Labs/Polyphase-Engine}
REF=${REF:-main}
JOBS=${JOBS:-6}
TEST_PROJECT=${TEST_PROJECT:-https://github.com/mholtkamp/octo-bombers}
OUT=/out
SRC=/work/pp
SUMMARY=$OUT/summary.txt
: > "$SUMMARY"

step() {
  local name=$1; shift
  echo "=== $name"
  local t0=$(date +%s)
  ( set -e; "$@" ) > "$OUT/$name.log" 2>&1
  local rc=$?
  local res=PASS; [ $rc -eq 0 ] || res=FAIL
  echo "$res  $name  rc=$rc  $(( $(date +%s) - t0 ))s" | tee -a "$SUMMARY"
  return $rc
}

env_info() {
  cat /etc/os-release | head -2
  gcc --version | head -1; ldd --version | head -1; cmake --version | head -1
  python3 --version; glslc --version | head -1
  VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json vulkaninfo --summary 2>&1 | grep -E "deviceName|apiVersion" || true
}

clone() {
  git clone --depth 1 --branch "$REF" --recurse-submodules --shallow-submodules "$REPO" "$SRC"
  git -C "$SRC" log -1 --oneline
  if [ -f /out/local.patch ]; then git -C "$SRC" apply --ignore-whitespace /out/local.patch && echo "applied local.patch"; fi
}

prebuild()     { cd "$SRC" && bash Tools/prebuild.sh; }
build_editor() { make -C "$SRC/Standalone" -f Makefile_Linux_Editor -j"$JOBS"; }

editor_sanity() {
  local e="$SRC/Standalone/Build/Linux/PolyphaseEditor.elf"
  ls -la "$e"; ldd "$e"
  ! ldd "$e" | grep -q "not found"
  "$e" -h || true
}

package_game() {
  git clone --depth 1 "$TEST_PROJECT" /work/proj
  bash "$SRC/Tools/CI/TestBuildProject/verify_project_build.sh" \
    "$SRC/Standalone/Build/Linux/PolyphaseEditor.elf" /work/proj/Bomber Linux
}

# Runs a binary under Xvfb + lavapipe for N seconds. Still alive at timeout (rc 124) = pass.
run_under_xvfb() {
  local tag=$1 secs=$2; shift 2
  Xvfb :99 -screen 0 1280x720x24 >/dev/null 2>&1 &
  local xpid=$!
  sleep 2
  export DISPLAY=:99 VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
  ( sleep $(( secs - 3 )); xwd -root -silent | magick xwd:- "$OUT/$tag.png" ) &
  timeout "$secs" "$@"
  local rc=$?
  kill $xpid 2>/dev/null
  echo "exit rc=$rc"
  [ $rc -eq 124 ]
}

run_game() {
  cd /work/proj/Bomber/Packaged/Linux
  ls -la
  local game=$(ls *.elf | head -1)
  run_under_xvfb game 20 "./$game"
}

run_editor() {
  cd "$SRC/Standalone/Build/Linux"
  run_under_xvfb editor 40 ./PolyphaseEditor.elf -project /work/proj/Bomber/$(cd /work/proj/Bomber && ls *.octp | head -1)
}

step env           env_info
step clone         clone          || exit 1
step prebuild      prebuild       || exit 1
step build_editor  build_editor   || exit 1
step editor_sanity editor_sanity
step package_game  package_game   && step run_game run_game
step run_editor    run_editor
echo "--- summary"; cat "$SUMMARY"
