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

bool Initialize();

bool Update();

void Shutdown();

void Quit();

POLYPHASE_API class World* GetWorld(int32_t index);
POLYPHASE_API int32_t GetNumWorlds();
POLYPHASE_API class SignalBus* GetSignalBus();

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

void EnableConsole(bool enable);

void ResizeWindow(uint32_t width, uint32_t height);

bool IsPlayingInEditor();
bool IsPlaying();

bool IsHeadless();

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
