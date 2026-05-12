#include <stdint.h>

#undef min
#undef max

#include "Engine.h"
#include "World.h"
#include "Renderer.h"
#include "InputDevices.h"
#include "Log.h"
#include "Assets/Scene.h"
#include "AssetManager.h"

#include "Nodes/Widgets/StatsOverlay.h"

#if POLYPHASE_DLL_CONSUMER && !EDITOR
// W1: Runtime addon DLL/SO discovery (Phase 5 of DLL plan).
#include "System/System.h"
#include "System/ModuleLoader.h"
#include "Plugins/RuntimePluginManager.h"
#include "Plugins/PolyphasePluginAPI.h"
#include <string>
#endif

#define EMBEDDED_ENABLED (PLATFORM_DOLPHIN || PLATFORM_3DS || PLATFORM_LINUX || PLATFORM_WINDOWS)

#if EMBEDDED_ENABLED
#include "../Generated/EmbeddedAssets.h"
#include "../Generated/EmbeddedScripts.h"
extern uint32_t gNumEmbeddedAssets;
#endif

void OctPreInitialize(EngineConfig& config)
{
    GetEngineState()->mStandalone = true;

#if !EDITOR

    if (config.mWindowWidth == 0)
        config.mWindowWidth = 1280;

    if (config.mWindowHeight == 0)
        config.mWindowHeight = 720;

#if EMBEDDED_ENABLED
    config.mEmbeddedAssetCount = gNumEmbeddedAssets;
    config.mEmbeddedAssets = gEmbeddedAssets;
    config.mEmbeddedScriptCount = gNumEmbeddedScripts;
    config.mEmbeddedScripts = gEmbeddedScripts;
    config.mEmbeddedConfig = gEmbeddedConfig_Data;
    config.mEmbeddedConfigSize = gEmbeddedConfig_Size;
#endif

#endif
}

void OctPostInitialize()
{

}

void OctPreUpdate()
{

}

void OctPostUpdate()
{

}

void OctPreShutdown()
{

}

void OctPostShutdown()
{

}

// W1: When the exe consumes the engine DLL, the static-lib auto-register
// (Engine/Source/Engine/OctHookAutoRegister.cpp) is gated out of the DLL,
// so the engine's OctGameHooks struct starts empty. This block runs at exe
// static init and registers the locally-defined Oct* functions via the
// DLL-exported RegisterOctHooks(). The DLL is loaded before exe static
// init runs, so the function pointer is already resolved by the time we
// call it. No-op for static-lib builds — the engine wires itself up.
#if POLYPHASE_DLL_CONSUMER
namespace {
    struct OctHooksAutoRegister {
        OctHooksAutoRegister() {
            OctGameHooks h;
            h.preInitialize  = &OctPreInitialize;
            h.postInitialize = &OctPostInitialize;
            h.preUpdate      = &OctPreUpdate;
            h.postUpdate     = &OctPostUpdate;
            h.preShutdown    = &OctPreShutdown;
            h.postShutdown   = &OctPostShutdown;
            RegisterOctHooks(h);
        }
    } sOctHooksAutoRegister;
}
#endif

// W1: Exe-side entry-point block, used when the standalone exe links against
// PolyphaseGame.dll instead of the static Engine.lib. The static-lib build path
// is unchanged — that one gets its main/GameMain from Engine.cpp (which the DLL
// build gates out via POLYPHASE_DLL_BUILD). POLYPHASE_DLL_CONSUMER is set only
// in Standalone.vcxproj's Debug Shared / Release Shared configs.
//
// The block is a verbatim copy of Engine.cpp's gated-out version. Oct* refs
// here resolve locally in this TU (definitions are right above), so exe-link
// is self-contained and the DLL boundary is crossed only via the exported
// engine functions (Initialize, Update, Shutdown, ReadEngineConfig,
// ReadCommandLineArgs, GetEngineState, EnableConsole, EditorMain, etc.).
#if POLYPHASE_DLL_CONSUMER

#if EDITOR
// Forward declaration — EditorMain is exported by PolyphaseEditor.dll.
void EditorMain(int32_t argc, char** argv);
#endif

#if !EDITOR
// W1: Discover addon DLLs/SOs in <exe-dir>/Addons/ and queue them for the
// engine's RuntimePluginManager to process at Initialize() time. Each addon
// file must export `PolyphasePlugin_GetDesc` (the same entry point the editor
// hot-reload pathway uses). Failures are logged but do not abort the game —
// a broken addon shouldn't take down the whole title.
static void DiscoverAndQueueRuntimeAddons()
{
    std::string exePath = SYS_GetExecutablePath();
    size_t lastSlash = exePath.find_last_of("\\/");
    std::string addonDir = (lastSlash != std::string::npos)
        ? exePath.substr(0, lastSlash) + "/Addons"
        : std::string("./Addons");

    DirEntry entry;
    SYS_OpenDirectory(addonDir, entry);
    if (!entry.mValid)
    {
        // No Addons directory next to the exe — fine, it's optional.
        SYS_CloseDirectory(entry);
        return;
    }

#if PLATFORM_WINDOWS
    const std::string kAddonExt = ".dll";
#else
    const std::string kAddonExt = ".so";
#endif

    while (entry.mValid)
    {
        if (!entry.mDirectory)
        {
            std::string filename = entry.mFilename;
            if (filename.size() > kAddonExt.size() &&
                filename.compare(filename.size() - kAddonExt.size(), kAddonExt.size(), kAddonExt) == 0)
            {
                std::string fullPath = addonDir + "/" + filename;
                void* handle = MOD_Load(fullPath.c_str());
                if (handle != nullptr)
                {
                    using GetDescFn = int (*)(PolyphasePluginDesc*);
                    GetDescFn getDesc = reinterpret_cast<GetDescFn>(MOD_Symbol(handle, "PolyphasePlugin_GetDesc"));
                    if (getDesc != nullptr)
                    {
                        std::string addonId = filename.substr(0, filename.size() - kAddonExt.size());
                        QueuePluginRegistration(getDesc, addonId.c_str());
                        LogDebug("[RuntimeAddon] Loaded %s", fullPath.c_str());
                    }
                    else
                    {
                        LogWarning("[RuntimeAddon] %s: missing PolyphasePlugin_GetDesc symbol", fullPath.c_str());
                        MOD_Unload(handle);
                    }
                }
                else
                {
                    LogWarning("[RuntimeAddon] failed to load %s: %s", fullPath.c_str(), MOD_GetError());
                }
            }
        }
        SYS_IterateDirectory(entry);
    }

    SYS_CloseDirectory(entry);
}
#endif

#if !EDITOR
void GameMain(int32_t argc, char** argv)
{
    GetEngineState()->mArgC = argc;
    GetEngineState()->mArgV = argv;
    ReadCommandLineArgs(argc, argv);
    OctPreInitialize(*GetMutableEngineConfig());
    ReadEngineConfig();
    DiscoverAndQueueRuntimeAddons();
    Initialize();
    OctPostInitialize();

    EnableConsole(true);
    EnableConsole(false);

    bool loop = true;
    while (loop)
    {
        OctPreUpdate();
        loop = Update();
        OctPostUpdate();
    }

    OctPreShutdown();
    Shutdown();
    OctPostShutdown();
}
#endif

#if PLATFORM_WINDOWS && !_DEBUG && !EDITOR
int32_t WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int32_t nCmdShow)
{
    int32_t argc = __argc;
    char** argv = __argv;
#else
int main(int argc, char** argv)
{
#endif

#if EDITOR
    EditorMain(argc, argv);
#else
    GameMain(argc, argv);
#endif

    return 0;
}

#endif  // POLYPHASE_DLL_CONSUMER
