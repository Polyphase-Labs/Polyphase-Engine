#!/bin/bash
# Verify that Mach-O files / static archives contain the required CPU slices.
#
#   bash Tools/CI/mac_check_archs.sh "arm64 x86_64" <file>...
#
# Executables and dylibs are inspected with `lipo -archs`. Static archives
# produced by `ar` from fat objects (what Makefile_Mac emits for
# MAC_ARCH=universal) are thin archives of fat members, which lipo does not
# understand as a unit, so one member is extracted and inspected instead.
# libtool-style fat archives (as LunarG ships) answer lipo directly.
set -euo pipefail

if [ $# -lt 2 ]; then
    echo "usage: $0 \"<arch> [<arch>...]\" <file>..." >&2
    exit 2
fi
REQUIRED="$1"; shift

archs_of() {
    local f="$1"
    if lipo -archs "$f" 2>/dev/null; then
        return 0
    fi
    local tmp member
    tmp="$(mktemp -d)"
    member="$(ar -t "$f" 2>/dev/null | grep -v '^__\.SYMDEF' | head -n1 || true)"
    if [ -z "$member" ]; then
        rm -rf "$tmp"
        return 1
    fi
    (cd "$tmp" && ar -x "$f" "$member")
    lipo -archs "$tmp/$member"
    rm -rf "$tmp"
}

status=0
for f in "$@"; do
    if [ ! -e "$f" ]; then
        echo "MISSING  $f" >&2
        status=1
        continue
    fi
    have="$(archs_of "$f" || true)"
    missing=""
    for a in $REQUIRED; do
        case " $have " in
            *" $a "*) ;;
            *) missing="$missing $a" ;;
        esac
    done
    if [ -n "$missing" ]; then
        echo "FAIL     $f: has '$have', missing$missing" >&2
        status=1
    else
        echo "OK       $f: $have"
    fi
done
exit $status
