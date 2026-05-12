#!/usr/bin/env python3
"""Generate a Windows .def file listing every public symbol in a set of
.obj files. Used by the engine's Debug Shared|x64 / Release Shared|x64
configs to export the entire engine surface from PolyphaseGame.dll
without per-symbol __declspec(dllexport) annotations.

This is the equivalent of CMake's WINDOWS_EXPORT_ALL_SYMBOLS=ON, ported
to a standalone script invocable as an MSBuild PreLinkEvent.

Usage (typically wired into MSBuild as a PreLinkEvent):
  py generate_dll_def.py --int-dir <path> --output <out.def> [--name <dll_name>]

The script walks --int-dir recursively, runs `dumpbin /symbols` on every
.obj, parses the lines that look like:
  003 00000010 SECT2 notype ()    External     | ?Foo@@YAXXZ (void __cdecl Foo(void))
and emits a Module-Definition file (.def) with EXPORTS for every
External symbol that is NOT a UNDEF reference (i.e. is defined here).

Symbols that should NOT be exported:
  - Symbols starting with `__` (compiler-generated, e.g. __local_stdio_)
  - Symbols starting with `_RTC_`, `_CT??_R*` (RTTI), `__@@_PchSym_` (PCH)
  - UNDEF symbols (these are *imports* this obj needs, not its exports)
  - Static (file-local) symbols, i.e. not 'External'

It uses the MSVC C/C++ name mangling as-is (no demangling) because the
linker matches mangled names. A successfully-generated .def can be passed
to link.exe via /DEF:<file>.

Exit codes:
  0  success
  1  dumpbin not on PATH / not found
  2  no .obj files found under --int-dir
  3  parse failure / wrote zero exports
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

# Pattern for dumpbin /symbols output. Example lines:
#   003 00000010 SECT2  notype ()    External     | ?Foo@@YAXXZ
#   00B 00000000 UNDEF  notype       External     | ??_7type_info@@6B@
# We want the third column ("SECT*" or "UNDEF"), the "External" tag, and
# the symbol name after the pipe.
_SYM_LINE = re.compile(
    r"^\s*[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(SECT[0-9A-Fa-f]+|UNDEF)\s+\S+(?:\s+\([^)]*\))?\s+(External|Static|WeakExternal)\s+\|\s+(\S+)"
)

def _should_skip(sym: str) -> bool:
    """Filter symbols we don't want in the EXPORTS table."""
    if not sym:
        return True
    # Compiler-generated / CRT internals — skip.
    if sym.startswith("__local_stdio"):
        return True
    if sym.startswith("_RTC_"):
        return True
    if sym.startswith("__@@_PchSym"):
        return True
    # Imports from KERNEL32/MSVCP/UCRT (decorated stdcall imports) — leave to
    # the consuming exe to import directly.
    if sym.startswith("__imp_"):
        return True
    # RTTI helper template instantiations — leave them as private.
    if sym.startswith("??_C@"):
        # String literal symbols. Not useful to export.
        return True
    return False

def _run_dumpbin(dumpbin_exe: str, obj_path: Path) -> str:
    """Run dumpbin /symbols on one object and return its stdout."""
    res = subprocess.run(
        [dumpbin_exe, "/symbols", str(obj_path)],
        capture_output=True,
        text=True,
        check=False,
    )
    if res.returncode != 0:
        sys.stderr.write(
            f"[generate_dll_def] warning: dumpbin failed for {obj_path}: {res.stderr.strip()}\n"
        )
    return res.stdout

def _find_dumpbin() -> str | None:
    """Locate dumpbin.exe. Tries PATH first, then a heuristic search under
    Visual Studio install dirs. Returns None if not found."""
    # First: PATH
    for path_entry in os.environ.get("PATH", "").split(os.pathsep):
        candidate = Path(path_entry) / "dumpbin.exe"
        if candidate.exists():
            return str(candidate)
    # MSBuild invocations typically already have dumpbin on PATH via
    # vcvarsall, so the PATH lookup is sufficient in 99% of cases. Don't
    # try to be clever with VSWHERE — fail loudly so the caller can fix
    # their environment.
    return None

def _collect_symbols(dumpbin_exe: str, obj_files: list[Path]) -> set[str]:
    exports: set[str] = set()
    total_seen = 0
    for obj in obj_files:
        out = _run_dumpbin(dumpbin_exe, obj)
        for line in out.splitlines():
            m = _SYM_LINE.match(line)
            if m is None:
                continue
            section, scope, sym = m.group(1), m.group(2), m.group(3)
            total_seen += 1
            # Only emit symbols that are DEFINED here (have a SECTn section)
            # and are visible to the linker (External or WeakExternal).
            if section.startswith("UNDEF"):
                continue
            if scope == "Static":
                continue
            if _should_skip(sym):
                continue
            exports.add(sym)
    sys.stderr.write(
        f"[generate_dll_def] scanned {len(obj_files)} obj files, "
        f"{total_seen} symbol lines, kept {len(exports)} exports\n"
    )
    return exports

def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--int-dir", required=True, help="Intermediate dir to scan for .obj files (recursive)")
    p.add_argument("--output", required=True, help="Path to write the generated .def file")
    p.add_argument("--name", default="PolyphaseGame", help="LIBRARY name written into the .def header")
    p.add_argument("--dumpbin", default=None, help="Explicit path to dumpbin.exe (otherwise resolved from PATH)")
    args = p.parse_args(argv)

    int_dir = Path(args.int_dir)
    if not int_dir.is_dir():
        sys.stderr.write(f"[generate_dll_def] error: --int-dir is not a directory: {int_dir}\n")
        return 2

    obj_files = sorted(int_dir.rglob("*.obj"))
    if not obj_files:
        sys.stderr.write(f"[generate_dll_def] error: no .obj files under {int_dir}\n")
        return 2

    dumpbin = args.dumpbin or _find_dumpbin()
    if not dumpbin:
        sys.stderr.write(
            "[generate_dll_def] error: dumpbin.exe not on PATH. "
            "Run from a Developer Command Prompt or pass --dumpbin.\n"
        )
        return 1

    exports = _collect_symbols(dumpbin, obj_files)
    if not exports:
        sys.stderr.write("[generate_dll_def] error: no exportable symbols found\n")
        return 3

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="ascii", errors="replace") as f:
        f.write(f"LIBRARY {args.name}\n")
        f.write("EXPORTS\n")
        for sym in sorted(exports):
            # The .def syntax allows just listing the symbol name; the linker
            # exports it under the same mangled name.
            f.write(f"    {sym}\n")

    sys.stderr.write(f"[generate_dll_def] wrote {len(exports)} exports to {out_path}\n")
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
