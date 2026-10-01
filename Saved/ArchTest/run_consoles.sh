#!/usr/bin/env bash
# Console packaging on Arch: Wii, GameCube, 3DS through the headless editor,
# same script CI uses. Logs to /out/console_<platform>.log.
set -u

SRC=/work/pp
PROJ=/work/proj/Bomber
OUT=/out
SUMMARY=$OUT/summary_consoles.txt
: > "$SUMMARY"

echo "=== toolchains" | tee "$OUT/console_env.log"
{
  powerpc-eabi-gcc --version | head -1
  arm-none-eabi-gcc --version | head -1
  ls /opt/devkitpro/libogc2/lib/cube/libogc.a /opt/devkitpro/libogc/lib/wii/libogc.a /opt/devkitpro/libctru/lib/libctru.a
  command -v elf2dol gcdsptool 3dsxtool smdhtool
} >> "$OUT/console_env.log" 2>&1
cat "$OUT/console_env.log"

for P in ${PLATFORMS:-Wii GameCube 3DS}; do
  echo "=== $P"
  t0=$(date +%s)
  bash "$SRC/Tools/CI/TestBuildProject/verify_project_build.sh" \
    "$SRC/Standalone/Build/Linux/PolyphaseEditor.elf" "$PROJ" "$P" \
    > "$OUT/console_$P.log" 2>&1
  rc=$?
  res=PASS; [ $rc -eq 0 ] || res=FAIL
  echo "$res  $P  rc=$rc  $(( $(date +%s) - t0 ))s" | tee -a "$SUMMARY"
  ls -la "$PROJ/Packaged/$P" >> "$OUT/console_$P.log" 2>&1
done
echo "--- summary"; cat "$SUMMARY"
