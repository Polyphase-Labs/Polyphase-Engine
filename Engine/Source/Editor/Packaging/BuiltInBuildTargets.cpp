#if EDITOR

#include "BuiltInBuildTargets.h"
#include "BuildTargetRegistry.h"
#include "EngineTypes.h"
#include "Plugins/PolyphaseBuildTargetAPI.h"

namespace BuiltInBuildTargets
{
    const char* const kWindowsId  = "polyphase.windows";
    const char* const kLinuxId    = "polyphase.linux";
    const char* const kAndroidId  = "polyphase.android";
    const char* const kGameCubeId = "polyphase.gamecube";
    const char* const kWiiId      = "polyphase.wii";
    const char* const kN3DSId     = "polyphase.n3ds";

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
        default: return "";
        }
    }

    // Built-in descriptors carry metadata only — callback pointers stay
    // null so ActionManager's legacy switch-on-Platform path runs for them.
    // Addon-provided targets register descriptors with real callbacks and
    // dispatch through those instead.
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
        // All function pointers left as nullptr — ActionManager checks
        // RegisteredBuildTarget::mIsBuiltIn and runs the legacy path.

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
    }
}

#endif /* EDITOR */
