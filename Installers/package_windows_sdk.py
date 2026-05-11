#!/usr/bin/env python3
"""
package_windows_sdk.py - Package the Windows native-addon SDK zip.

Produces polyphase-sdk-windows-x64.zip containing:
  - Lib/Windows/x64/ReleaseEditor/{Polyphase,Lua}.lib  (Release-CRT import libs)
  - Engine/Source/**.{h,hxx,hpp,inl}                   (engine headers only)
  - External/{Assimp,Bullet,glm,Imgui,ImGuizmo,Lua,Vorbis}/**.{h,hxx,hpp,inl}

The zip is rooted so an addon's build.bat can point POLYPHASE_PATH at the
unzipped root with no /I changes vs. cloning the engine repo directly.

Pre-flight aborts the workflow loudly if either .lib is missing, matching the
"Verify import libraries" pattern in .github/workflows/release.yml.

Usage:
    python Installers/package_windows_sdk.py [--source-root .] [--output polyphase-sdk-windows-x64.zip] [--verbose]
"""

import argparse
import shutil
import sys
import zipfile
from pathlib import Path


# Source paths must match what stage_distribution.py expects at
# Installers/stage_distribution.py:280-289. Kept in sync by duplication
# (two paths; cross-script imports inside Installers/ aren't worth it).
POLYPHASE_LIB_SRC = Path("Standalone") / "Build" / "Windows" / "x64" / "ReleaseEditor" / "Polyphase.lib"
LUA_LIB_SRC       = Path("External") / "Lua" / "Build" / "Windows" / "x64" / "ReleaseEditor" / "Lua.lib"

# Where the libs land inside the SDK zip.
LIB_DST_DIR = Path("Lib") / "Windows" / "x64" / "ReleaseEditor"

# External subdirs to ship. Header-only filter is applied recursively; glm is
# header-only end-to-end (.hpp / .inl) so the filter catches everything.
EXTERNAL_SUBDIRS = ["Assimp", "Bullet", "glm", "Imgui", "ImGuizmo", "Lua", "Vorbis"]

HEADER_EXTS = {".h", ".hxx", ".hpp", ".inl"}

# Dirs to skip when walking External trees — build outputs, not source.
SKIP_DIRS = {"Build", "Intermediate", ".git", ".vs", "__pycache__"}


def log(msg, verbose):
    if verbose:
        print(f"  {msg}")


def copy_headers(src_root: Path, dst_root: Path, verbose: bool) -> int:
    """Recursively copy header files (.h/.hxx/.hpp/.inl) under src_root into
    dst_root, preserving subdir structure. Returns file count."""
    if not src_root.is_dir():
        log(f"SKIP (not found): {src_root}", verbose)
        return 0
    count = 0
    for path in src_root.rglob("*"):
        if path.is_dir():
            continue
        # Skip anything under a SKIP_DIRS subtree.
        rel_parts = path.relative_to(src_root).parts
        if any(part in SKIP_DIRS for part in rel_parts):
            continue
        if path.suffix.lower() not in HEADER_EXTS:
            continue
        dst = dst_root / path.relative_to(src_root)
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, dst)
        count += 1
    return count


def copy_lib(src: Path, dst: Path, verbose: bool) -> None:
    """Copy a single .lib; abort with a clear message if missing."""
    if not src.is_file():
        print(f"ERROR: {src} was not produced by the engine build.")
        print(f"       Without it, native addons cannot link against the editor.")
        sys.exit(1)
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    log(f"{src} -> {dst}", verbose)


def stage_sdk(source_root: Path, stage_dir: Path, verbose: bool) -> None:
    if stage_dir.exists():
        print(f"Cleaning previous staging dir: {stage_dir}")
        shutil.rmtree(stage_dir)
    stage_dir.mkdir(parents=True)

    print("Staging import libraries...")
    copy_lib(source_root / POLYPHASE_LIB_SRC, stage_dir / LIB_DST_DIR / "Polyphase.lib", verbose)
    copy_lib(source_root / LUA_LIB_SRC,       stage_dir / LIB_DST_DIR / "Lua.lib",       verbose)

    print("Staging Engine/Source headers...")
    n = copy_headers(source_root / "Engine" / "Source",
                     stage_dir / "Engine" / "Source", verbose)
    print(f"  {n} header files")
    if n == 0:
        print("ERROR: no headers found under Engine/Source — wrong --source-root?")
        sys.exit(1)

    print("Staging External/ headers...")
    for sub in EXTERNAL_SUBDIRS:
        n = copy_headers(source_root / "External" / sub,
                         stage_dir / "External" / sub, verbose)
        print(f"  External/{sub}: {n} header files")


def zip_stage(stage_dir: Path, output: Path, verbose: bool) -> None:
    print(f"Writing {output} ...")
    if output.exists():
        output.unlink()
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for path in sorted(stage_dir.rglob("*")):
            if path.is_file():
                arcname = path.relative_to(stage_dir)
                zf.write(path, arcname)
                log(f"+ {arcname}", verbose)


def verify_zip(output: Path) -> None:
    """Defensive post-write check: zip exists, non-empty, contains Polyphase.lib."""
    if not output.is_file() or output.stat().st_size == 0:
        print(f"ERROR: {output} is missing or empty after zipping.")
        sys.exit(1)
    expected = str((LIB_DST_DIR / "Polyphase.lib").as_posix())
    with zipfile.ZipFile(output, "r") as zf:
        names = zf.namelist()
        # ZipFile stores forward-slash paths regardless of platform.
        if expected not in names:
            print(f"ERROR: {output} does not contain {expected}.")
            print(f"       Top-level entries: {sorted({n.split('/')[0] for n in names})}")
            sys.exit(1)
    print(f"[OK] {output} ({output.stat().st_size:,} bytes)")


def main():
    parser = argparse.ArgumentParser(description="Package the Polyphase Windows native-addon SDK zip.")
    parser.add_argument("--source-root", default=".", help="Engine repo root (default: cwd)")
    parser.add_argument("--output", default="polyphase-sdk-windows-x64.zip",
                        help="Output zip path (default: polyphase-sdk-windows-x64.zip)")
    parser.add_argument("--stage-dir", default="sdk",
                        help="Intermediate staging dir (default: sdk)")
    parser.add_argument("--verbose", "-v", action="store_true")
    args = parser.parse_args()

    source_root = Path(args.source_root).resolve()
    if not (source_root / "Engine").is_dir():
        print(f"ERROR: Engine/ not found under {source_root}. Pass --source-root.")
        sys.exit(1)

    stage_dir = Path(args.stage_dir)
    if not stage_dir.is_absolute():
        stage_dir = source_root / stage_dir
    output = Path(args.output)
    if not output.is_absolute():
        output = source_root / output

    stage_sdk(source_root, stage_dir, args.verbose)
    zip_stage(stage_dir, output, args.verbose)
    verify_zip(output)


if __name__ == "__main__":
    main()
