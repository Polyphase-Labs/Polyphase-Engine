#pragma once

#include <stdint.h>
#include <string>

#include "PolyphaseAPI.h"
#include "EngineTypes.h"
#include "Property.h"

extern void OctPreInitialize(EngineConfig& config);
extern void OctPostInitialize();
extern void OctPreUpdate();
extern void OctPostUpdate();
extern void OctPreShutdown();
extern void OctPostShutdown();

// W1: Hook registration indirection. Today the engine static-lib path resolves
// the six Oct* externs above at exe-link time (Standalone/Main.cpp,
// Template/Main.cpp, or a user game's Main.cpp provides the defs). That extern
// model can't cross the engine-DLL boundary, so the engine routes its hook
// calls through this small function-pointer struct instead.
//
// - Static-lib builds (POLYPHASE_DLL_BUILD undefined): Engine/Source/Engine/OctHookAutoRegister.cpp
//   takes the address of the externs at static-init time and calls RegisterOctHooks().
//   Zero source change required of existing game projects.
// - DLL builds (POLYPHASE_DLL_BUILD=1): the engine DLL has no Oct* externs in
//   its TUs. The consuming exe (POLYPHASE_DLL_CONSUMER=1) defines its own Oct*
//   and registers them via the DLL-exported RegisterOctHooks() at exe static
//   init — see Standalone/Source/Main.cpp.
struct OctGameHooks
{
    void (*preInitialize)(EngineConfig&) = nullptr;
    void (*postInitialize)()             = nullptr;
    void (*preUpdate)()                  = nullptr;
    void (*postUpdate)()                 = nullptr;
    void (*preShutdown)()                = nullptr;
    void (*postShutdown)()               = nullptr;
};
POLYPHASE_API void RegisterOctHooks(const OctGameHooks& hooks);
POLYPHASE_API const OctGameHooks& GetOctHooks();

bool Initialize();

bool Update();

void Shutdown();

void Quit();

POLYPHASE_API class World* GetWorld(int32_t index);
POLYPHASE_API int32_t GetNumWorlds();
POLYPHASE_API class SignalBus* GetSignalBus();

// Resolves shipped engine content ("Assets/", "Scripts/") to a readable path.
// Prefers the root-relative "Engine/<subDir>" and falls back to
// "<projectDir>Engine/<subDir>", which is where console packages end up when
// they're deployed into a per-project folder on an SD card.
POLYPHASE_API std::string GetEngineContentDir(const char* subDir);

POLYPHASE_API struct EngineState* GetEngineState();
POLYPHASE_API const struct EngineConfig* GetEngineConfig();
POLYPHASE_API struct EngineConfig* GetMutableEngineConfig();

POLYPHASE_API const class Clock* GetAppClock();

// Default scene names to search for when no explicit scene is specified
// Developers can modify this list to add custom default scene names
extern const std::vector<std::string>& GetDefaultSceneNames();
void SetDefaultSceneNames(const std::vector<std::string>& names);


bool IsShuttingDown();

void LoadProject(const std::string& path, bool discoverAssets = true);

// Close the currently-loaded project — symmetric counterpart to LoadProject.
// Ends PIE, closes edit scenes, purges asset state, unloads the project
// directory, clears loaded Lua scripts, and (when unloadNativeAddons=true)
// FreeLibrary's every loaded native addon DLL. Leaves engine state empty
// (mProjectDirectory == ""); the editor falls back to its no-project UI.
// LoadProject calls this internally before loading a new path; callers that
// just want the close half (e.g. Tools > Close Project, Tier-2 recovery)
// invoke it directly. Editor-only — shipped runtimes have no close concept.
#if EDITOR
void CloseProject(bool unloadNativeAddons = true);
#endif

void EnableConsole(bool enable);

void ResizeWindow(uint32_t width, uint32_t height);

bool IsPlayingInEditor();
bool IsPlaying();

POLYPHASE_API bool IsHeadless();

bool IsGameTickEnabled();

void ReloadAllScripts(bool restartComponents = true);

#if EDITOR
void SetScriptHotReloadEnabled(bool enabled);
bool IsScriptHotReloadEnabled();
#endif

void SetPaused(bool paused);
bool IsPaused();
void FrameStep();

void SetTimeDilation(float timeDilation);
float GetTimeDilation();

void GarbageCollect();

void GatherGlobalProperties(std::vector<Property>& props);

void SetScreenOrientation(ScreenOrientation mode);
ScreenOrientation GetScreenOrientation();

void WriteEngineConfig(std::string path = "");
void ReadEngineConfig(std::string path = "");
void ResetEngineConfig();

// Rewrite the .octp project file's `name=` line in place. Preserves other lines
// (assets=, solution=, etc.). Used to keep the .octp in sync when the project
// name is edited via AppSettings, since the .octp is what packaging reads.
void WriteProjectFile(const std::string& path, const std::string& newName);

void ReadCommandLineArgs(int32_t argc, char** argv);


#if LUA_ENABLED
POLYPHASE_API lua_State* GetLua();
#endif
