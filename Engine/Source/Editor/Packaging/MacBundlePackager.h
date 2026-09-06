#pragma once

#if EDITOR

#include "Plugins/PolyphaseBuildTargetAPI.h"

#include <string>

/**
 * Built-in "macOS (App Bundle)" build target — the canonical target for
 * Platform::Mac.
 *
 * Compilation is untouched: ActionManager's legacy Makefile path builds the
 * arm64 Mach-O with Makefile_Mac_Game exactly like Linux. PostPackage then
 * wraps the staged payload into <Project>.app:
 *
 *   Contents/MacOS/<Project>              the Mach-O (from Packaged/Mac/<Project>.macho; + MacOS/Addons/<name>.dylib)
 *   Contents/Frameworks/                  libvulkan.1.dylib + libMoltenVK.dylib
 *   Contents/Resources/                   the loose Packaged/Mac tree
 *   Contents/Resources/vulkan/icd.d/      MoltenVK_icd.json (bundle-relative)
 *   Contents/Info.plist, PkgInfo, .icns
 *
 * The Vulkan loader searches <bundle>/Contents/Resources/vulkan/icd.d before
 * anything else on macOS, so a bundled game needs no VK_ICD_FILENAMES.
 *
 * The app is always at least ad-hoc signed: Apple Silicon refuses to run
 * unsigned arm64 code, and install_name_tool invalidates the linker's
 * signature. A Developer ID identity, notarization and a .dmg are opt-in
 * profile options.
 */
namespace MacBundlePackager
{
    extern const char* const kTargetId;

    /** Fill a descriptor ready to hand to BuildTargetRegistry::Register. */
    void FillDesc(PolyphaseBuildTargetDesc& outDesc);

    /**
     * Locate the LunarG Vulkan SDK root for macOS (the directory holding
     * bin/, lib/, include/): $VULKAN_SDK, else the newest ~/VulkanSDK/<ver>/macOS.
     * Returns "" when nothing is found.
     */
    std::string ResolveVulkanSdkRoot();
}

#endif /* EDITOR */
