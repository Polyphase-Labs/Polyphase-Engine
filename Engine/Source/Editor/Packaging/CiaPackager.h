#pragma once

#if EDITOR

#include "Plugins/PolyphaseBuildTargetAPI.h"

#include <string>
#include <cstdint>

/**
 * Built-in "Nintendo 3DS (CIA)" build target.
 *
 * Like the RPM / AppImage targets, compilation is not overridden — the
 * legacy Makefile_3DS path still produces the .elf / .smdh / .3dsx. All this
 * adds is a PostPackage step that wraps the packaged output into an
 * installable <Project>.cia with makerom, optionally with a HOME Menu banner
 * (image or 3D scene) and tune built by bannertool.
 *
 * makerom and bannertool are not part of devkitPro. They are resolved (in
 * order) from the explicit paths set in Preferences > External > Launchers,
 * the editor-managed tools directory the download button fills, devkitPro's
 * tools/bin, and finally PATH.
 */
namespace CiaPackager
{
    extern const char* const kTargetId;

    /** Fill a descriptor ready to hand to BuildTargetRegistry::Register. */
    void FillDesc(PolyphaseBuildTargetDesc& outDesc);

    enum class Tool
    {
        Makerom,
        Bannertool,
        Cwavtool,       // optional DSP-ADPCM banner tune encoder (PCM16 via bannertool is the proven default)
        Python,
        Pycgfx,

        Count
    };

    /**
     * Resolve a tool to an executable path (Makerom / Bannertool), a python
     * command line (Python, e.g. "py -3"), or the directory containing
     * pycgfx's main.py (Pycgfx). Empty when not found. Results are cached;
     * safe to call from the Validate background thread.
     */
    std::string ResolveTool(Tool tool);

    /** Forget cached resolutions (call after prefs change or a download). */
    void InvalidateToolCache();

    /** Explicit path override from preferences ("" = auto-detect). */
    void SetToolOverride(Tool tool, const std::string& path);

    /** <prefs>/../Tools/3DS — where the download button installs tools. */
    std::string GetToolsDirectory();

    /**
     * Write the 48x48 SMDH icon PNG from `source`: an image file (absolute or
     * project-relative), or the name / .oct path of an imported Texture asset.
     * Returns false when nothing usable was found (caller falls back to the
     * libctru default icon).
     */
    bool WriteSmdhIcon(const std::string& source, const std::string& projectDir, const std::string& outPng);

    enum ToolInstallMask : uint32_t
    {
        ToolInstall_Makerom    = 1u << 0,
        ToolInstall_Bannertool = 1u << 1,
        ToolInstall_Pycgfx     = 1u << 2,
        ToolInstall_Cwavtool   = 1u << 3,
    };

    /** Kick off a background download/install. No-op while one is running. */
    void StartToolInstall(uint32_t mask);
    bool IsToolInstallRunning();
    std::string GetToolInstallStatus();
}

#endif /* EDITOR */
