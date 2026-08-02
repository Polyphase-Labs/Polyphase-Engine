#pragma once

#if EDITOR

#include "Plugins/PolyphaseBuildTargetAPI.h"

/**
 * Built-in "Linux (RPM)" build target.
 *
 * Compilation is not overridden — GetCompileCommand stays null so the legacy
 * Makefile_Linux_Game path produces the ELF exactly as the plain Linux target
 * does. All this adds is a PostPackage step that wraps the already-staged
 * payload in Packaged/polyphase.linux.rpm/ into an .rpm.
 *
 * The payload layout mirrors Installers/build_deb_linux.sh: the game tree under
 * <prefix>/<name>/, a working-directory-setting wrapper at /usr/bin/<name>, a
 * .desktop entry, and an optional icon.
 */
namespace RpmPackager
{
    extern const char* const kTargetId;

    /** Fill a descriptor ready to hand to BuildTargetRegistry::Register. */
    void FillDesc(PolyphaseBuildTargetDesc& outDesc);
}

#endif /* EDITOR */
