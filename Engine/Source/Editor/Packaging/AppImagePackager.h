#pragma once

#if EDITOR

#include "Plugins/PolyphaseBuildTargetAPI.h"

/**
 * Built-in "Linux (AppImage)" build target.
 *
 * Like the RPM target, this overrides nothing about compilation — the legacy
 * Makefile_Linux_Game path still produces the ELF. PostPackage assembles an
 * AppDir around the staged payload and hands it to appimagetool.
 *
 * Two things are load-bearing and easy to get wrong:
 *  - AppRun must cd into the payload before exec, because the engine resolves
 *    project paths relative to the working directory.
 *  - AppRun and the ELF must be chmod +x inside the AppDir. The engine never
 *    sets an execute bit on packaged output, and mksquashfs copies modes
 *    verbatim, so a missing bit produces an AppImage that builds fine and
 *    refuses to run.
 *
 * An AppImage inherits the glibc of whatever host compiled the ELF and will
 * not start on older distros. Building the ELF in the oldest supported Docker
 * image (see Docker/build.sh) is the fix; nothing here can work around it.
 */
namespace AppImagePackager
{
    extern const char* const kTargetId;

    /** Fill a descriptor ready to hand to BuildTargetRegistry::Register. */
    void FillDesc(PolyphaseBuildTargetDesc& outDesc);
}

#endif /* EDITOR */
