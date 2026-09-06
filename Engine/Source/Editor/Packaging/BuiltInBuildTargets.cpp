#if EDITOR

#include "BuiltInBuildTargets.h"
#include "BuildTargetRegistry.h"
#include "EngineTypes.h"
#include "Plugins/PolyphaseBuildTargetAPI.h"

#include "RpmPackager.h"
#include "AppImagePackager.h"
#include "CiaPackager.h"
#include "MacBundlePackager.h"

namespace BuiltInBuildTargets
{
    const char* const kWindowsId  = "polyphase.windows";
    const char* const kLinuxId    = "polyphase.linux";
    const char* const kAndroidId  = "polyphase.android";
    const char* const kGameCubeId = "polyphase.gamecube";
    const char* const kWiiId      = "polyphase.wii";
    const char* const kN3DSId     = "polyphase.n3ds";
    const char* const kMacId      = MacBundlePackager::kTargetId;

    const char* IdForPlatform(int platform)
    {
        switch (static_cast<Platform>(platform))
        {
        case Platform::Windows:  return kWindowsId;
        case Platform::Linux:    return kLinuxId;
        case Platform::Android:  return kAndroidId;
        case Platform::GameCube: return kGameCubeId;
        case Platform::Wii:      return kWiiId;
        case Platform::N3DS:     return kN3DSId;
        case Platform::Mac:      return kMacId;
        default: return "";
        }
    }

    // The six platform-owning targets carry metadata only — callback pointers
    // stay null, so ActionManager's legacy switch-on-Platform path runs for
    // them end to end. Targets that DO need callbacks (built-in packagers as
    // well as addon-provided targets) register a filled-in descriptor below;
    // ActionManager dispatches on the callback being present, not on where the
    // target came from.
    static void RegisterBuiltIn(BuildTargetRegistry& registry,
                                const char* id, const char* displayName,
                                const char* category, Platform basePlatform,
                                const char* extension,
                                bool supportsRunOnDevice, bool supportsEmulator)
    {
        PolyphaseBuildTargetDesc desc{};
        desc.apiVersion          = POLYPHASE_BUILD_TARGET_API_VERSION;
        desc.targetId            = id;
        desc.displayName         = displayName;
        desc.iconText            = "";
        desc.category            = category;
        desc.basePlatform        = static_cast<int32_t>(basePlatform);
        desc.binaryExtension     = extension;
        desc.requiresDocker      = 0;
        desc.supportsRunOnDevice = supportsRunOnDevice ? 1 : 0;
        desc.supportsEmulator    = supportsEmulator ? 1 : 0;

        registry.Register(/*hookId=*/ 0, &desc, /*isBuiltIn=*/ true);
    }

    void RegisterAll(BuildTargetRegistry& registry)
    {
        RegisterBuiltIn(registry, kWindowsId,  "Windows",          "Desktop",   Platform::Windows,  ".exe",  false, false);
        RegisterBuiltIn(registry, kLinuxId,    "Linux",            "Desktop",   Platform::Linux,    ".elf",  false, false);
        RegisterBuiltIn(registry, kAndroidId,  "Android",          "Mobile",    Platform::Android,  ".apk",  true,  false);
        RegisterBuiltIn(registry, kGameCubeId, "GameCube",         "Console",   Platform::GameCube, ".dol",  false, true);
        RegisterBuiltIn(registry, kWiiId,      "Wii",              "Console",   Platform::Wii,      ".dol",  true,  true);
        RegisterBuiltIn(registry, kN3DSId,     "Nintendo 3DS",     "Handheld",  Platform::N3DS,     ".3dsx", true,  true);

        // macOS is the canonical (platform-owning) target for Platform::Mac,
        // yet it ships real callbacks: the Makefile compile path is shared
        // with Linux, and PostPackage wraps the Mach-O into <Project>.app.
        // Registered straight from its descriptor (RegisterBuiltIn would null
        // the callbacks).
        {
            PolyphaseBuildTargetDesc desc{};
            MacBundlePackager::FillDesc(desc);
            registry.Register(/*hookId=*/ 0, &desc, /*isBuiltIn=*/ true);
        }

        // Linux packaging targets. Each shares Platform::Linux with kLinuxId —
        // they reuse the same compile and cook path entirely and only add a
        // PostPackage step that wraps the staged payload. Because they aren't
        // the canonical target for Linux, ActionManager gives them their own
        // Packaged/<targetId>/ directory and their own build-cache manifest.
        {
            PolyphaseBuildTargetDesc desc{};
            RpmPackager::FillDesc(desc);
            registry.Register(/*hookId=*/ 0, &desc, /*isBuiltIn=*/ true);
        }

        {
            PolyphaseBuildTargetDesc desc{};
            AppImagePackager::FillDesc(desc);
            registry.Register(/*hookId=*/ 0, &desc, /*isBuiltIn=*/ true);
        }

        // 3DS installable title. Shares Platform::N3DS with kN3DSId the same
        // way the Linux packagers share Platform::Linux: same cook + Makefile
        // compile, plus a PostPackage that wraps the output into a .cia.
        {
            PolyphaseBuildTargetDesc desc{};
            CiaPackager::FillDesc(desc);
            registry.Register(/*hookId=*/ 0, &desc, /*isBuiltIn=*/ true);
        }
    }
}

#endif /* EDITOR */
