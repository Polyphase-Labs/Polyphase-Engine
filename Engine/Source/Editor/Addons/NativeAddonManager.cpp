#if EDITOR

#include "NativeAddonManager.h"
#include "AddonManager.h"
#include "AddonDependencyResolver.h"
#include "ActionManager.h"
#include "EditorState.h"
#include "Engine/Assets/Scene.h"
#include "System/System.h"
#include "System/ModuleLoader.h"
#include "Engine.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/AudioManager.h"
#include "Audio/Audio.h"
#include "Engine/Clock.h"
#include "Engine/Nodes/Node.h"
#include "Engine/Nodes/3D/Node3d.h"
#include "Engine/Asset.h"
#include "Engine/NodeGraph/GraphNode.h"
#include "Engine/Timeline/TimelineClip.h"
#include "Engine/Timeline/TimelineTrack.h"

#if PLATFORM_WINDOWS
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#elif PLATFORM_LINUX
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>   // ::rmdir for TryClearAddonIntermediates fallback
#endif
#include "Engine/Gizmos.h"
#include "Engine/Assets/TinyLLMAsset.h"
#include "Input/Input.h"
#include "Log.h"
#include "Stream.h"
#include "Utilities.h"
#include "Script.h"
#include "Plugins/ImGuiPluginContext.h"

#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include "Plugins/EditorUIHooks.h"

#include "document.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cstdio>
#include <cctype>
#include <cstdlib>  // std::getenv (POLYPHASE_CALL_ADDON_ONUNLOAD opt-in)
#include <cstring>
#include <functional>
#include <algorithm>
#include <sstream>
#include <fstream>

NativeAddonManager* NativeAddonManager::sInstance = nullptr;

// Helper function to create directories recursively
static bool CreateDirectoryRecursive(const std::string& path)
{
    if (path.empty())
    {
        return false;
    }

    // Normalize path separators
    std::string normalizedPath = path;
    for (char& c : normalizedPath)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }

    // Remove trailing slash for processing
    if (normalizedPath.back() == '/')
    {
        normalizedPath.pop_back();
    }

    // If directory already exists, we're done
    if (DoesDirExist(normalizedPath.c_str()))
    {
        return true;
    }

    // Find parent directory
    size_t lastSlash = normalizedPath.find_last_of('/');
    if (lastSlash != std::string::npos && lastSlash > 0)
    {
        std::string parentPath = normalizedPath.substr(0, lastSlash);

        // Skip drive letter on Windows (e.g., "M:")
        bool isDriveRoot = (parentPath.length() == 2 && parentPath[1] == ':');
        if (!isDriveRoot && !DoesDirExist(parentPath.c_str()))
        {
            if (!CreateDirectoryRecursive(parentPath))
            {
                return false;
            }
        }
    }

    // Create this directory
    return SYS_CreateDirectory(normalizedPath.c_str());
}

// Helper function to convert addon name to export macro name
// e.g., "inventory-system-runtime" -> "INVENTORY_SYSTEM_RUNTIME_EXPORTS"
static std::string GenerateExportMacroName(const std::string& addonName)
{
    std::string result;
    for (char c : addonName)
    {
        if (c == '-' || c == ' ')
        {
            result += '_';
        }
        else if (std::isalnum(static_cast<unsigned char>(c)))
        {
            result += std::toupper(static_cast<unsigned char>(c));
        }
    }
    result += "_EXPORTS";
    return result;
}

// Helper function to convert addon name to library name (for .lib files)
// e.g., "inventory-system-runtime" -> "inventory_system_runtime"
static std::string GenerateLibraryName(const std::string& addonName)
{
    std::string result;
    for (char c : addonName)
    {
        if (c == '-')
        {
            result += '_';
        }
        else
        {
            result += c;
        }
    }
    return result;
}

// ===== Engine API Implementation =====

static void PluginLogDebug(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    LogDebug("[Plugin] %s", buffer);
}

static void PluginLogWarning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    LogWarning("[Plugin] %s", buffer);
}

static void PluginLogError(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    LogError("[Plugin] %s", buffer);
}

static lua_State* PluginGetLua()
{
    return GetLua();
}

// ===== World Management =====

static World* PluginGetWorld(int32_t index)
{
    return GetWorld(index);
}

static int32_t PluginGetNumWorlds()
{
    return GetNumWorlds();
}

// ===== Node Operations =====

static Node* PluginSpawnNode(World* world, const char* typeName)
{
    if (world == nullptr || typeName == nullptr)
    {
        return nullptr;
    }
    return world->SpawnNode(typeName);
}

static void PluginDestroyNode(Node* node)
{
    if (node != nullptr)
    {
        node->Destroy();
    }
}

static Node* PluginFindNode(World* world, const char* name)
{
    if (world == nullptr || name == nullptr)
    {
        return nullptr;
    }
    return world->FindNode(name);
}

// ===== Node3D Operations =====

static void PluginNode3D_GetRotation(Node3D* node, float* outX, float* outY, float* outZ)
{
    if (node != nullptr && outX != nullptr && outY != nullptr && outZ != nullptr)
    {
        glm::vec3 rot = node->GetRotationEuler();
        *outX = rot.x;
        *outY = rot.y;
        *outZ = rot.z;
    }
}

static void PluginNode3D_SetRotation(Node3D* node, float x, float y, float z)
{
    if (node != nullptr)
    {
        node->SetRotation(glm::vec3(x, y, z));
    }
}

static void PluginNode3D_AddRotation(Node3D* node, float x, float y, float z)
{
    if (node != nullptr)
    {
        node->AddRotation(glm::vec3(x, y, z));
    }
}

static void PluginNode3D_GetPosition(Node3D* node, float* outX, float* outY, float* outZ)
{
    if (node != nullptr && outX != nullptr && outY != nullptr && outZ != nullptr)
    {
        glm::vec3 pos = node->GetPosition();
        *outX = pos.x;
        *outY = pos.y;
        *outZ = pos.z;
    }
}

static void PluginNode3D_SetPosition(Node3D* node, float x, float y, float z)
{
    if (node != nullptr)
    {
        node->SetPosition(glm::vec3(x, y, z));
    }
}

static void PluginNode3D_GetScale(Node3D* node, float* outX, float* outY, float* outZ)
{
    if (node != nullptr && outX != nullptr && outY != nullptr && outZ != nullptr)
    {
        glm::vec3 scale = node->GetScale();
        *outX = scale.x;
        *outY = scale.y;
        *outZ = scale.z;
    }
}

static void PluginNode3D_SetScale(Node3D* node, float x, float y, float z)
{
    if (node != nullptr)
    {
        node->SetScale(glm::vec3(x, y, z));
    }
}

// ===== Asset System =====

static Asset* PluginLoadAsset(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    return LoadAsset(name);
}

static Asset* PluginFetchAsset(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    return FetchAsset(name);
}

static void PluginUnloadAsset(const char* name)
{
    if (name != nullptr)
    {
        UnloadAsset(name);
    }
}

// ===== TinyLLM =====

static int32_t PluginTinyLLM_Encode(Asset* model, const char* text, bool addBos, bool addEos,
                                     int32_t* outTokens, int32_t maxTokens)
{
    if (!model || !text || !outTokens || maxTokens <= 0)
    {
        return -1;
    }

    TinyLLMAsset* llmAsset = model->As<TinyLLMAsset>();
    if (!llmAsset)
    {
        return -1;
    }

    std::vector<int32_t> tokens;
    llmAsset->Encode(text, addBos, addEos, tokens);

    int32_t count = (int32_t)tokens.size();
    if (count > maxTokens)
    {
        count = maxTokens;
    }

    for (int32_t i = 0; i < count; i++)
    {
        outTokens[i] = tokens[i];
    }

    return count;
}

static int32_t PluginTinyLLM_Decode(Asset* model, int32_t prevToken, int32_t token,
                                     char* outStr, int32_t maxLen)
{
    if (!model || !outStr || maxLen <= 0)
    {
        return -1;
    }

    TinyLLMAsset* llmAsset = model->As<TinyLLMAsset>();
    if (!llmAsset)
    {
        return -1;
    }

    std::string decoded = llmAsset->Decode(prevToken, token);
    int32_t len = (int32_t)decoded.size();

    if (len >= maxLen)
    {
        len = maxLen - 1;
    }

    memcpy(outStr, decoded.c_str(), len);
    outStr[len] = '\0';

    return len;
}

// ===== Audio =====

static void PluginPlaySound2D(SoundWave* sound, float volume, float pitch)
{
    if (sound != nullptr)
    {
        AudioManager::PlaySound2D(sound, volume, pitch, 0.0f, false);
    }
}

static void PluginStopAllSounds()
{
    AudioManager::StopAllSounds();
}

static void PluginSetMasterVolume(float volume)
{
    AudioManager::SetMasterVolume(volume);
}

static float PluginGetMasterVolume()
{
    return AudioManager::GetMasterVolume();
}

// Streaming audio pass-throughs to the low-level AUD_ API. Kept as thin forwarders so
// the plugin ABI is stable across engine versions even if AudioManager grows a streaming
// abstraction on top.
static uint32_t PluginAudio_OpenStream(uint32_t sampleRate, uint32_t numChannels, uint32_t bitsPerSample)
{
    return AUD_OpenStream(sampleRate, numChannels, bitsPerSample);
}
static void PluginAudio_CloseStream(uint32_t streamId)
{
    AUD_CloseStream(streamId);
}
static int32_t PluginAudio_SubmitStreamBuffer(uint32_t streamId, const uint8_t* data, uint32_t byteSize)
{
    return AUD_SubmitStreamBuffer(streamId, data, byteSize);
}
static uint64_t PluginAudio_GetStreamPlayedSamples(uint32_t streamId)
{
    return AUD_GetStreamPlayedSamples(streamId);
}
static void PluginAudio_SetStreamVolume(uint32_t streamId, float volume)
{
    AUD_SetStreamVolume(streamId, volume);
}
static void PluginAudio_SetStreamPaused(uint32_t streamId, bool paused)
{
    AUD_SetStreamPaused(streamId, paused);
}
static void PluginAudio_FlushStream(uint32_t streamId)
{
    AUD_FlushStream(streamId);
}

// ===== Input =====

static bool PluginIsKeyDown(int32_t key)
{
    return INP_IsKeyDown(key);
}

static bool PluginIsKeyJustPressed(int32_t key)
{
    return INP_IsKeyJustDown(key);
}

static bool PluginIsKeyJustReleased(int32_t key)
{
    return INP_IsKeyJustUp(key);
}

static bool PluginIsMouseButtonDown(int32_t button)
{
    return INP_IsMouseButtonDown(button);
}

static bool PluginIsMouseButtonJustPressed(int32_t button)
{
    return INP_IsMouseButtonJustDown(button);
}

static void PluginGetMousePosition(int32_t* x, int32_t* y)
{
    if (x != nullptr && y != nullptr)
    {
        INP_GetMousePosition(*x, *y);
    }
}

static void PluginGetMouseDelta(int32_t* deltaX, int32_t* deltaY)
{
    if (deltaX != nullptr && deltaY != nullptr)
    {
        INP_GetMouseDelta(*deltaX, *deltaY);
    }
}

static int32_t PluginGetScrollWheelDelta()
{
    return INP_GetScrollWheelDelta();
}

// ===== Time =====

static float PluginGetDeltaTime()
{
    const Clock* clock = GetAppClock();
    return clock ? clock->DeltaTime() : 0.0f;
}

static float PluginGetElapsedTime()
{
    const Clock* clock = GetAppClock();
    return clock ? clock->GetTime() : 0.0f;
}

// ===== Lua Wrappers =====
// These forward calls to the engine's Lua library so plugins don't need to link against Lua

static void Plugin_lua_settop(lua_State* L, int idx) { lua_settop(L, idx); }
static void Plugin_lua_pushvalue(lua_State* L, int idx) { lua_pushvalue(L, idx); }
static void Plugin_lua_pop(lua_State* L, int n) { lua_pop(L, n); }
static int Plugin_lua_gettop(lua_State* L) { return lua_gettop(L); }

static int Plugin_lua_type(lua_State* L, int idx) { return lua_type(L, idx); }
static int Plugin_lua_isfunction(lua_State* L, int idx) { return lua_isfunction(L, idx); }
static int Plugin_lua_istable(lua_State* L, int idx) { return lua_istable(L, idx); }
static int Plugin_lua_isuserdata(lua_State* L, int idx) { return lua_isuserdata(L, idx); }
static int Plugin_lua_isnil(lua_State* L, int idx) { return lua_isnil(L, idx); }

static int Plugin_lua_toboolean(lua_State* L, int idx) { return lua_toboolean(L, idx); }
static double Plugin_lua_tonumber(lua_State* L, int idx) { return (double)lua_tonumber(L, idx); }
static const char* Plugin_lua_tostring(lua_State* L, int idx) { return lua_tostring(L, idx); }
static void* Plugin_lua_touserdata(lua_State* L, int idx) { return lua_touserdata(L, idx); }

static void Plugin_lua_pushnil(lua_State* L) { lua_pushnil(L); }
static void Plugin_lua_pushboolean(lua_State* L, int b) { lua_pushboolean(L, b); }
static void Plugin_lua_pushnumber(lua_State* L, double n) { lua_pushnumber(L, (lua_Number)n); }
static void Plugin_lua_pushstring(lua_State* L, const char* s) { lua_pushstring(L, s); }
static void Plugin_lua_pushinteger(lua_State* L, long long n) { lua_pushinteger(L, (lua_Integer)n); }

static void* Plugin_lua_newuserdata(lua_State* L, size_t sz) { return lua_newuserdata(L, sz); }

static void Plugin_lua_createtable(lua_State* L, int narr, int nrec) { lua_createtable(L, narr, nrec); }
static void Plugin_lua_setfield(lua_State* L, int idx, const char* k) { lua_setfield(L, idx, k); }
static void Plugin_lua_getfield(lua_State* L, int idx, const char* k) { lua_getfield(L, idx, k); }
static void Plugin_lua_setglobal(lua_State* L, const char* name) { lua_setglobal(L, name); }
static void Plugin_lua_getglobal(lua_State* L, const char* name) { lua_getglobal(L, name); }
static void Plugin_lua_rawset(lua_State* L, int idx) { lua_rawset(L, idx); }
static void Plugin_lua_rawget(lua_State* L, int idx) { lua_rawget(L, idx); }
static void Plugin_lua_settable(lua_State* L, int idx) { lua_settable(L, idx); }
static void Plugin_lua_gettable(lua_State* L, int idx) { lua_gettable(L, idx); }

static int Plugin_lua_setmetatable(lua_State* L, int objindex) { return lua_setmetatable(L, objindex); }
static int Plugin_lua_getmetatable(lua_State* L, int objindex) { return lua_getmetatable(L, objindex); }

static int Plugin_luaL_newmetatable(lua_State* L, const char* tname) { return luaL_newmetatable(L, tname); }
static void Plugin_luaL_setmetatable(lua_State* L, const char* tname) { luaL_setmetatable(L, tname); }
static void* Plugin_luaL_checkudata(lua_State* L, int ud, const char* tname) { return luaL_checkudata(L, ud, tname); }
static double Plugin_luaL_checknumber(lua_State* L, int arg) { return (double)luaL_checknumber(L, arg); }
static long long Plugin_luaL_checkinteger(lua_State* L, int arg) { return (long long)luaL_checkinteger(L, arg); }
static const char* Plugin_luaL_checkstring(lua_State* L, int arg) { return luaL_checkstring(L, arg); }
static void Plugin_luaL_setfuncs(lua_State* L, const void* l, int nup) { luaL_setfuncs(L, (const luaL_Reg*)l, nup); }
static void Plugin_luaL_getmetatable(lua_State* L, const char* tname) { luaL_getmetatable(L, tname); }

// ===== Gizmos Wrappers =====
static void PluginGizmos_SetColor(float r, float g, float b, float a) { Gizmos::SetColor(glm::vec4(r, g, b, a)); }
static void PluginGizmos_SetMatrix(const float* m) { Gizmos::SetMatrix(glm::make_mat4(m)); }
static void PluginGizmos_ResetState() { Gizmos::ResetState(); }
static void PluginGizmos_DrawCube(float cx, float cy, float cz, float sx, float sy, float sz) { Gizmos::DrawCube({cx,cy,cz}, {sx,sy,sz}); }
static void PluginGizmos_DrawWireCube(float cx, float cy, float cz, float sx, float sy, float sz) { Gizmos::DrawWireCube({cx,cy,cz}, {sx,sy,sz}); }
static void PluginGizmos_DrawSphere(float cx, float cy, float cz, float radius) { Gizmos::DrawSphere({cx,cy,cz}, radius); }
static void PluginGizmos_DrawWireSphere(float cx, float cy, float cz, float radius) { Gizmos::DrawWireSphere({cx,cy,cz}, radius); }
static void PluginGizmos_DrawLine(float x0, float y0, float z0, float x1, float y1, float z1) { Gizmos::DrawLine({x0,y0,z0}, {x1,y1,z1}); }
static void PluginGizmos_DrawRay(float ox, float oy, float oz, float dx, float dy, float dz) { Gizmos::DrawRay({ox,oy,oz}, {dx,dy,dz}); }

// ===== NativeAddonManager Implementation =====

void NativeAddonManager::Create()
{
    if (sInstance == nullptr)
    {
        sInstance = new NativeAddonManager();
    }
}

void NativeAddonManager::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

NativeAddonManager* NativeAddonManager::Get()
{
    return sInstance;
}

NativeAddonManager::NativeAddonManager()
{
    InitializeEngineAPI();
}

NativeAddonManager::~NativeAddonManager()
{
    // Unload all native addons
    for (auto& pair : mStates)
    {
        if (pair.second.mModuleHandle != nullptr)
        {
            UnloadNativeAddon(pair.first);
        }
    }
}

void NativeAddonManager::InitializeEngineAPI()
{
    // Logging
    mEngineAPI.LogDebug = PluginLogDebug;
    mEngineAPI.LogWarning = PluginLogWarning;
    mEngineAPI.LogError = PluginLogError;

    // Lua
    mEngineAPI.GetLua = PluginGetLua;

    // Lua Wrappers (Lua_ prefix to avoid macro conflicts)
    mEngineAPI.Lua_settop = Plugin_lua_settop;
    mEngineAPI.Lua_pushvalue = Plugin_lua_pushvalue;
    mEngineAPI.Lua_pop = Plugin_lua_pop;
    mEngineAPI.Lua_gettop = Plugin_lua_gettop;

    mEngineAPI.Lua_type = Plugin_lua_type;
    mEngineAPI.Lua_isfunction = Plugin_lua_isfunction;
    mEngineAPI.Lua_istable = Plugin_lua_istable;
    mEngineAPI.Lua_isuserdata = Plugin_lua_isuserdata;
    mEngineAPI.Lua_isnil = Plugin_lua_isnil;

    mEngineAPI.Lua_toboolean = Plugin_lua_toboolean;
    mEngineAPI.Lua_tonumber = Plugin_lua_tonumber;
    mEngineAPI.Lua_tostring = Plugin_lua_tostring;
    mEngineAPI.Lua_touserdata = Plugin_lua_touserdata;

    mEngineAPI.Lua_pushnil = Plugin_lua_pushnil;
    mEngineAPI.Lua_pushboolean = Plugin_lua_pushboolean;
    mEngineAPI.Lua_pushnumber = Plugin_lua_pushnumber;
    mEngineAPI.Lua_pushstring = Plugin_lua_pushstring;
    mEngineAPI.Lua_pushinteger = Plugin_lua_pushinteger;

    mEngineAPI.Lua_newuserdata = Plugin_lua_newuserdata;

    mEngineAPI.Lua_createtable = Plugin_lua_createtable;
    mEngineAPI.Lua_setfield = Plugin_lua_setfield;
    mEngineAPI.Lua_getfield = Plugin_lua_getfield;
    mEngineAPI.Lua_setglobal = Plugin_lua_setglobal;
    mEngineAPI.Lua_getglobal = Plugin_lua_getglobal;
    mEngineAPI.Lua_rawset = Plugin_lua_rawset;
    mEngineAPI.Lua_rawget = Plugin_lua_rawget;
    mEngineAPI.Lua_settable = Plugin_lua_settable;
    mEngineAPI.Lua_gettable = Plugin_lua_gettable;

    mEngineAPI.Lua_setmetatable = Plugin_lua_setmetatable;
    mEngineAPI.Lua_getmetatable = Plugin_lua_getmetatable;

    mEngineAPI.LuaL_newmetatable = Plugin_luaL_newmetatable;
    mEngineAPI.LuaL_setmetatable = Plugin_luaL_setmetatable;
    mEngineAPI.LuaL_checkudata = Plugin_luaL_checkudata;
    mEngineAPI.LuaL_checknumber = Plugin_luaL_checknumber;
    mEngineAPI.LuaL_checkinteger = Plugin_luaL_checkinteger;
    mEngineAPI.LuaL_checkstring = Plugin_luaL_checkstring;
    mEngineAPI.LuaL_setfuncs = Plugin_luaL_setfuncs;
    mEngineAPI.LuaL_getmetatable = Plugin_luaL_getmetatable;

    // World Management
    mEngineAPI.GetWorld = PluginGetWorld;
    mEngineAPI.GetNumWorlds = PluginGetNumWorlds;

    // Node Operations
    mEngineAPI.SpawnNode = PluginSpawnNode;
    mEngineAPI.DestroyNode = PluginDestroyNode;
    mEngineAPI.FindNode = PluginFindNode;

    // Node3D Operations
    mEngineAPI.Node3D_GetRotation = PluginNode3D_GetRotation;
    mEngineAPI.Node3D_SetRotation = PluginNode3D_SetRotation;
    mEngineAPI.Node3D_AddRotation = PluginNode3D_AddRotation;
    mEngineAPI.Node3D_GetPosition = PluginNode3D_GetPosition;
    mEngineAPI.Node3D_SetPosition = PluginNode3D_SetPosition;
    mEngineAPI.Node3D_GetScale = PluginNode3D_GetScale;
    mEngineAPI.Node3D_SetScale = PluginNode3D_SetScale;

    // Asset System
    mEngineAPI.LoadAsset = PluginLoadAsset;
    mEngineAPI.FetchAsset = PluginFetchAsset;
    mEngineAPI.UnloadAsset = PluginUnloadAsset;

    // TinyLLM
    mEngineAPI.TinyLLM_Encode = PluginTinyLLM_Encode;
    mEngineAPI.TinyLLM_Decode = PluginTinyLLM_Decode;

    // Audio
    mEngineAPI.PlaySound2D = PluginPlaySound2D;
    mEngineAPI.StopAllSounds = PluginStopAllSounds;
    mEngineAPI.SetMasterVolume = PluginSetMasterVolume;
    mEngineAPI.GetMasterVolume = PluginGetMasterVolume;

    // Streaming audio
    mEngineAPI.Audio_OpenStream             = PluginAudio_OpenStream;
    mEngineAPI.Audio_CloseStream            = PluginAudio_CloseStream;
    mEngineAPI.Audio_SubmitStreamBuffer     = PluginAudio_SubmitStreamBuffer;
    mEngineAPI.Audio_GetStreamPlayedSamples = PluginAudio_GetStreamPlayedSamples;
    mEngineAPI.Audio_SetStreamVolume        = PluginAudio_SetStreamVolume;
    mEngineAPI.Audio_SetStreamPaused        = PluginAudio_SetStreamPaused;
    mEngineAPI.Audio_FlushStream            = PluginAudio_FlushStream;

    // Input
    mEngineAPI.IsKeyDown = PluginIsKeyDown;
    mEngineAPI.IsKeyJustPressed = PluginIsKeyJustPressed;
    mEngineAPI.IsKeyJustReleased = PluginIsKeyJustReleased;
    mEngineAPI.IsMouseButtonDown = PluginIsMouseButtonDown;
    mEngineAPI.IsMouseButtonJustPressed = PluginIsMouseButtonJustPressed;
    mEngineAPI.GetMousePosition = PluginGetMousePosition;
    mEngineAPI.GetMouseDelta = PluginGetMouseDelta;
    mEngineAPI.GetScrollWheelDelta = PluginGetScrollWheelDelta;

    // Time
    mEngineAPI.GetDeltaTime = PluginGetDeltaTime;
    mEngineAPI.GetElapsedTime = PluginGetElapsedTime;

    // Gizmos
    mEngineAPI.Gizmos_SetColor = PluginGizmos_SetColor;
    mEngineAPI.Gizmos_SetMatrix = PluginGizmos_SetMatrix;
    mEngineAPI.Gizmos_ResetState = PluginGizmos_ResetState;
    mEngineAPI.Gizmos_DrawCube = PluginGizmos_DrawCube;
    mEngineAPI.Gizmos_DrawWireCube = PluginGizmos_DrawWireCube;
    mEngineAPI.Gizmos_DrawSphere = PluginGizmos_DrawSphere;
    mEngineAPI.Gizmos_DrawWireSphere = PluginGizmos_DrawWireSphere;
    mEngineAPI.Gizmos_DrawLine = PluginGizmos_DrawLine;
    mEngineAPI.Gizmos_DrawRay = PluginGizmos_DrawRay;

    // Editor UI (will be set when EditorUIHookManager is initialized)
    mEngineAPI.editorUI = nullptr;

    // ImGui context for plugins
    mEngineAPI.GetImGuiContext = [](ImGuiPluginContext* outCtx) {
        GetImGuiPluginContext(outCtx);
    };
}

void NativeAddonManager::DiscoverNativeAddons()
{
    LogDebug("Discovering native addons...");

    // Clear existing states (but keep track of loaded modules)
    std::unordered_map<std::string, void*> loadedModules;
    for (auto& pair : mStates)
    {
        if (pair.second.mModuleHandle != nullptr)
        {
            loadedModules[pair.first] = pair.second.mModuleHandle;
        }
    }
    mStates.clear();

    if (AddonManager* addonMgr = AddonManager::Get())
    {
        addonMgr->LoadInstalledAddons();
    }

    // Resolve cross-addon dependencies BEFORE scanning native addons so any
    // freshly-fetched addon folders (created by AddonDependencyResolver) are
    // visible to ScanLocalPackages below. This walks all packages, not just
    // native ones, so non-native (Lua/asset) addons get their deps too.
    {
        std::vector<std::string> order;
        std::vector<std::string> missing;
        std::string err;
        if (!AddonDependencyResolver::ResolveAll(order, missing, err))
        {
            LogWarning("Addon dependency resolution failed: %s", err.c_str());
        }
        mCachedLoadOrder = order;
        if (!missing.empty())
        {
            std::string list;
            for (const std::string& m : missing) { list += m; list += " "; }
            LogWarning("Unresolved addon dependencies: %s", list.c_str());
        }
    }

    // Scan both sources
    ScanLocalPackages();
    ScanInstalledAddons();

    // Restore loaded module handles
    for (auto& pair : loadedModules)
    {
        auto it = mStates.find(pair.first);
        if (it != mStates.end())
        {
            it->second.mModuleHandle = pair.second;
        }
    }

    LogDebug("Discovered %zu native addons", mStates.size());
}

std::vector<std::string> NativeAddonManager::GetLoadOrder() const
{
    std::vector<std::string> out;
    // Filter the project-wide topo order down to addons we actually discovered
    // as native (so non-native packages don't appear and order is stable).
    for (const std::string& id : mCachedLoadOrder)
    {
        if (mStates.find(id) != mStates.end())
        {
            out.push_back(id);
        }
    }
    // Any native addon missing from the cached order (e.g. resolver hasn't run,
    // or the addon has no declared deps and wasn't visited) goes at the end.
    for (const auto& pair : mStates)
    {
        if (std::find(out.begin(), out.end(), pair.first) == out.end())
        {
            out.push_back(pair.first);
        }
    }
    return out;
}

void NativeAddonManager::ScanLocalPackages()
{
    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    if (projectDir.empty())
    {
        return;
    }

    std::string packagesDir = projectDir + "Packages/";
    if (!DoesDirExist(packagesDir.c_str()))
    {
        return;
    }

    DirEntry dirEntry;
    SYS_OpenDirectory(packagesDir, dirEntry);

    while (dirEntry.mValid)
    {
        // Use strcmp for char[] comparison (not pointer comparison)
        if (dirEntry.mDirectory &&
            strcmp(dirEntry.mFilename, ".") != 0 &&
            strcmp(dirEntry.mFilename, "..") != 0)
        {
            std::string addonPath = packagesDir + dirEntry.mFilename + "/";
            std::string packageJsonPath = addonPath + "package.json";

            if (SYS_DoesFileExist(packageJsonPath.c_str(), false))
            {
                NativeModuleMetadata metadata;
                ContentMetadata content;
                if (ParsePackageJson(packageJsonPath, metadata, &content) && metadata.mHasNative)
                {
                    NativeAddonState state;
                    state.mAddonId = dirEntry.mFilename;
                    state.mSourcePath = addonPath;
                    state.mNativeMetadata = metadata;
                    state.mContentMetadata = content;

                    mStates[state.mAddonId] = state;
                    LogDebug("Found local native addon: %s", state.mAddonId.c_str());

                    // Update IDE config (ensures paths are correct if engine moved)
                    GenerateIDEConfig(addonPath);
                }
            }
        }

        SYS_IterateDirectory(dirEntry);
    }
    SYS_CloseDirectory(dirEntry);
}

void NativeAddonManager::ScanInstalledAddons()
{
    AddonManager* addonMgr = AddonManager::Get();
    if (addonMgr == nullptr)
    {
        return;
    }

    const std::vector<InstalledAddon>& installed = addonMgr->GetInstalledAddons();
    std::string cacheDir = addonMgr->GetAddonCacheDirectory();

    for (const InstalledAddon& inst : installed)
    {
        // Skip if already found in local packages
        if (mStates.find(inst.mId) != mStates.end())
        {
            continue;
        }

        // Check cache for this addon
        std::string addonCachePath = cacheDir + "/" + inst.mId + "/";
        std::string packageJsonPath = addonCachePath + "package.json";

        if (SYS_DoesFileExist(packageJsonPath.c_str(), false))
        {
            NativeModuleMetadata metadata;
            ContentMetadata content;
            if (ParsePackageJson(packageJsonPath, metadata, &content) && metadata.mHasNative)
            {
                NativeAddonState state;
                state.mAddonId = inst.mId;
                state.mSourcePath = addonCachePath;
                state.mNativeMetadata = metadata;
                state.mContentMetadata = content;

                mStates[state.mAddonId] = state;
                LogDebug("Found installed native addon: %s", state.mAddonId.c_str());
            }
        }
    }
}

bool NativeAddonManager::ParsePackageJson(const std::string& path, NativeModuleMetadata& outMetadata, ContentMetadata* outContent)
{
    outMetadata = NativeModuleMetadata{};

    Stream stream;
    if (!stream.ReadFile(path.c_str(), false))
    {
        return false;
    }

    std::string jsonStr(stream.GetData(), stream.GetSize());
    rapidjson::Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError())
    {
        return false;
    }

    // Helper: parse a "dependencies" value (object map or legacy string array) into outDeps.
    auto parseDependencies = [&](const rapidjson::Value& deps, std::vector<AddonDependencySpec>& outDeps) {
        if (deps.IsObject())
        {
            for (rapidjson::Value::ConstMemberIterator it = deps.MemberBegin(); it != deps.MemberEnd(); ++it)
            {
                if (!it->name.IsString()) continue;
                std::string id = it->name.GetString();
                std::string value = it->value.IsString() ? it->value.GetString() : std::string();
                outDeps.push_back(AddonDependencySpec::FromValue(id, value));
            }
        }
        else if (deps.IsArray())
        {
            for (rapidjson::SizeType i = 0; i < deps.Size(); ++i)
            {
                if (!deps[i].IsString()) continue;
                outDeps.push_back(AddonDependencySpec::FromValue(deps[i].GetString(), std::string()));
            }
        }
    };

    // Top-level shared content metadata (works for native + non-native addons).
    if (outContent != nullptr)
    {
        *outContent = ContentMetadata{};
        if (doc.HasMember("name") && doc["name"].IsString())
        {
            outContent->mId = doc["name"].GetString();
            outContent->mName = outContent->mId;
        }
        if (doc.HasMember("author") && doc["author"].IsString())      outContent->mAuthor = doc["author"].GetString();
        if (doc.HasMember("description") && doc["description"].IsString()) outContent->mDescription = doc["description"].GetString();
        if (doc.HasMember("url") && doc["url"].IsString())            outContent->mUrl = doc["url"].GetString();
        if (doc.HasMember("version") && doc["version"].IsString())    outContent->mVersion = doc["version"].GetString();
        if (doc.HasMember("updated") && doc["updated"].IsString())    outContent->mUpdated = doc["updated"].GetString();
        if (doc.HasMember("onInstall") && doc["onInstall"].IsString()) outContent->mOnInstallScript = doc["onInstall"].GetString();

        if (doc.HasMember("dependencies"))
        {
            parseDependencies(doc["dependencies"], outContent->mDependencies);
        }
    }

    if (!doc.HasMember("native") || !doc["native"].IsObject())
    {
        // Non-native addon (Lua/asset-only). Top-level content has already been
        // populated above. Return true so the caller can use the ContentMetadata;
        // callers that only want native addons still gate on outMetadata.mHasNative.
        return true;
    }

    const rapidjson::Value& native = doc["native"];
    outMetadata.mHasNative = true;

    if (native.HasMember("target") && native["target"].IsString())
    {
        std::string target = native["target"].GetString();
        outMetadata.mTarget = (target == "editor") ?
            NativeAddonTarget::EditorOnly : NativeAddonTarget::EngineAndEditor;
    }

    if (native.HasMember("sourceDir") && native["sourceDir"].IsString())
    {
        outMetadata.mSourceDir = native["sourceDir"].GetString();
    }

    if (native.HasMember("binaryName") && native["binaryName"].IsString())
    {
        outMetadata.mBinaryName = native["binaryName"].GetString();
    }

    if (native.HasMember("entrySymbol") && native["entrySymbol"].IsString())
    {
        outMetadata.mEntrySymbol = native["entrySymbol"].GetString();
    }

    if (native.HasMember("apiVersion") && native["apiVersion"].IsUint())
    {
        outMetadata.mPluginApiVersion = native["apiVersion"].GetUint();
    }

    if (native.HasMember("resolveMode") && native["resolveMode"].IsString())
    {
        const std::string resolveMode = native["resolveMode"].GetString();
        if (resolveMode == "binary")
        {
            outMetadata.mResolveMode = NativeAddonResolveMode::Binary;
        }
        else
        {
            outMetadata.mResolveMode = NativeAddonResolveMode::Source;
        }
    }

    if (native.HasMember("exportDefine") && native["exportDefine"].IsString())
    {
        outMetadata.mExportDefine = native["exportDefine"].GetString();
    }

    // Legacy: native.dependencies. New addons should use top-level "dependencies".
    // We still accept this, but only when top-level "dependencies" is absent on the
    // shared ContentMetadata, so we don't duplicate. Logs a deprecation note.
    if (native.HasMember("dependencies"))
    {
        if (outContent != nullptr && outContent->mDependencies.empty())
        {
            LogWarning("Addon '%s': native.dependencies is deprecated; move to top-level \"dependencies\".", path.c_str());
            parseDependencies(native["dependencies"], outContent->mDependencies);
        }
    }

    // Optional build extras: third-party includes, lib dirs, libs, defines, binary copies.
    // Enables addons to link additional libraries (e.g. FFmpeg) by dropping them alongside
    // the addon source, without touching the engine.
    auto readStringArray = [&](const char* key, std::vector<std::string>& out) {
        if (native.HasMember(key) && native[key].IsArray())
        {
            const rapidjson::Value& arr = native[key];
            for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
            {
                if (arr[i].IsString())
                {
                    out.push_back(arr[i].GetString());
                }
            }
        }
    };
    readStringArray("extraDefines",     outMetadata.mExtraDefines);
    readStringArray("extraIncludeDirs", outMetadata.mExtraIncludeDirs);
    readStringArray("extraLibDirs",     outMetadata.mExtraLibDirs);
    readStringArray("extraLibs",        outMetadata.mExtraLibs);
    readStringArray("copyBinaries",     outMetadata.mCopyBinaries);

    if (native.HasMember("binaries") && native["binaries"].IsArray())
    {
        const rapidjson::Value& binaries = native["binaries"];
        for (rapidjson::SizeType i = 0; i < binaries.Size(); ++i)
        {
            if (!binaries[i].IsObject())
            {
                continue;
            }

            const rapidjson::Value& binaryObj = binaries[i];
            NativeBinaryDescriptor descriptor;

            if (binaryObj.HasMember("platform") && binaryObj["platform"].IsString())
            {
                descriptor.mPlatform = binaryObj["platform"].GetString();
            }
            if (binaryObj.HasMember("arch") && binaryObj["arch"].IsString())
            {
                descriptor.mArch = binaryObj["arch"].GetString();
            }
            if (binaryObj.HasMember("config") && binaryObj["config"].IsString())
            {
                descriptor.mConfig = binaryObj["config"].GetString();
            }
            if (binaryObj.HasMember("type") && binaryObj["type"].IsString())
            {
                descriptor.mType = binaryObj["type"].GetString();
            }
            if (binaryObj.HasMember("value") && binaryObj["value"].IsString())
            {
                descriptor.mValue = binaryObj["value"].GetString();
            }
            if (binaryObj.HasMember("checksumSha256") && binaryObj["checksumSha256"].IsString())
            {
                descriptor.mChecksumSha256 = binaryObj["checksumSha256"].GetString();
            }
            if (binaryObj.HasMember("entryPath") && binaryObj["entryPath"].IsString())
            {
                descriptor.mEntryPath = binaryObj["entryPath"].GetString();
            }

            if (descriptor.mType != "releaseAsset" && descriptor.mType != "url" && descriptor.mType != "zip")
            {
                LogWarning("Native addon manifest '%s' has unknown binary descriptor type '%s'; skipping.",
                           path.c_str(), descriptor.mType.c_str());
                continue;
            }

            if (descriptor.mValue.empty())
            {
                continue;
            }

            outMetadata.mBinaries.push_back(descriptor);
        }
    }

    // Per-platform overrides: top-level `nativePerPlatform.<PlatformName>.{extraDefines,
    // extraIncludeDirs, extraLibDirs, extraLibs, copyBinaries}`. Resolved at build
    // time by NativeModuleMetadata::ResolveExtras, which concatenates these onto
    // the common arrays above. Platform names match GetPlatformString(Platform):
    // "Windows", "Linux", "Android", "GameCube", "Wii", "3DS". Unknown keys are
    // accepted but ignored at resolve time.
    if (doc.HasMember("nativePerPlatform") && doc["nativePerPlatform"].IsObject())
    {
        const rapidjson::Value& byPlatform = doc["nativePerPlatform"];
        for (rapidjson::Value::ConstMemberIterator it = byPlatform.MemberBegin();
             it != byPlatform.MemberEnd(); ++it)
        {
            if (!it->name.IsString() || !it->value.IsObject()) continue;

            const std::string platformName = it->name.GetString();
            const rapidjson::Value& block = it->value;

            auto readPlatformArray = [&](const char* key, std::vector<std::string>& out) {
                if (block.HasMember(key) && block[key].IsArray())
                {
                    const rapidjson::Value& arr = block[key];
                    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
                    {
                        if (arr[i].IsString())
                        {
                            out.push_back(arr[i].GetString());
                        }
                    }
                }
            };

            NativeModuleMetadata::PlatformExtras px;
            readPlatformArray("extraDefines",     px.mExtraDefines);
            readPlatformArray("extraIncludeDirs", px.mExtraIncludeDirs);
            readPlatformArray("extraLibDirs",     px.mExtraLibDirs);
            readPlatformArray("extraLibs",        px.mExtraLibs);
            readPlatformArray("copyBinaries",     px.mCopyBinaries);

            outMetadata.mPerPlatform[platformName] = std::move(px);
        }
    }

    return true;
}

std::vector<std::string> NativeAddonManager::GetDiscoveredAddonIds() const
{
    std::vector<std::string> ids;
    ids.reserve(mStates.size());
    for (const auto& pair : mStates)
    {
        ids.push_back(pair.first);
    }
    return ids;
}

std::string NativeAddonManager::ComputeFingerprint(const std::string& addonId)
{
    auto it = mStates.find(addonId);
    if (it == mStates.end())
    {
        return "";
    }

    const NativeAddonState& state = it->second;
    std::string sourceDir = state.mSourcePath + state.mNativeMetadata.mSourceDir + "/";

    if (!DoesDirExist(sourceDir.c_str()))
    {
        return "";
    }

    // Gather all source files and compute hash from their mtimes and sizes
    std::vector<std::string> sourceFiles = GatherSourceFiles(sourceDir);
    if (sourceFiles.empty())
    {
        return "";
    }

    std::sort(sourceFiles.begin(), sourceFiles.end());

    uint64_t hash = 0;
    for (const std::string& file : sourceFiles)
    {
        // Simple hash using file path and mtime
        // In a real implementation, you might want to use actual file content hashing
        for (char c : file)
        {
            hash = hash * 31 + c;
        }

        // Add file size as part of fingerprint
        Stream stream;
        if (stream.ReadFile(file.c_str(), false))
        {
            hash = hash * 31 + stream.GetSize();
        }
    }

    // Also hash package.json so edits to native.* / nativePerPlatform.* (extraDefines,
    // extraIncludeDirs, extraLibDirs, extraLibs, copyBinaries) invalidate the cached
    // build and regenerate build.bat on next reload.
    std::string packageJsonPath = state.mSourcePath + "package.json";
    Stream pkgStream;
    if (pkgStream.ReadFile(packageJsonPath.c_str(), false))
    {
        const char* data = pkgStream.GetData();
        uint32_t size = pkgStream.GetSize();
        for (uint32_t i = 0; i < size; ++i)
        {
            hash = hash * 31 + uint8_t(data[i]);
        }
    }

    // Tag the fingerprint with the host engine's CRT config. The build script
    // picks /MDd vs /MD based on _DEBUG (see GenerateBuildScript), so a DLL
    // compiled by one config cannot be safely loaded by the other -- mismatched
    // CRT heaps and _ITERATOR_DEBUG_LEVEL crash at the first cross-module STL
    // operation. Without this tag, Debug and Release engines share a cache
    // directory and silently reuse each other's ABI-incompatible DLL.
    const char* configTag =
#if defined(_DEBUG)
        "dbg";
#else
        "rel";
#endif

    // W1: Also tag by editor flavor (static vs DLL). When the editor was built
    // as PolyphaseEditor.dll the addon links against the DLL's import lib;
    // when static it links against the exe's own export lib. The two produce
    // ABI-incompatible addon DLLs (different symbol resolution paths), so they
    // mustn't share a cache slot. Detect at compile time via POLYPHASE_DLL_BUILD
    // — same source TU compiles into the DLL when set, into the static lib
    // otherwise.
#if POLYPHASE_DLL_BUILD
    const char* flavorTag = "dll";
#else
    const char* flavorTag = "static";
#endif

    char fingerprint[40];
    snprintf(fingerprint, sizeof(fingerprint), "%s_%s_%016llx", configTag, flavorTag, (unsigned long long)hash);
    return fingerprint;
}

std::vector<std::string> NativeAddonManager::TryClearAddonIntermediates(const std::string& addonId)
{
    std::vector<std::string> locked;

    std::string fingerprint = ComputeFingerprint(addonId);
    if (fingerprint.empty())
    {
        return locked;
    }

    std::string root = GetIntermediateDir(addonId) + fingerprint + "/";
    if (!DoesDirExist(root.c_str()))
    {
        return locked;
    }

    // Walk the dir tree, attempting to delete every file. Files that resist
    // deletion (locked by mspdbsrv.exe holding the .pdb, a debugger holding
    // the .dll/.exp, an open editor with a stale handle, etc.) are reported
    // back to the caller so it can pause the build and prompt the user.
    //
    // We deliberately try to delete EVERY file (not bail on first failure),
    // so the modal lists all blockers at once instead of forcing the user
    // through one prompt per file.
    std::function<void(const std::string&)> walk;
    walk = [&](const std::string& dir)
    {
        DirEntry entry;
        SYS_OpenDirectory(dir, entry);
        if (!entry.mValid)
        {
            return;
        }

        std::vector<std::string> subDirs;

        while (entry.mValid)
        {
            if (strcmp(entry.mFilename, ".") != 0 &&
                strcmp(entry.mFilename, "..") != 0)
            {
                std::string path = dir + entry.mFilename;

                if (entry.mDirectory)
                {
                    subDirs.push_back(path + "/");
                }
                else
                {
                    bool deleted = false;
#if PLATFORM_WINDOWS
                    // Clear read-only / hidden flags first so DeleteFileA isn't
                    // refused on a writable-but-marked-readonly file.
                    SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
                    deleted = (DeleteFileA(path.c_str()) != 0);
#else
                    deleted = (::remove(path.c_str()) == 0);
#endif
                    if (!deleted)
                    {
                        locked.push_back(path);
                    }
                }
            }

            SYS_IterateDirectory(entry);
        }
        SYS_CloseDirectory(entry);

        // Recurse after closing the directory handle (some platforms refuse
        // to delete files while a Find handle on the parent is open).
        for (const auto& sub : subDirs)
        {
            walk(sub);
        }

        // Best-effort dir removal — silently ignored if not empty (e.g. a
        // child file was locked). The next build recreates it as needed.
#if PLATFORM_WINDOWS
        ::RemoveDirectoryA(dir.c_str());
#else
        ::rmdir(dir.c_str());
#endif
    };

    walk(root);

    return locked;
}

std::vector<std::string> NativeAddonManager::GatherSourceFiles(const std::string& sourceDir)
{
    std::vector<std::string> files;

    std::function<void(const std::string&)> scanDir;
    scanDir = [&](const std::string& dir)
    {
        DirEntry dirEntry;
        SYS_OpenDirectory(dir, dirEntry);

        if (!dirEntry.mValid)
        {
            // Directory doesn't exist or can't be opened
            return;
        }

        while (dirEntry.mValid)
        {
            // Use strcmp for char[] comparison (not pointer comparison)
            if (strcmp(dirEntry.mFilename, ".") != 0 && strcmp(dirEntry.mFilename, "..") != 0)
            {
                std::string path = dir + dirEntry.mFilename;

                if (dirEntry.mDirectory)
                {
                    scanDir(path + "/");
                }
                else
                {
                    // Check for C++ source files
                    std::string filename = dirEntry.mFilename;
                    size_t dotPos = filename.find_last_of('.');
                    if (dotPos != std::string::npos)
                    {
                        std::string ext = filename.substr(dotPos);
                        if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp")
                        {
                            files.push_back(path);
                        }
                    }
                }
            }

            SYS_IterateDirectory(dirEntry);
        }
        SYS_CloseDirectory(dirEntry);
    };

    scanDir(sourceDir);
    return files;
}

// ===== Build meta sidecar =====
//
// Format: tiny key=value text file written next to the addon DLL after a
// successful build. Used on subsequent loads to detect "stale relative to the
// current host editor binary" — e.g. user rebuilt Polyphase.exe (which may
// have changed engine ABI) and the cached addon DLL is now untrustworthy.
// Fields:
//   config       = "dbg" / "rel"
//   built_at     = unix epoch seconds, time the addon was built
//   engine_mtime = unix epoch seconds, mtime of Polyphase.exe at build time
//   fingerprint  = the full source-fingerprint string, for sanity

static int64_t StatFileMTime(const std::string& path)
{
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
        return 0;
    return static_cast<int64_t>(info.st_mtime);
}

static std::string GetMetaPathForOutput(const std::string& outputPath)
{
    return outputPath + ".meta";
}

static const char* HostConfigTag()
{
#if defined(_DEBUG)
    return "dbg";
#else
    return "rel";
#endif
}

static int64_t GetEngineBinaryMTime()
{
    std::string exePath = SYS_GetExecutablePath();
    return exePath.empty() ? 0 : StatFileMTime(exePath);
}

void NativeAddonManager::WriteAddonBuildMeta(const std::string& outputPath,
                                             const std::string& fingerprint)
{
    std::string metaPath = GetMetaPathForOutput(outputPath);
    int64_t builtAt    = (int64_t)time(nullptr);
    int64_t engineTime = GetEngineBinaryMTime();

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "config=%s\nbuilt_at=%lld\nengine_mtime=%lld\nfingerprint=%s\n",
        HostConfigTag(),
        (long long)builtAt,
        (long long)engineTime,
        fingerprint.c_str());

    if (len <= 0 || len >= (int)sizeof(buf))
        return;

    FILE* f = fopen(metaPath.c_str(), "wb");
    if (f == nullptr)
    {
        LogWarning("WriteAddonBuildMeta: failed to open %s", metaPath.c_str());
        return;
    }
    fwrite(buf, 1, (size_t)len, f);
    fclose(f);
}

// Returns true when the meta sidecar is missing, malformed, or describes a
// build that doesn't match the current host config / current Polyphase.exe.
bool NativeAddonManager::MetaIndicatesRebuildNeeded(const std::string& outputPath) const
{
    std::string metaPath = GetMetaPathForOutput(outputPath);
    if (!SYS_DoesFileExist(metaPath.c_str(), false))
    {
        return true;  // never built (or pre-meta-format DLL) -> rebuild
    }

    FILE* f = fopen(metaPath.c_str(), "rb");
    if (f == nullptr)
        return true;

    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    std::string content(buf);

    auto readField = [&](const char* key) -> std::string {
        std::string needle = std::string(key) + "=";
        size_t pos = content.find(needle);
        if (pos == std::string::npos) return "";
        size_t valStart = pos + needle.size();
        size_t valEnd = content.find('\n', valStart);
        if (valEnd == std::string::npos) valEnd = content.size();
        return content.substr(valStart, valEnd - valStart);
    };

    std::string metaConfig      = readField("config");
    std::string metaEngineMTime = readField("engine_mtime");

    if (metaConfig != HostConfigTag())
    {
        LogDebug("Addon meta config '%s' != host '%s'; rebuild needed",
                 metaConfig.c_str(), HostConfigTag());
        return true;
    }

    int64_t metaEngine = 0;
    try { metaEngine = std::stoll(metaEngineMTime); } catch (...) { metaEngine = 0; }

    int64_t curEngine = GetEngineBinaryMTime();
    if (metaEngine != 0 && curEngine != 0 && metaEngine != curEngine)
    {
        LogDebug("Addon was built against Polyphase.exe mtime=%lld but current is %lld; rebuild needed",
                 (long long)metaEngine, (long long)curEngine);
        return true;
    }

    return false;
}

bool NativeAddonManager::NeedsBuild(const std::string& addonId)
{
    auto it = mStates.find(addonId);
    if (it == mStates.end())
    {
        return false;
    }

    std::string currentFingerprint = ComputeFingerprint(addonId);
    if (currentFingerprint.empty())
    {
        return false;
    }

    // Check if output exists with current fingerprint
    std::string outputPath = GetOutputPath(addonId, currentFingerprint);
    if (!SYS_DoesFileExist(outputPath.c_str(), false))
    {
        return true;
    }

    // DLL is on disk for the current source fingerprint, but it may be
    // stale relative to the current host editor binary or the wrong CRT
    // config. The meta sidecar carries that information.
    if (MetaIndicatesRebuildNeeded(outputPath))
    {
        return true;
    }

    return false;
}

NativeAddonResolveMode NativeAddonManager::ResolveModeForAddon(const std::string& addonId) const
{
    // Installed settings override manifest default
    AddonManager* am = AddonManager::Get();
    if (am != nullptr)
    {
        const std::vector<InstalledAddon>& installed = am->GetInstalledAddons();
        for (const InstalledAddon& inst : installed)
        {
            if (inst.mId == addonId)
            {
                return inst.mNativeMode;
            }
        }
    }

    // Fall back to manifest default
    auto it = mStates.find(addonId);
    if (it != mStates.end())
    {
        return it->second.mNativeMetadata.mResolveMode;
    }

    return NativeAddonResolveMode::Source;
}

bool NativeAddonManager::IsBinaryDescriptorCompatible(const NativeBinaryDescriptor& descriptor,
                                                       const NativeAddonState& state) const
{
    // Check platform
    std::string currentPlatform;
#if PLATFORM_WINDOWS
    currentPlatform = "Windows";
#elif PLATFORM_LINUX
    currentPlatform = "Linux";
#else
    return false;
#endif

    if (!descriptor.mPlatform.empty() && descriptor.mPlatform != currentPlatform)
    {
        return false;
    }

    // Check architecture
    std::string currentArch = "x64";
    if (!descriptor.mArch.empty() && descriptor.mArch != currentArch)
    {
        return false;
    }

    // Check config (Debug/Release)
#if defined(_DEBUG)
    const char* currentConfig = "Debug";
#else
    const char* currentConfig = "Release";
#endif
    if (!descriptor.mConfig.empty() && descriptor.mConfig != currentConfig)
    {
        return false;
    }

    return true;
}

bool NativeAddonManager::ResolveBinaryModulePath(const std::string& addonId,
                                                  std::string& outModulePath,
                                                  std::string& outStatus,
                                                  std::string& outError)
{
    auto it = mStates.find(addonId);
    if (it == mStates.end())
    {
        outError = "Addon not found";
        outStatus = "Missing Binary";
        return false;
    }

    NativeAddonState& state = it->second;
    const std::string& projectDir = GetEngineState()->mProjectDirectory;

    std::string binaryName = state.mNativeMetadata.mBinaryName;
    if (binaryName.empty())
    {
        binaryName = addonId;
    }

#if PLATFORM_WINDOWS
    std::string binaryFilename = binaryName + ".dll";
#else
    std::string binaryFilename = "lib" + binaryName + ".so";
#endif

#if defined(_DEBUG)
    const char* currentConfig = "Debug";
#else
    const char* currentConfig = "Release";
#endif

    std::string syncedDir = projectDir + "Intermediate/Plugins/" + addonId + "/Synced/";

    // Resolution order 1: Config-specific synced prebuilt (e.g., Synced/Debug/ or Synced/Release/)
    std::string configSyncedDir = syncedDir + currentConfig + "/";
    std::string configSyncedPath = configSyncedDir + binaryFilename;
    if (SYS_DoesFileExist(configSyncedPath.c_str(), false))
    {
        outModulePath = configSyncedPath;
        outStatus = std::string("Synced (") + currentConfig + ")";
        return true;
    }

    // Also check for any DLL/SO in the config-specific synced dir
    if (DoesDirExist(configSyncedDir.c_str()))
    {
        DirEntry dirEntry;
        SYS_OpenDirectory(configSyncedDir, dirEntry);
        while (dirEntry.mValid)
        {
            std::string filename = dirEntry.mFilename;
            if (!dirEntry.mDirectory && filename != "." && filename != "..")
            {
                size_t dotPos = filename.find_last_of('.');
                if (dotPos != std::string::npos)
                {
                    std::string ext = filename.substr(dotPos);
#if PLATFORM_WINDOWS
                    if (ext == ".dll")
#else
                    if (ext == ".so")
#endif
                    {
                        outModulePath = configSyncedDir + filename;
                        outStatus = std::string("Synced (") + currentConfig + ")";
                        SYS_CloseDirectory(dirEntry);
                        return true;
                    }
                }
            }
            SYS_IterateDirectory(dirEntry);
        }
        SYS_CloseDirectory(dirEntry);
    }

    // Resolution order 2: Flat synced prebuilt (backwards compatibility)
    std::string syncedPath = syncedDir + binaryFilename;
    if (SYS_DoesFileExist(syncedPath.c_str(), false))
    {
        outModulePath = syncedPath;
        outStatus = "Synced";
        return true;
    }

    // Also check for any DLL/SO in the flat synced dir
    if (DoesDirExist(syncedDir.c_str()))
    {
        DirEntry dirEntry;
        SYS_OpenDirectory(syncedDir, dirEntry);
        while (dirEntry.mValid)
        {
            std::string filename = dirEntry.mFilename;
            if (!dirEntry.mDirectory && filename != "." && filename != "..")
            {
                size_t dotPos = filename.find_last_of('.');
                if (dotPos != std::string::npos)
                {
                    std::string ext = filename.substr(dotPos);
#if PLATFORM_WINDOWS
                    if (ext == ".dll")
#else
                    if (ext == ".so")
#endif
                    {
                        outModulePath = syncedDir + filename;
                        outStatus = "Synced";
                        SYS_CloseDirectory(dirEntry);
                        return true;
                    }
                }
            }
            SYS_IterateDirectory(dirEntry);
        }
        SYS_CloseDirectory(dirEntry);
    }

    // Resolution order 3: Local intermediate (from prior source build)
    std::string fingerprint = ComputeFingerprint(addonId);
    if (!fingerprint.empty())
    {
        std::string localPath = GetOutputPath(addonId, fingerprint);
        if (SYS_DoesFileExist(localPath.c_str(), false))
        {
            outModulePath = localPath;
            outStatus = "Using Local Intermediate";
            return true;
        }
    }

    // No binary found
    outError = "No precompiled binary available for " + std::string(currentConfig) + " config. Use Sync to download or switch to Source mode.";
    outStatus = "Missing Binary";
    return false;
}

std::string NativeAddonManager::GetIntermediateDir(const std::string& addonId)
{
    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    return projectDir + "Intermediate/Plugins/" + addonId + "/";
}

std::string NativeAddonManager::GetOutputPath(const std::string& addonId, const std::string& fingerprint)
{
    auto it = mStates.find(addonId);
    if (it == mStates.end())
    {
        return "";
    }

    std::string intermediateDir = GetIntermediateDir(addonId);
    std::string binaryName = it->second.mNativeMetadata.mBinaryName;
    if (binaryName.empty())
    {
        binaryName = addonId;
    }

#if PLATFORM_WINDOWS
    return intermediateDir + fingerprint + "/" + binaryName + ".dll";
#else
    return intermediateDir + fingerprint + "/lib" + binaryName + ".so";
#endif
}

bool NativeAddonManager::GenerateBuildScript(const std::string& addonId,
                                             const std::string& outputDir,
                                             const std::string& outputPath,
                                             std::string& outScriptPath)
{
    auto it = mStates.find(addonId);
    if (it == mStates.end())
    {
        return false;
    }

    const NativeAddonState& state = it->second;
    std::string sourceDir = state.mSourcePath + state.mNativeMetadata.mSourceDir + "/";
    std::string polyphasePath = SYS_GetPolyphasePath();

    // Try to load from manifest, fall back to hardcoded paths
    std::vector<std::string> includePaths;
    std::vector<std::string> defines;

    if (!LoadAddonIncludesManifest(includePaths, defines))
    {
        // Fallback to hardcoded paths
        includePaths = {
            "Engine/Source",
            "Engine/Source/Engine",
            "Engine/Source/Plugins",
            "External",
            "External/Assimp",
            "External/Bullet",
            "External/Lua",
            "External/glm",
            "External/Imgui",
            "External/ImGuizmo",
            "External/Vorbis"
        };
        defines = {
            "OCTAVE_PLUGIN_EXPORT",
            "EDITOR=1",
            "LUA_ENABLED=1",
            "GLM_FORCE_RADIANS",
#if PLATFORM_WINDOWS
            "PLATFORM_WINDOWS=1",
            "API_VULKAN=1",
            "NOMINMAX"
#elif PLATFORM_LINUX
            "PLATFORM_LINUX=1",
            "API_VULKAN=1"
#elif PLATFORM_ANDROID
            "PLATFORM_ANDROID=1",
            "API_VULKAN=1"
#elif PLATFORM_3DS
            "PLATFORM_3DS=1",
            "API_C3D=1"
#endif
        };
    }

    // Create output directory (recursively to handle missing parent dirs)
    if (!DoesDirExist(outputDir.c_str()))
    {
        CreateDirectoryRecursive(outputDir);
    }

    std::vector<std::string> sourceFiles = GatherSourceFiles(sourceDir);

    // Resolve per-platform extras for the host platform (this script only ever
    // builds the editor-side hot-reload DLL on the host the editor runs on; the
    // console packaging path uses its own resolve in ActionManager.cpp).
#if PLATFORM_WINDOWS
    const NativeModuleMetadata::PlatformExtras nativeExtras =
        state.mNativeMetadata.ResolveExtras("Windows");
#elif PLATFORM_LINUX
    const NativeModuleMetadata::PlatformExtras nativeExtras =
        state.mNativeMetadata.ResolveExtras("Linux");
#else
    const NativeModuleMetadata::PlatformExtras nativeExtras =
        state.mNativeMetadata.ResolveExtras(""); // common only
#endif

    // Get parent Packages directory for resolving sibling addon dependencies
    std::string packagesDir;
    {
        std::string path = state.mSourcePath;
        while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        {
            path.pop_back();
        }
        size_t lastSlash = path.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            packagesDir = path.substr(0, lastSlash + 1);
        }
    }

#if PLATFORM_WINDOWS
    // Generate a batch file for Windows
    outScriptPath = outputDir + "build.bat";

    std::stringstream ss;
    ss << "@echo off\n";
    ss << "setlocal\n";
    ss << "\n";
    ss << ":: Find Visual Studio\n";
    ss << "set \"VSWHERE=%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe\"\n";
    ss << "for /f \"usebackq tokens=*\" %%i in (`\"%VSWHERE%\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (\n";
    ss << "  set \"VS_PATH=%%i\"\n";
    ss << ")\n";
    ss << "\n";
    ss << "if not defined VS_PATH (\n";
    ss << "  echo ERROR: Visual Studio not found\n";
    ss << "  exit /b 1\n";
    ss << ")\n";
    ss << "\n";
    ss << "call \"%VS_PATH%\\VC\\Auxiliary\\Build\\vcvars64.bat\" >nul 2>&1\n";
    ss << "\n";
    ss << ":: Compile\n";
    // Match the engine's MSVC runtime so STL objects (std::string, std::vector, etc.) that
    // cross the addon/engine DLL boundary have the same layout and iterator-debug state.
    // Mismatching /MD and /MDd yields two CRT heaps and _ITERATOR_DEBUG_LEVEL 0 vs 2, which
    // crashes at the first container destruction across modules (e.g. Node::mName destruction
    // while discovering addon-registered node classes).
    // /std:c++17 matches the engine and the addon .vcxproj template (LanguageStandard
    // stdcpp17). Without this, MSVC defaults to C++14 and addons can't use <filesystem>,
    // structured bindings, etc.
#if defined(_DEBUG)
    ss << "cl.exe /nologo /EHsc /std:c++17 /Od /Zi /LD /MDd /D_DEBUG ";
#else
    ss << "cl.exe /nologo /EHsc /std:c++17 /O2 /LD /MD ";
#endif

    // Add defines from manifest
    for (const std::string& define : defines)
    {
        ss << "/D" << define << " ";
    }

    // Extra defines from the addon's package.json (e.g. POLYPHASE_WITH_FFMPEG=1)
    for (const std::string& define : nativeExtras.mExtraDefines)
    {
        ss << "/D" << define << " ";
    }

    // Add export macro for this plugin
    std::string exportMacro = state.mNativeMetadata.mExportDefine.empty()
        ? GenerateExportMacroName(state.mAddonId)
        : state.mNativeMetadata.mExportDefine;
    ss << "/D" << exportMacro << " ";

    // Add include paths from manifest
    for (const std::string& path : includePaths)
    {
        ss << "/I\"" << polyphasePath << path << "/\" ";
    }
    ss << "/I\"" << sourceDir << "\" ";

    // Add dependency addon Source directories
    for (const AddonDependencySpec& dep : state.mContentMetadata.mDependencies)
    {
        ss << "/I\"" << packagesDir << dep.mId << "/Source/\" ";
    }

    // Extra include directories from the addon's package.json, resolved relative to the
    // addon's package root (e.g. "External/ffmpeg/include")
    for (const std::string& inc : nativeExtras.mExtraIncludeDirs)
    {
        ss << "/I\"" << state.mSourcePath << inc << "/\" ";
    }

    // Add Vulkan SDK include path
    ss << "/I\"%VULKAN_SDK%/Include\" ";

    // Add source files. Filter by *file extension*, not path substring — the
    // earlier `src.find(".c")` check false-matches any addon whose path
    // contains ".c" (e.g. addon ids with `.character.` or `.core`), passing
    // headers to cl which then forwards them to link → LNK1107.
    for (const std::string& src : sourceFiles)
    {
        size_t dotPos = src.find_last_of('.');
        if (dotPos == std::string::npos) continue;
        std::string ext = src.substr(dotPos);
        if (ext == ".cpp" || ext == ".c")
        {
            ss << "\"" << src << "\" ";
        }
    }

    ss << "/Fe\"" << outputPath << "\" ";
    ss << "/link /DLL ";

    // Get executable directory for installed editor lib paths.
    //
    // W1: normalize backslashes to forward slashes. SYS_GetExecutablePath returns
    // the Windows-native path with `\` separators; if any directory along the
    // way contains a space (e.g. `DebugEditor Shared\`), emitting it as
    // `/LIBPATH:"...DebugEditor Shared\"` makes MSVC's CRT argv parser interpret
    // the trailing `\"` as an escaped literal `"`, which leaves the LIBPATH arg
    // unterminated and eats the next several tokens until the real closing `"`.
    // link.exe then sees a stray `.obj` somewhere in the resulting blob and
    // dies with LNK1181. cl/link accept `/` as a path separator on Windows so
    // converting up front sidesteps the escape rule entirely.
    std::string exePath = SYS_GetExecutablePath();
    std::string exeDir;
    {
        size_t lastSlash = exePath.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            exeDir = exePath.substr(0, lastSlash + 1);
        }
        for (char& c : exeDir) { if (c == '\\') c = '/'; }
    }

    // W1: We're running inside the editor exe — and this TU (NativeAddonManager.cpp)
    // is part of the engine source tree, so it compiles either *into* the static
    // editor or *into* PolyphaseEditor.dll. The DLL build defines
    // POLYPHASE_DLL_BUILD=1; the static build does not. That makes the editor
    // flavor a compile-time constant, no filesystem probing required (which had
    // a false-positive when the installer shipped both flavors side-by-side).
#if POLYPHASE_DLL_BUILD
    const bool editorIsDll = true;
    const char* engineLibName = "PolyphaseEditor.lib";
#else
    const bool editorIsDll = false;
    const char* engineLibName = "Polyphase.lib";
#endif
    (void)editorIsDll;  // referenced only by the editorIsDll fingerprint tag below

    // Link against Polyphase import library and Lua library
    // First check installed editor paths (alongside executable)
    ss << "/LIBPATH:\"" << exeDir << "\" ";
    ss << "/LIBPATH:\"" << exeDir << "lib/\" ";
    // Then check development build paths
    ss << "/LIBPATH:\"" << polyphasePath << "/Standalone/Build/Windows/x64/DebugEditor/\" ";
    ss << "/LIBPATH:\"" << polyphasePath << "/Standalone/Build/Windows/x64/ReleaseEditor/\" ";
    // W1: PolyphaseEditor.lib (DLL flavor import lib) is emitted by the engine
    // project itself, not by Standalone. Add the engine's DLL-build dirs so
    // addons built against a DLL editor find the engine import lib.
    ss << "/LIBPATH:\"" << polyphasePath << "/Engine/Build/Windows/x64/DebugEditor Shared/\" ";
    ss << "/LIBPATH:\"" << polyphasePath << "/Engine/Build/Windows/x64/ReleaseEditor Shared/\" ";
    ss << "/LIBPATH:\"" << polyphasePath << "/External/Lua/Build/Windows/x64/DebugEditor/\" ";
    ss << "/LIBPATH:\"" << polyphasePath << "/External/Lua/Build/Windows/x64/ReleaseEditor/\" ";

    // Add dependency addon library paths and libraries
    for (const AddonDependencySpec& dep : state.mContentMetadata.mDependencies)
    {
        ss << "/LIBPATH:\"" << packagesDir << dep.mId << "/Build/Debug/\" ";
        ss << "/LIBPATH:\"" << packagesDir << dep.mId << "/Build/Release/\" ";
    }

    // Extra lib directories from the addon's package.json, resolved relative to the addon root
    for (const std::string& libDir : nativeExtras.mExtraLibDirs)
    {
        ss << "/LIBPATH:\"" << state.mSourcePath << libDir << "/\" ";
    }

    ss << engineLibName << " Lua.lib ";
    for (const AddonDependencySpec& dep : state.mContentMetadata.mDependencies)
    {
        ss << GenerateLibraryName(dep.mId) << ".lib ";
    }

    // Extra libs from the addon's package.json
    for (const std::string& lib : nativeExtras.mExtraLibs)
    {
        ss << lib << " ";
    }

    ss << "\n";
    ss << "\n";
    ss << "if %ERRORLEVEL% neq 0 (\n";
    ss << "  echo Build failed\n";
    ss << "  exit /b 1\n";
    ss << ")\n";
    ss << "\n";

    // Post-build: copy any binary directories (e.g. FFmpeg DLLs) next to the addon DLL so
    // dynamic dependencies resolve at load time.
    if (!nativeExtras.mCopyBinaries.empty())
    {
        // Derive the output directory from outputPath (strip the trailing filename).
        std::string outDir = outputPath;
        size_t lastSlash = outDir.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            outDir = outDir.substr(0, lastSlash + 1);
        }

        for (const std::string& binDir : nativeExtras.mCopyBinaries)
        {
            std::string src = state.mSourcePath + binDir;
            // xcopy flags: /Y overwrite, /E recurse including empty dirs, /I treat dest as dir, /D only-newer
            ss << "xcopy /Y /E /I /D \"" << src << "\" \"" << outDir << "\" >nul\n";
        }
        ss << "\n";
    }

    ss << "echo Build succeeded\n";

    std::string content = ss.str();
    Stream stream(content.c_str(), (uint32_t)content.size());
    stream.WriteFile(outScriptPath.c_str());

#else
    // Generate a shell script for Linux
    outScriptPath = outputDir + "build.sh";

    // FFmpeg via pkg-config when the addon opts in. The package.json's
    // extraIncludeDirs/extraLibs entries describe the Windows External/ffmpeg
    // layout, which doesn't exist on Linux — so on Linux we resolve FFmpeg
    // through the system instead. The marker is the conventional
    // POLYPHASE_WITH_FFMPEG define already used by the FFmpeg-bundling addon.
    // Inspect the resolved (Linux-platform-aware) defines so addons that move
    // POLYPHASE_WITH_FFMPEG under nativePerPlatform.Linux still trigger this.
    bool wantsFFmpeg = false;
    for (const std::string& d : nativeExtras.mExtraDefines)
    {
        if (d.rfind("POLYPHASE_WITH_FFMPEG", 0) == 0)
        {
            wantsFFmpeg = true;
            break;
        }
    }

    std::stringstream ss;
    ss << "#!/bin/bash\n";
    ss << "set -e\n";
    ss << "\n";
    if (wantsFFmpeg)
    {
        ss << "FFMPEG_CFLAGS=$(pkg-config --cflags libavformat libavcodec libavutil libswscale libswresample)\n";
        ss << "FFMPEG_LIBS=$(pkg-config --libs libavformat libavcodec libavutil libswscale libswresample)\n";
        ss << "\n";
    }
    ss << "g++ -shared -fPIC -O2 -std=c++17 \\\n";

    // Add defines from manifest
    for (const std::string& define : defines)
    {
        ss << "  -D" << define << " \\\n";
    }

    // Extra defines from the addon's package.json (e.g. POLYPHASE_WITH_FFMPEG=1).
    for (const std::string& define : nativeExtras.mExtraDefines)
    {
        ss << "  -D" << define << " \\\n";
    }

    // Add export macro for this plugin
    std::string exportMacroLinux = state.mNativeMetadata.mExportDefine.empty()
        ? GenerateExportMacroName(state.mAddonId)
        : state.mNativeMetadata.mExportDefine;
    ss << "  -D" << exportMacroLinux << " \\\n";

    // Add include paths from manifest
    for (const std::string& path : includePaths)
    {
        ss << "  -I\"" << polyphasePath << path << "/\" \\\n";
    }
    ss << "  -I\"" << sourceDir << "\" \\\n";

    // Extra include dirs from the addon's package.json (resolved relative to the addon root).
    // Missing dirs are harmless — g++ ignores them with a warning.
    for (const std::string& incDir : nativeExtras.mExtraIncludeDirs)
    {
        ss << "  -I\"" << state.mSourcePath << incDir << "/\" \\\n";
    }

    // Add dependency addon Source directories (packagesDir already computed above for Windows)
    for (const AddonDependencySpec& dep : state.mContentMetadata.mDependencies)
    {
        ss << "  -I\"" << packagesDir << dep.mId << "/Source/\" \\\n";
    }

    // Add Vulkan SDK include path
    ss << "  -I\"$VULKAN_SDK/include\" \\\n";

    if (wantsFFmpeg)
    {
        ss << "  $FFMPEG_CFLAGS \\\n";
    }

    // Add source files (extension match, not path substring — see Windows path).
    for (const std::string& src : sourceFiles)
    {
        size_t dotPos = src.find_last_of('.');
        if (dotPos == std::string::npos) continue;
        std::string ext = src.substr(dotPos);
        if (ext == ".cpp" || ext == ".c")
        {
            ss << "  \"" << src << "\" \\\n";
        }
    }

    // Lua symbols come from the editor executable on Linux: the engine's Linux build
    // compiles External/Lua directly into libEngineEditor.a, and the editor links with
    // -rdynamic, so dlopened addons resolve lua_* via --unresolved-symbols below.
    // (Windows ships a separate Lua.lib; Linux does not.)
    std::string luaLibPathLinux;
    std::vector<std::string> luaConfigsLinux = {"DebugEditor", "ReleaseEditor", "Debug", "Release"};
    for (const std::string& config : luaConfigsLinux)
    {
        std::string testPath = polyphasePath + "External/Lua/Build/Linux/x64/" + config + "/libLua.a";
        if (SYS_DoesFileExist(testPath.c_str(), false))
        {
            luaLibPathLinux = testPath;
            break;
        }
    }
    if (!luaLibPathLinux.empty())
    {
        ss << "  \"" << luaLibPathLinux << "\" \\\n";
    }

    // Link against dependency shared libraries
    for (const AddonDependencySpec& dep : state.mContentMetadata.mDependencies)
    {
        std::string depLibName = GenerateLibraryName(dep.mId);
        ss << "  -L\"" << packagesDir << dep.mId << "/Build/\" \\\n";
        ss << "  -l" << depLibName << " \\\n";
    }

    if (wantsFFmpeg)
    {
        ss << "  $FFMPEG_LIBS \\\n";
    }

    // Engine symbols (Node3D, lua_*, Bullet inlines instantiated via templates, ImGui, etc.)
    // live in the editor executable, not in any .so the addon links against. The
    // executable is built with -rdynamic so dlopen resolves them at load time.
    // Note: --unresolved-symbols=ignore-in-shared-libs is NOT enough — it only suppresses
    // errors from .so deps, not from the .so we're producing. Use ignore-all here.
    ss << "  -Wl,--unresolved-symbols=ignore-all \\\n";
    ss << "  -o \"" << outputPath << "\"\n";
    ss << "\n";
    ss << "echo \"Build succeeded\"\n";

    std::string content = ss.str();
    Stream stream(content.c_str(), (uint32_t)content.size());
    stream.WriteFile(outScriptPath.c_str());

    // Make executable
    std::string chmodCmd = "chmod +x \"" + outScriptPath + "\"";
    SYS_Exec(chmodCmd.c_str(), nullptr);
#endif

    return true;
}

bool NativeAddonManager::BuildNativeAddon(const std::string& addonId, std::string& outError)
{
    auto it = mStates.find(addonId);
    if (it == mStates.end())
    {
        outError = "Addon not found: " + addonId;
        return false;
    }

    NativeAddonState& state = it->second;
    state.mBuildInProgress = true;
    state.mBuildLog.clear();
    state.mBuildError.clear();
    state.mBuildFailureAcknowledged = false;

    LogDebug("Building native addon: %s", addonId.c_str());

    // Pre-build sweep: try to clear stale intermediates before invoking the
    // compiler/linker. If any file is locked (mspdbsrv keeping the .pdb open
    // is the classic LNK1201 trigger), pause the build and surface a modal
    // so the user can release the lock holder and Retry.
    {
        std::vector<std::string> stillLocked = TryClearAddonIntermediates(addonId);
        if (!stillLocked.empty())
        {
            mBlocked.mActive = true;
            mBlocked.mAddonId = addonId;
            mBlocked.mLockedFiles = std::move(stillLocked);
            mBlocked.mIntermediateDir = GetIntermediateDir(addonId);

            outError = "Build blocked: " + std::to_string(mBlocked.mLockedFiles.size())
                     + " intermediate file(s) locked. See modal for paths.";
            state.mBuildInProgress = false;
            state.mBuildSucceeded = false;
            state.mBuildError = outError;
            LogError("Build of '%s' paused: locked intermediate files. See popup.",
                     addonId.c_str());
            return false;
        }
    }

    // Compute fingerprint
    std::string fingerprint = ComputeFingerprint(addonId);
    if (fingerprint.empty())
    {
        outError = "Failed to compute fingerprint";
        state.mBuildInProgress = false;
        state.mBuildSucceeded = false;
        state.mBuildError = outError;
        return false;
    }

    // Get output paths
    std::string intermediateDir = GetIntermediateDir(addonId);
    std::string outputDir = intermediateDir + fingerprint + "/";
    std::string outputPath = GetOutputPath(addonId, fingerprint);

    // Create directories (recursively to handle missing parent dirs)
    if (!CreateDirectoryRecursive(outputDir))
    {
        outError = "Failed to create output directory: " + outputDir;
        state.mBuildInProgress = false;
        state.mBuildSucceeded = false;
        state.mBuildError = outError;
        return false;
    }

    // Generate build script
    std::string scriptPath;
    if (!GenerateBuildScript(addonId, outputDir, outputPath, scriptPath))
    {
        outError = "Failed to generate build script";
        state.mBuildInProgress = false;
        state.mBuildSucceeded = false;
        state.mBuildError = outError;
        return false;
    }

    // Execute build
    std::string stdoutStr;
    int exitCode = 0;

#if PLATFORM_WINDOWS
    std::string cmd = "cmd /c \"" + scriptPath + "\"";
#else
    std::string cmd = "bash \"" + scriptPath + "\"";
#endif

    bool success = SYS_ExecFull(cmd.c_str(), &stdoutStr, nullptr, &exitCode);

    state.mBuildLog = stdoutStr;
    state.mBuildInProgress = false;

    if (!success || exitCode != 0)
    {
        outError = "Build failed with exit code " + std::to_string(exitCode);
        state.mBuildSucceeded = false;
        state.mBuildError = outError + "\n" + stdoutStr;
        LogError("Build failed for %s: %s", addonId.c_str(), stdoutStr.c_str());
        return false;
    }

    // Verify output exists
    if (!SYS_DoesFileExist(outputPath.c_str(), false))
    {
        outError = "Build completed but output file not found: " + outputPath;
        state.mBuildSucceeded = false;
        state.mBuildError = outError;
        return false;
    }

    state.mBuildSucceeded = true;
    state.mFingerprint = fingerprint;

    // Drop a meta sidecar next to the DLL so future loads can detect when
    // the cached binary is stale relative to the current host editor.
    WriteAddonBuildMeta(outputPath, fingerprint);

    LogDebug("Build succeeded for %s", addonId.c_str());

    return true;
}

bool NativeAddonManager::LoadNativeAddon(const std::string& addonId, std::string& outError)
{
    auto it = mStates.find(addonId);
    if (it == mStates.end())
    {
        outError = "Addon not found: " + addonId;
        return false;
    }

    NativeAddonState& state = it->second;

    // Already loaded?
    if (state.mModuleHandle != nullptr)
    {
        outError = "Addon already loaded";
        return false;
    }

    // Determine active resolve mode
    NativeAddonResolveMode activeMode = ResolveModeForAddon(addonId);

    // One-shot override: a prior Reload Native Addons request flagged this
    // addon to be loaded as if it were source-mode regardless of what its
    // package.json says. Consume the flag here so a normal load on the next
    // session is unaffected. See ReloadNativeAddonsWithProjectRestart for
    // the producer side.
    auto forceSourceIt = mForceSourceForNextLoad.find(addonId);
    if (forceSourceIt != mForceSourceForNextLoad.end())
    {
        mForceSourceForNextLoad.erase(forceSourceIt);
        if (activeMode != NativeAddonResolveMode::Source)
        {
            LogDebug("LoadNativeAddon: forcing %s to source-compile (Reload Native Addons override)",
                     addonId.c_str());
            activeMode = NativeAddonResolveMode::Source;
        }
    }

    state.mActiveResolveMode = activeMode;

    std::string modulePath;
    std::string fingerprint;

    if (activeMode == NativeAddonResolveMode::Binary)
    {
        // Binary mode: use precompiled binary, never compile
        std::string status;
        bool resolved = ResolveBinaryModulePath(addonId, modulePath, status, outError);

        // Auto-sync on first load: if the addon has remote binary descriptors
        // in its package.json (the CI release workflow populates these) and
        // the local Synced/ cache is empty, fetch the binary now. This is
        // how "addon install" becomes a zero-click experience — the editor
        // pulls the matching DLL/SO from the addon's GitHub release the
        // first time it tries to load. Subsequent loads use the cached
        // synced binary as before. Source iteration ("I edited Source/*.cpp")
        // is handled separately by the Reload Native Addons flow which goes
        // through BuildNativeAddon → fingerprinted intermediate dir.
        if (!resolved)
        {
            const Addon* addonInfo = nullptr;
            if (AddonManager* am = AddonManager::Get())
            {
                addonInfo = am->FindAddon(addonId);
            }
            const bool hasRemoteBinaries = addonInfo != nullptr &&
                                           !addonInfo->mNative.mBinaries.empty();
            if (hasRemoteBinaries)
            {
                LogDebug("Auto-syncing prebuilt binary for %s (first-install fetch)",
                         addonId.c_str());
                std::string syncError;
                if (AddonManager::Get()->SyncNativeAddonBinary(addonId, syncError))
                {
                    // Re-probe — sync drops the binary into the Synced cache
                    // dir at the path ResolveBinaryModulePath expects.
                    resolved = ResolveBinaryModulePath(addonId, modulePath, status, outError);
                    if (!resolved)
                    {
                        LogWarning("Auto-sync reported success for %s but binary still "
                                   "not resolvable: %s", addonId.c_str(), outError.c_str());
                    }
                }
                else
                {
                    LogWarning("Auto-sync failed for %s: %s", addonId.c_str(), syncError.c_str());
                    // Fall through to the existing failure handling below.
                }
            }
        }

        if (!resolved)
        {
            // In Debug builds, fall back to source compilation if no Debug binary available
#if defined(_DEBUG)
            LogWarning("No Debug binary available for %s, falling back to source compilation", addonId.c_str());
            state.mBinaryStatus = "Debug Fallback (Source)";
            state.mLoadedFromBinary = false;

            // Fall through to source compilation
            if (NeedsBuild(addonId))
            {
                if (!BuildNativeAddon(addonId, outError))
                {
                    return false;
                }
            }

            fingerprint = state.mFingerprint;
            if (fingerprint.empty())
            {
                fingerprint = ComputeFingerprint(addonId);
            }

            modulePath = GetOutputPath(addonId, fingerprint);
#else
            // Release mode: binary is required
            state.mBinaryStatus = status;
            state.mLoadedFromBinary = false;
            LogWarning("Binary mode load failed for %s: %s", addonId.c_str(), outError.c_str());
            return false;
#endif
        }
        else
        {
            state.mBinaryStatus = status;
            state.mLoadedFromBinary = true;
            // Binary mode doesn't use fingerprinting
            fingerprint.clear();
        }
    }
    else
    {
        // Source mode: build if needed, use fingerprinted output
        state.mLoadedFromBinary = false;
        state.mBinaryStatus.clear();

        if (NeedsBuild(addonId))
        {
            if (!BuildNativeAddon(addonId, outError))
            {
                return false;
            }
        }

        fingerprint = state.mFingerprint;
        if (fingerprint.empty())
        {
            fingerprint = ComputeFingerprint(addonId);
        }

        modulePath = GetOutputPath(addonId, fingerprint);
    }

    if (!SYS_DoesFileExist(modulePath.c_str(), false))
    {
        outError = "Module file not found: " + modulePath;
        return false;
    }

    // Load the module
    LogDebug("Loading native addon: %s from %s", addonId.c_str(), modulePath.c_str());

    void* handle = MOD_Load(modulePath.c_str());
    if (handle == nullptr)
    {
        outError = "Failed to load module: " + std::string(MOD_GetError());
        return false;
    }

    // Get entry point
    PolyphasePlugin_GetDescFunc getDesc = (PolyphasePlugin_GetDescFunc)MOD_Symbol(handle, state.mNativeMetadata.mEntrySymbol.c_str());
    if (getDesc == nullptr)
    {
        MOD_Unload(handle);
        outError = "Entry symbol not found: " + state.mNativeMetadata.mEntrySymbol;
        return false;
    }

    // Get plugin descriptor
    PolyphasePluginDesc desc = {};
    if (getDesc(&desc) != 0)
    {
        MOD_Unload(handle);
        outError = "Failed to get plugin descriptor";
        return false;
    }

    // Verify API version (accept version 1 or 2 for backward compatibility)
    if (desc.apiVersion < 1 || desc.apiVersion > OCTAVE_PLUGIN_API_VERSION)
    {
        MOD_Unload(handle);
        outError = "API version mismatch: plugin=" + std::to_string(desc.apiVersion) +
                   ", max supported=" + std::to_string(OCTAVE_PLUGIN_API_VERSION);
        return false;
    }

    // For v1 plugins, zero out the v2 fields they don't know about
    if (desc.apiVersion < 2)
    {
        desc.OnEditorPreInit = nullptr;
        desc.OnEditorReady = nullptr;
    }

    // Call OnLoad
    if (desc.OnLoad != nullptr)
    {
        int result = desc.OnLoad(&mEngineAPI);
        if (result != 0)
        {
            MOD_Unload(handle);
            outError = "Plugin OnLoad failed with code " + std::to_string(result);
            return false;
        }
    }

    // Register types if provided
    if (desc.RegisterTypes != nullptr)
    {
        // TODO: Pass actual node factory
        desc.RegisterTypes(nullptr);
    }

    // Register script functions if provided
    if (desc.RegisterScriptFuncs != nullptr)
    {
        desc.RegisterScriptFuncs(GetLua());
    }

    // Register editor UI if provided and in editor
    if (desc.RegisterEditorUI != nullptr && mEngineAPI.editorUI != nullptr)
    {
        // Generate hook ID from addon ID
        uint64_t hookId = 0;
        for (char c : addonId)
        {
            hookId = hookId * 31 + c;
        }
        desc.RegisterEditorUI(mEngineAPI.editorUI, hookId);
    }

    // Store state
    state.mModuleHandle = handle;
    state.mLoadedPath = modulePath;
    state.mDesc = desc;
    state.mDescValid = true;
    state.mFingerprint = fingerprint;

    LogDebug("Successfully loaded native addon: %s (v%s)", desc.pluginName, desc.pluginVersion);

    // Rehydrate any assets PurgeAssetsFromModule unloaded in the matching
    // UnloadNativeAddon. The new DLL has re-registered its factories by this
    // point (RegisterTypes / static initializers ran via MOD_Load above), so
    // LoadAsset can recreate the Asset through the fresh factory and populate
    // stub->mAsset again. Without this, save-after-reload no-ops because the
    // stub stays empty until the user manually re-opens the asset.
    if (!state.mPurgedAssetUuids.empty())
    {
        AssetManager* mgr = AssetManager::Get();
        if (mgr != nullptr)
        {
            size_t reloaded = 0;
            for (uint64_t uuid : state.mPurgedAssetUuids)
            {
                AssetStub* stub = mgr->GetAssetStubByUuid(uuid);
                if (stub != nullptr && stub->mAsset == nullptr)
                {
                    mgr->LoadAsset(*stub);
                    if (stub->mAsset != nullptr) ++reloaded;
                }
            }
            LogDebug("Rehydrated %zu/%zu addon-typed asset(s) after reload of %s",
                     reloaded, state.mPurgedAssetUuids.size(), addonId.c_str());
        }
        state.mPurgedAssetUuids.clear();
    }

    return true;
}

namespace
{
#if PLATFORM_WINDOWS
    // Strip factories whose object address falls inside the given DLL module's memory image.
    // Called before FreeLibrary so the engine's factory lists don't hold dangling pointers
    // (which would otherwise trip the duplicate-class-name assert on addon reload).
    void StripFactoriesFromModule(void* moduleHandle)
    {
        if (moduleHandle == nullptr) return;

        MODULEINFO info = {};
        if (!GetModuleInformation(GetCurrentProcess(), (HMODULE)moduleHandle, &info, sizeof(info)))
        {
            LogWarning("StripFactoriesFromModule: GetModuleInformation failed");
            return;
        }

        const uintptr_t base = (uintptr_t)info.lpBaseOfDll;
        const uintptr_t end  = base + info.SizeOfImage;

        auto strip = [&](std::vector<Factory*>& list, const char* label) {
            size_t removed = 0;
            for (auto it = list.begin(); it != list.end(); )
            {
                uintptr_t p = (uintptr_t)(*it);
                if (p >= base && p < end)
                {
                    it = list.erase(it);
                    ++removed;
                }
                else
                {
                    ++it;
                }
            }
            if (removed > 0)
            {
                LogDebug("  stripped %zu %s factories belonging to unloaded module", removed, label);
            }
        };

        strip(Node::GetFactoryList(),          "Node");
        strip(Asset::GetFactoryList(),         "Asset");
        strip(GraphNode::GetFactoryList(),     "GraphNode");
        strip(TimelineClip::GetFactoryList(),  "TimelineClip");
        strip(TimelineTrack::GetFactoryList(), "TimelineTrack");
    }
#elif PLATFORM_LINUX
    // Strip factories whose object address falls inside the given .so module's memory image.
    // Windows parity via dlinfo(RTLD_DI_LINKMAP) for the module base + dladdr per factory.
    void StripFactoriesFromModule(void* moduleHandle)
    {
        if (moduleHandle == nullptr) return;

        struct link_map* lm = nullptr;
        if (dlinfo(moduleHandle, RTLD_DI_LINKMAP, &lm) != 0 || lm == nullptr)
        {
            LogWarning("StripFactoriesFromModule: dlinfo failed (%s)", dlerror() ? dlerror() : "unknown");
            return;
        }
        void* moduleBase = reinterpret_cast<void*>(lm->l_addr);

        auto strip = [&](std::vector<Factory*>& list, const char* label) {
            size_t removed = 0;
            for (auto it = list.begin(); it != list.end(); )
            {
                Dl_info info = {};
                if (dladdr(reinterpret_cast<void*>(*it), &info) != 0 && info.dli_fbase == moduleBase)
                {
                    it = list.erase(it);
                    ++removed;
                }
                else
                {
                    ++it;
                }
            }
            if (removed > 0)
            {
                LogDebug("  stripped %zu %s factories belonging to unloaded module", removed, label);
            }
        };

        strip(Node::GetFactoryList(),          "Node");
        strip(Asset::GetFactoryList(),         "Asset");
        strip(GraphNode::GetFactoryList(),     "GraphNode");
        strip(TimelineClip::GetFactoryList(),  "TimelineClip");
        strip(TimelineTrack::GetFactoryList(), "TimelineTrack");
    }
#else
    void StripFactoriesFromModule(void* /*moduleHandle*/)
    {
        // Other platforms (3DS, GC, Wii, Android) don't support native addon hot-reload yet.
    }
#endif

    // Free any loaded Asset instances whose vtable lives inside the given DLL.
    // Must run before FreeLibrary so each ~Asset() / Asset::Destroy() call
    // dispatches into still-mapped code. Skipping this leaves AssetStubs holding
    // pointers to objects with dangling vtables — the next ImGui frame crashes
    // in DrawAssetItems on stub->mAsset->GetTypeName().
    //
    // Appends the UUID of every purged stub (when non-zero) to outRehydrateUuids
    // so the caller can reload them through the new factory once the DLL is
    // back. The post-reload reload happens in LoadNativeAddon.
    static void PurgeStubsAndCollectUuids(const std::vector<AssetStub*>& stubsToPurge,
                                          std::vector<uint64_t>& outRehydrateUuids)
    {
        if (stubsToPurge.empty()) return;

        LogDebug("PurgeAssetsFromModule: unloading %zu addon-typed asset instance(s)",
                 stubsToPurge.size());

        // Clear inspector if it points at any to-be-freed asset. The stub itself
        // isn't deleted (only mAsset is nulled) so selected-stub pointers stay
        // valid; subsequent LoadAsset will rehydrate from disk through the new
        // factory after the DLL reloads.
        EditorState* es = GetEditorState();
        if (es != nullptr)
        {
            for (AssetStub* stub : stubsToPurge)
            {
                if (es->GetInspectedObject() == stub->mAsset)
                {
                    es->InspectObject(nullptr, true);
                    break;
                }
            }
        }

        // Force-free regardless of refcount: refusing here leaves a live Asset*
        // with a dangling vtable, which is strictly worse than dangling AssetRefs.
        for (AssetStub* stub : stubsToPurge)
        {
            if (stub->mUuid != 0)
            {
                outRehydrateUuids.push_back(stub->mUuid);
            }

            stub->mAsset->Destroy();
            delete stub->mAsset;
            stub->mAsset = nullptr;
        }
    }

#if PLATFORM_WINDOWS
    void PurgeAssetsFromModule(void* moduleHandle, std::vector<uint64_t>& outRehydrateUuids)
    {
        if (moduleHandle == nullptr) return;

        AssetManager* mgr = AssetManager::Get();
        if (mgr == nullptr) return;

        MODULEINFO info = {};
        if (!GetModuleInformation(GetCurrentProcess(), (HMODULE)moduleHandle, &info, sizeof(info)))
        {
            LogWarning("PurgeAssetsFromModule: GetModuleInformation failed");
            return;
        }

        const uintptr_t base = (uintptr_t)info.lpBaseOfDll;
        const uintptr_t end  = base + info.SizeOfImage;

        std::vector<AssetStub*> stubsToPurge;
        for (auto& kv : mgr->GetAssetMap())
        {
            AssetStub* stub = kv.second;
            if (stub == nullptr || stub->mAsset == nullptr) continue;

            // First machine word of any C++ object with virtuals is the vtable
            // pointer. Vtables live in .rdata of the module that compiled the
            // class, so an addon-class instance has its vtable inside [base, end).
            uintptr_t vtable = *reinterpret_cast<uintptr_t*>(stub->mAsset);
            if (vtable >= base && vtable < end)
            {
                stubsToPurge.push_back(stub);
            }
        }

        PurgeStubsAndCollectUuids(stubsToPurge, outRehydrateUuids);
    }
#elif PLATFORM_LINUX
    void PurgeAssetsFromModule(void* moduleHandle, std::vector<uint64_t>& outRehydrateUuids)
    {
        if (moduleHandle == nullptr) return;

        AssetManager* mgr = AssetManager::Get();
        if (mgr == nullptr) return;

        struct link_map* lm = nullptr;
        if (dlinfo(moduleHandle, RTLD_DI_LINKMAP, &lm) != 0 || lm == nullptr)
        {
            LogWarning("PurgeAssetsFromModule: dlinfo failed (%s)", dlerror() ? dlerror() : "unknown");
            return;
        }
        void* moduleBase = reinterpret_cast<void*>(lm->l_addr);

        std::vector<AssetStub*> stubsToPurge;
        for (auto& kv : mgr->GetAssetMap())
        {
            AssetStub* stub = kv.second;
            if (stub == nullptr || stub->mAsset == nullptr) continue;

            void* vtable = *reinterpret_cast<void**>(stub->mAsset);
            Dl_info dlinf = {};
            if (dladdr(vtable, &dlinf) != 0 && dlinf.dli_fbase == moduleBase)
            {
                stubsToPurge.push_back(stub);
            }
        }

        PurgeStubsAndCollectUuids(stubsToPurge, outRehydrateUuids);
    }
#else
    void PurgeAssetsFromModule(void* /*moduleHandle*/, std::vector<uint64_t>& /*outRehydrateUuids*/)
    {
    }
#endif

    // Wrapper around PurgeAssetsFromModule. SEH was removed here for the
    // same C1001 LTCG reason as the OnUnload/RemoveAllHooks wrappers above;
    // the helper is retained as a stable call site for future re-protection.
    bool SafeCallPurgeAssetsFromModule(void* moduleHandle,
                                       std::vector<uint64_t>& outRehydrateUuids,
                                       const char* /*addonId*/)
    {
        PurgeAssetsFromModule(moduleHandle, outRehydrateUuids);
        return true;
    }
}

// Wrappers around addon-dispatching teardown steps. The intent was to put
// these inside __try/__except to survive a crashy addon's cleanup, but
// __try/__except in this TU triggered MSVC's LTCG into an internal compiler
// error (C1001 in <memory>(2092) during the Standalone link). With LTCG
// enabled across Engine.lib and Standalone, mixing SEH with template-heavy
// translation units is a known MSVC pathology.
//
// Default policy: SKIP the addon's OnUnload entirely. Two real-world addons
// have crashed the editor on Reload inside their own OnUnload destructor
// cascades (com.polyphase.system.character.relationship,
// com.polyphase.system.adventure.cutscene — both ~10 frames deep into the
// addon DLL before the AV). OnUnload is optional in the PolyphasePluginDesc
// contract, MOD_Unload (FreeLibrary) still runs, and the OS dispatches C++
// static destructors for in-DLL globals via DllMain(DLL_PROCESS_DETACH).
// So skipping OnUnload is safe for any addon whose cleanup is "release
// in-DLL state" — which is the vast majority.
//
// Opt back in per-process with POLYPHASE_CALL_ADDON_ONUNLOAD=1 if a
// specific addon genuinely needs OnUnload to fire (e.g. it releases
// process-wide resources the OS won't clean up via DllMain — external
// file handles, sockets, shared memory).
//
// Long-term, the right fix is a per-addon flag in PolyphasePluginDesc so
// each addon opts into the risky path itself, but that's a contract change
// requiring every addon to rebuild. Skip-by-default ships safely now.
namespace
{
    static bool ShouldCallAddonOnUnload()
    {
        const char* v = std::getenv("POLYPHASE_CALL_ADDON_ONUNLOAD");
        return (v != nullptr && v[0] != '\0' && v[0] != '0');
    }

    bool SafeCallAddonOnUnload(const PolyphasePluginDesc& desc, const char* addonId)
    {
        if (desc.OnUnload == nullptr) return true;

        if (!ShouldCallAddonOnUnload())
        {
            LogDebug("Addon '%s': skipping OnUnload by default; DLL static "
                     "destructors still run via DllMain(PROCESS_DETACH). Set "
                     "POLYPHASE_CALL_ADDON_ONUNLOAD=1 to re-enable.", addonId);
            return true;
        }

        desc.OnUnload();
        return true;
    }

    bool SafeCallRemoveAllHooks(EditorUIHooks* editorUI, uint64_t hookId, const char* /*addonId*/)
    {
        if (editorUI == nullptr || editorUI->RemoveAllHooks == nullptr) return true;
        editorUI->RemoveAllHooks(hookId);
        return true;
    }

}

bool NativeAddonManager::UnloadNativeAddon(const std::string& addonId)
{
    auto it = mStates.find(addonId);
    if (it == mStates.end())
    {
        return false;
    }

    NativeAddonState& state = it->second;

    if (state.mModuleHandle == nullptr)
    {
        return true;  // Already unloaded
    }

    LogDebug("Unloading native addon: %s", addonId.c_str());

    // OnUnload + RemoveAllHooks are addon-side / addon-touching code that
    // can fault on a poorly-written addon. They go through the Safe* call
    // sites so we have a single place to bypass or re-add exception
    // protection later. OnUnload is skipped by default — set
    // POLYPHASE_CALL_ADDON_ONUNLOAD=1 to re-enable. hookId formula must
    // match LoadNativeAddon.
    if (state.mDescValid)
    {
        SafeCallAddonOnUnload(state.mDesc, addonId.c_str());
    }
    {
        uint64_t hookId = 0;
        for (char c : addonId)
        {
            hookId = hookId * 31 + c;
        }
        SafeCallRemoveAllHooks(mEngineAPI.editorUI, hookId, addonId.c_str());
    }

    // Free any Asset instances whose class came from this DLL BEFORE FreeLibrary,
    // so each destructor still dispatches into mapped code. Done before the factory
    // strip so factory state is still around if a destructor needs it. The .oct
    // file on disk is untouched; LoadAsset will rehydrate through the new factory
    // after the DLL reloads.
    //
    // Append-only into mPurgedAssetUuids: if a previous Unload purged some uuids
    // and the matching Load never happened (e.g. build failed), we want them
    // queued through the next successful Load. LoadNativeAddon clears the list
    // once it has rehydrated.
    //
    // Routed through the Safe* wrapper as a stable call site for re-adding
    // SEH-style protection later (was inlined here originally; pulled out
    // because LTCG choked on __try in this TU — see anonymous-namespace
    // comment near SafeCallAddonOnUnload).
    SafeCallPurgeAssetsFromModule(state.mModuleHandle, state.mPurgedAssetUuids,
                                  addonId.c_str());

    // Remove factory pointers owned by this DLL from the engine's factory lists BEFORE
    // FreeLibrary. Otherwise the lists hold dangling pointers and the next load of the
    // addon hits a duplicate-class-name assert in Node::RegisterFactory when the DLL's
    // static initializer re-registers the same class names.
    StripFactoriesFromModule(state.mModuleHandle);

    // Unload module
    MOD_Unload(state.mModuleHandle);

    state.mModuleHandle = nullptr;
    state.mLoadedPath.clear();
    state.mDescValid = false;
    state.mDesc = {};

    return true;
}

bool NativeAddonManager::ReloadNativeAddon(const std::string& addonId, std::string& outError)
{
    LogDebug("Reloading native addon: %s", addonId.c_str());

    // Unload first
    UnloadNativeAddon(addonId);

    // Load again (will rebuild if needed)
    return LoadNativeAddon(addonId, outError);
}

// ===== Async build queue =====
//
// Reload used to call into the build path synchronously, freezing the editor
// for the duration of every addon's compile (cl.exe + link.exe). Now we
// queue the work and run each build script on a worker thread; the main
// thread polls completion in TickAsyncBuilds() each frame and finalises
// (writes meta, MOD_Loads, registers types). An ImGui modal in EditorImgui
// renders progress while the queue is non-empty.

bool NativeAddonManager::IsBuildingAsync() const
{
    return (mActiveBuild != nullptr) || !mBuildQueue.empty();
}

int NativeAddonManager::GetAsyncBuildTotal() const
{
    return mBuildQueueTotal;
}

int NativeAddonManager::GetAsyncBuildIndex() const
{
    return mBuildQueueIndex;
}

std::string NativeAddonManager::GetAsyncBuildAddonId() const
{
    return mActiveBuild ? mActiveBuild->addonId : std::string();
}

std::string NativeAddonManager::GetAsyncBuildOutput() const
{
    if (mActiveBuild == nullptr)
        return std::string();
    std::lock_guard<std::mutex> lock(mActiveBuild->outputMutex);
    return mActiveBuild->output;
}

void NativeAddonManager::StartNextQueuedBuild()
{
    while (!mBuildQueue.empty() && mActiveBuild == nullptr)
    {
        std::string addonId = mBuildQueue.front();
        mBuildQueue.erase(mBuildQueue.begin());
        ++mBuildQueueIndex;

        auto stateIt = mStates.find(addonId);
        if (stateIt == mStates.end())
        {
            LogWarning("Async build skipped: addon '%s' not in mStates", addonId.c_str());
            continue;
        }
        NativeAddonState& state = stateIt->second;

        auto job = std::make_unique<AsyncAddonBuild>();
        job->addonId = addonId;
        job->fingerprint = ComputeFingerprint(addonId);
        if (job->fingerprint.empty())
        {
            LogWarning("Async build skipped: empty fingerprint for '%s'", addonId.c_str());
            continue;
        }

        std::string intermediateDir = GetIntermediateDir(addonId);
        std::string outputDir = intermediateDir + job->fingerprint + "/";
        job->outputPath = GetOutputPath(addonId, job->fingerprint);

        // Pre-build sweep: clear any stale intermediates and detect locked
        // files BEFORE we run the linker. If a file is locked, pause the
        // queue here — re-push the addon to the front and bail. The block
        // modal lets the user free the file and retry, which calls
        // StartNextQueuedBuild again to resume.
        {
            std::vector<std::string> stillLocked = TryClearAddonIntermediates(addonId);
            if (!stillLocked.empty())
            {
                mBlocked.mActive = true;
                mBlocked.mAddonId = addonId;
                mBlocked.mLockedFiles = std::move(stillLocked);
                mBlocked.mIntermediateDir = GetIntermediateDir(addonId);

                // Put this addon back at the front so Retry resumes it first.
                mBuildQueue.insert(mBuildQueue.begin(), addonId);
                --mBuildQueueIndex;

                LogError("Async build of '%s' paused: locked intermediate files. See popup.",
                         addonId.c_str());
                return;
            }
        }

        if (!CreateDirectoryRecursive(outputDir))
        {
            LogError("Async build skipped: failed to create %s", outputDir.c_str());
            continue;
        }

        if (!GenerateBuildScript(addonId, outputDir, job->outputPath, job->scriptPath))
        {
            LogError("Async build skipped: GenerateBuildScript failed for '%s'", addonId.c_str());
            continue;
        }

        state.mBuildInProgress = true;
        state.mBuildLog.clear();
        state.mBuildError.clear();
        state.mBuildFailureAcknowledged = false;

        LogDebug("Building native addon (async): %s [%d/%d]",
                 addonId.c_str(), mBuildQueueIndex, mBuildQueueTotal);

        AsyncAddonBuild* jobPtr = job.get();
        job->thread = std::thread([jobPtr]()
        {
#if PLATFORM_WINDOWS
            std::string cmd = "cmd /c \"" + jobPtr->scriptPath + "\"";
#else
            std::string cmd = "bash \"" + jobPtr->scriptPath + "\"";
#endif
            std::string out;
            int exit = 0;
            SYS_ExecFull(cmd.c_str(), &out, nullptr, &exit);
            {
                std::lock_guard<std::mutex> lock(jobPtr->outputMutex);
                jobPtr->output = std::move(out);
            }
            jobPtr->exitCode.store(exit);
            jobPtr->complete.store(true);
        });

        mActiveBuild = std::move(job);
    }
}

void NativeAddonManager::RetryBlockedBuild()
{
    if (!mBlocked.mActive)
    {
        return;
    }

    LogDebug("Retry: re-sweeping intermediates for blocked addon '%s'.",
             mBlocked.mAddonId.c_str());

    // Clear blocked state up front. If the sweep still finds locks, the
    // build path will set it again with a fresh list.
    mBlocked = BuildBlocked{};

    // Two paths to resume:
    //  - If we were in the middle of an async queue (mBuildQueue has the
    //    addon at the front because StartNextQueuedBuild pushed it back),
    //    drive the queue again. TickAsyncBuilds will pick it up next frame.
    //  - Otherwise the block came from the synchronous BuildNativeAddon
    //    path (LoadNativeAddon -> BuildNativeAddon, called from
    //    ReloadAllNativeAddons). Re-trigger ReloadAllNativeAddons; its
    //    skip-if-up-to-date logic means the previously-succeeded addons
    //    aren't re-touched, only the failed one is retried.
    if (!mBuildQueue.empty())
    {
        StartNextQueuedBuild();
    }
    else
    {
        ReloadAllNativeAddons();
    }
}

void NativeAddonManager::CancelBlockedBuild()
{
    if (!mBlocked.mActive)
    {
        return;
    }

    LogDebug("Cancel: abandoning blocked build for '%s'.", mBlocked.mAddonId.c_str());

    mBlocked = BuildBlocked{};

    // Drain the async queue too — the user explicitly opted out of finishing
    // this rebuild session, so don't silently start the next one.
    mBuildQueue.clear();
    mBuildQueueTotal = 0;
    mBuildQueueIndex = 0;

    // If the blocked build was part of a project-restart flow, the user just
    // bailed out of the rebuild — abort the whole restart so we don't leave
    // the editor in a half-closed state with no live addons.
    if (mRestart.mPhase != ProjectRestartPhase::None)
    {
        LogWarning("Project-restart aborted: user cancelled blocked build.");
        ProjectRestartReset();
    }
}

// ===== Build-failure surface =====
//
// Derived from per-state mBuildSucceeded / mBuildError / mBuildInProgress
// rather than a separate list, so it stays in sync with the actual state of
// each addon (a successful re-build clears the failure automatically). The
// acknowledged flag suppresses re-display after the user dismisses, until the
// next build attempt resets it.
std::vector<NativeAddonManager::BuildFailureEntry>
NativeAddonManager::GetActiveBuildFailures() const
{
    std::vector<BuildFailureEntry> out;
    for (const auto& pair : mStates)
    {
        const NativeAddonState& state = pair.second;
        if (state.mBuildInProgress) continue;
        if (state.mBuildSucceeded)  continue;
        if (state.mBuildError.empty()) continue;
        if (state.mBuildFailureAcknowledged) continue;

        BuildFailureEntry e;
        e.mAddonId = pair.first;
        e.mError   = state.mBuildError;
        e.mLog     = state.mBuildLog;
        out.push_back(std::move(e));
    }
    return out;
}

bool NativeAddonManager::HasUnacknowledgedBuildFailures() const
{
    for (const auto& pair : mStates)
    {
        const NativeAddonState& state = pair.second;
        if (state.mBuildInProgress) continue;
        if (state.mBuildSucceeded)  continue;
        if (state.mBuildError.empty()) continue;
        if (state.mBuildFailureAcknowledged) continue;
        return true;
    }
    return false;
}

void NativeAddonManager::DismissBuildFailure(const std::string& addonId)
{
    auto it = mStates.find(addonId);
    if (it == mStates.end()) return;
    it->second.mBuildFailureAcknowledged = true;
}

void NativeAddonManager::DismissAllBuildFailures()
{
    for (auto& pair : mStates)
    {
        if (!pair.second.mBuildSucceeded && !pair.second.mBuildError.empty())
        {
            pair.second.mBuildFailureAcknowledged = true;
        }
    }
}

void NativeAddonManager::RetryFailedBuild(const std::string& addonId)
{
    auto it = mStates.find(addonId);
    if (it == mStates.end())
    {
        LogWarning("RetryFailedBuild: addon '%s' not found", addonId.c_str());
        return;
    }

    // Clear failure state so the modal doesn't bounce back from the previous
    // error before the new build has a chance to set fresh state.
    NativeAddonState& state = it->second;
    state.mBuildError.clear();
    state.mBuildLog.clear();
    state.mBuildFailureAcknowledged = false;
    state.mFingerprint.clear();

    // Reuse the project-aware reload path so vtable safety + dep ordering are
    // preserved. forceRebuild=true so the user's edit gets a fresh compile.
    std::vector<std::string> ids = { addonId };
    ReloadNativeAddonsWithProjectRestart(ids, /*forceRebuild=*/true, "user retry");
}

// ===== Project-restart reload chokepoint =====

void NativeAddonManager::ReloadNativeAddonsWithProjectRestart(
    const std::vector<std::string>& addonIds,
    bool forceRebuild,
    const char* reason)
{
    if (mRestart.mPhase != ProjectRestartPhase::None)
    {
        LogWarning("ReloadNativeAddonsWithProjectRestart: another restart is already in progress (phase=%d); ignoring.",
                   (int)mRestart.mPhase);
        return;
    }

    EditorState* es = GetEditorState();
    if (es == nullptr)
    {
        LogError("ReloadNativeAddonsWithProjectRestart: no EditorState; falling back to plain ReloadAllNativeAddons.");
        ReloadAllNativeAddons();
        return;
    }

    if (IsPlayingInEditor())
    {
        LogWarning("ReloadNativeAddonsWithProjectRestart: play-in-editor is active; ignoring. Stop PIE first.");
        return;
    }

    mRestart = ProjectRestart{};
    mRestart.mTargetAddons = addonIds;
    mRestart.mForceRebuild = forceRebuild;
    mRestart.mReason       = (reason != nullptr) ? reason : "";
    mRestart.mProjectPath  = GetEngineState()->mProjectPath;

    if (mRestart.mProjectPath.empty())
    {
        LogWarning("ReloadNativeAddonsWithProjectRestart: no project loaded; nothing to do.");
        ProjectRestartReset();
        return;
    }

    // Snapshot open scenes by name. Names survive the asset-manager rediscovery
    // OpenProject performs, so we can re-resolve them after the restart. The
    // active scene is restored by reopening it last.
    const std::vector<EditScene>& editScenes = es->mEditScenes;
    mRestart.mOpenSceneNames.reserve(editScenes.size());
    for (uint32_t i = 0; i < editScenes.size(); ++i)
    {
        Scene* scn = editScenes[i].mSceneAsset.Get<Scene>();
        if (scn != nullptr && !scn->GetName().empty())
        {
            mRestart.mOpenSceneNames.push_back(scn->GetName());
        }
    }
    EditScene* active = es->GetEditScene();
    if (active != nullptr)
    {
        Scene* scn = active->mSceneAsset.Get<Scene>();
        if (scn != nullptr) mRestart.mActiveSceneName = scn->GetName();
    }

    // Build the dirty queue. Walk every open scene's backing asset and queue
    // the ones marked dirty. The user gets one Save/Discard/Cancel popup per
    // dirty scene during AwaitingDirty.
    for (const std::string& sceneName : mRestart.mOpenSceneNames)
    {
        AssetStub* stub = AssetManager::Get()->GetAssetStub(sceneName);
        if (stub != nullptr && stub->mAsset != nullptr && stub->mAsset->GetDirtyFlag())
        {
            mRestart.mDirtyScenes.push_back(sceneName);
        }
    }
    mRestart.mDirtyCursor = 0;

    mRestart.mPhase = ProjectRestartPhase::AwaitingConfirm;

    LogDebug("Project-restart staged: %u open scene(s), %u dirty, force=%d, target=%u addon(s)",
             (uint32_t)mRestart.mOpenSceneNames.size(),
             (uint32_t)mRestart.mDirtyScenes.size(),
             forceRebuild ? 1 : 0,
             (uint32_t)mRestart.mTargetAddons.size());
}

void NativeAddonManager::ProjectRestartConfirm()
{
    if (mRestart.mPhase != ProjectRestartPhase::AwaitingConfirm)
        return;

    if (!mRestart.mDirtyScenes.empty())
    {
        mRestart.mDirtyCursor = 0;
        mRestart.mPhase = ProjectRestartPhase::AwaitingDirty;
    }
    else
    {
        ProjectRestartBeginClose();
    }
}

void NativeAddonManager::ProjectRestartCancel()
{
    if (mRestart.mPhase == ProjectRestartPhase::None) return;

    LogDebug("Project-restart cancelled by user (phase=%d).", (int)mRestart.mPhase);
    ProjectRestartReset();
}

void NativeAddonManager::ProjectRestartDirtySave()
{
    if (mRestart.mPhase != ProjectRestartPhase::AwaitingDirty) return;
    if (mRestart.mDirtyCursor < 0 ||
        mRestart.mDirtyCursor >= (int32_t)mRestart.mDirtyScenes.size()) return;

    const std::string& sceneName = mRestart.mDirtyScenes[mRestart.mDirtyCursor];
    AssetStub* stub = AssetManager::Get()->GetAssetStub(sceneName);
    if (stub != nullptr && stub->mAsset != nullptr)
    {
        // For an open edit scene, capture the live world tree back into the
        // Scene asset before saving — same path Save Scene uses elsewhere.
        bool savedAsEdit = false;
        EditorState* es = GetEditorState();
        if (es != nullptr)
        {
            for (uint32_t i = 0; i < es->mEditScenes.size() && !savedAsEdit; ++i)
            {
                Scene* scn = es->mEditScenes[i].mSceneAsset.Get<Scene>();
                if (scn == stub->mAsset)
                {
                    es->CaptureAndSaveScene(stub, nullptr);
                    savedAsEdit = true;
                }
            }
        }
        if (!savedAsEdit)
        {
            AssetManager::Get()->SaveAsset(*stub);
        }
    }

    mRestart.mDirtyCursor++;
    if (mRestart.mDirtyCursor >= (int32_t)mRestart.mDirtyScenes.size())
    {
        ProjectRestartBeginClose();
    }
}

void NativeAddonManager::ProjectRestartDirtyDiscard()
{
    if (mRestart.mPhase != ProjectRestartPhase::AwaitingDirty) return;

    // Clear the dirty flag so the OpenProject path doesn't try to prompt or
    // mistakenly auto-save during teardown. The asset is about to be unloaded
    // and reloaded from disk — discarding == "throw away the in-memory edits".
    if (mRestart.mDirtyCursor >= 0 &&
        mRestart.mDirtyCursor < (int32_t)mRestart.mDirtyScenes.size())
    {
        const std::string& sceneName = mRestart.mDirtyScenes[mRestart.mDirtyCursor];
        AssetStub* stub = AssetManager::Get()->GetAssetStub(sceneName);
        if (stub != nullptr && stub->mAsset != nullptr)
        {
            stub->mAsset->ClearDirtyFlag();
        }
    }

    mRestart.mDirtyCursor++;
    if (mRestart.mDirtyCursor >= (int32_t)mRestart.mDirtyScenes.size())
    {
        ProjectRestartBeginClose();
    }
}

void NativeAddonManager::ProjectRestartDirtyCancel()
{
    if (mRestart.mPhase != ProjectRestartPhase::AwaitingDirty) return;

    LogDebug("Project-restart cancelled at dirty prompt (cursor=%d / %u).",
             mRestart.mDirtyCursor, (uint32_t)mRestart.mDirtyScenes.size());
    ProjectRestartReset();
}

void NativeAddonManager::ProjectRestartBeginClose()
{
    OCT_ASSERT(mRestart.mPhase == ProjectRestartPhase::AwaitingConfirm ||
               mRestart.mPhase == ProjectRestartPhase::AwaitingDirty);

    LogDebug("Project-restart: closing project (target=%u addon(s), force=%d)",
             (uint32_t)mRestart.mTargetAddons.size(),
             mRestart.mForceRebuild ? 1 : 0);

    // Close every edit scene before unloading anything. Live nodes whose
    // class lives in an addon DLL would otherwise hold vtable pointers into
    // pages we're about to unmap.
    EditorState* es = GetEditorState();
    if (es != nullptr)
    {
        es->CloseAllEditScenes();
    }

    // Decide which addons to actually rebuild. Empty target list means "all
    // installed enabled native addons".
    std::vector<std::string> toBuild = mRestart.mTargetAddons;
    if (toBuild.empty())
    {
        AddonManager* addonMgr = AddonManager::Get();
        const std::vector<InstalledAddon>& installed =
            addonMgr ? addonMgr->GetInstalledAddons() : std::vector<InstalledAddon>();
        const std::string& projectDir = GetEngineState()->mProjectDirectory;
        std::string packagesDir = projectDir + "Packages/";
        for (const auto& pair : mStates)
        {
            const std::string& addonId = pair.first;
            const NativeAddonState& state = pair.second;
            bool isLocal = state.mSourcePath.find(packagesDir) == 0;
            if (isLocal)
            {
                toBuild.push_back(addonId);
            }
            else
            {
                for (const InstalledAddon& inst : installed)
                {
                    if (inst.mId == addonId && inst.mEnabled && inst.mEnableNative)
                    {
                        toBuild.push_back(addonId);
                        break;
                    }
                }
            }
        }
    }

    // Unload only the addons we're going to rebuild. Non-target addons stay
    // loaded across the project restart — when OpenProject runs LoadProject
    // → ReloadAllNativeAddons, that path's filter sees them already loaded
    // and up-to-date and skips them. Limiting the scope here is important
    // because every UnloadNativeAddon dispatches into addon code (OnUnload,
    // RemoveAllHooks → hook-destructor cascades), and a bug in one addon's
    // teardown shouldn't be able to take down unrelated addons.
    //
    // Unloading also frees the DLL / .pdb file handles for the targets so
    // the rebuild can write its outputs without hitting LNK1201 on Windows.
    for (const std::string& addonId : toBuild)
    {
        auto it = mStates.find(addonId);
        if (it != mStates.end() && it->second.mModuleHandle != nullptr)
        {
            UnloadNativeAddon(addonId);
        }
    }

    // For force-rebuild requests (forceRebuild=true): wipe each target's
    // fingerprint dir so NeedsBuild() returns true and the rebuild actually
    // runs. The unload above already freed the DLL/.pdb file handles, so
    // this delete won't hit a "file in use" error.
    //
    // Same condition also flags every target to be loaded in source mode
    // for its next LoadNativeAddon() call. Without this, an addon with
    // resolveMode=binary in package.json would skip BuildNativeAddon
    // entirely and reload from the synced binary cache — which defeats the
    // entire point of Reload Native Addons for users iterating on local
    // source of a CI-published addon. The flag is one-shot (consumed by
    // LoadNativeAddon), so a normal load on the next session reverts to
    // honoring package.json's resolveMode.
    if (mRestart.mForceRebuild)
    {
        for (const std::string& addonId : toBuild)
        {
            std::string fingerprint = ComputeFingerprint(addonId);
            if (!fingerprint.empty())
            {
                std::string fingerprintDir = GetIntermediateDir(addonId) + fingerprint + "/";
                if (DoesDirExist(fingerprintDir.c_str()))
                {
                    LogDebug("  removing stale build dir: %s", fingerprintDir.c_str());
                    SYS_RemoveDirectory(fingerprintDir.c_str());
                }
            }
            auto it = mStates.find(addonId);
            if (it != mStates.end())
            {
                it->second.mFingerprint.clear();
            }
            mForceSourceForNextLoad.insert(addonId);
        }
    }

    // Enqueue rebuilds. TickAsyncBuilds drives them across frames; when the
    // queue drains, its session-done branch detects Phase::Building and calls
    // ProjectRestartOnBuildsDone for the reopen step.
    mBuildQueue = toBuild;
    mBuildQueueTotal = (int)mBuildQueue.size();
    mBuildQueueIndex = 0;

    mRestart.mPhase = ProjectRestartPhase::Building;

    if (mBuildQueue.empty())
    {
        // Nothing to actually build (e.g. user invoked Reload on an addon
        // that's now disabled). Skip straight to reopen.
        LogDebug("Project-restart: no builds queued; reopening project immediately.");
        ProjectRestartOnBuildsDone();
    }
    else
    {
        StartNextQueuedBuild();
    }
}

void NativeAddonManager::ProjectRestartOnBuildsDone()
{
    OCT_ASSERT(mRestart.mPhase == ProjectRestartPhase::Building);

    mRestart.mPhase = ProjectRestartPhase::Reopening;

    const std::string projectPath  = mRestart.mProjectPath;
    const std::vector<std::string> openScenes  = mRestart.mOpenSceneNames;
    const std::string activeScene  = mRestart.mActiveSceneName;

    LogDebug("Project-restart: builds complete; reopening project '%s' and restoring %u scene(s).",
             projectPath.c_str(), (uint32_t)openScenes.size());

    // OpenProject does the full reset + load: discards old asset state, runs
    // LoadProject (which re-runs ReloadAllNativeAddons and rediscovers
    // assets), and triggers OnProjectOpen hooks. Native factories register
    // fresh from the rebuilt DLLs so scene reload below resolves cleanly.
    ActionManager::Get()->OpenProject(projectPath.c_str());

    // Restore open scenes by name. The active scene is opened last so it ends
    // up as the editor's current scene. Best-effort: skip any that no longer
    // exist on disk.
    EditorState* es = GetEditorState();
    if (es != nullptr)
    {
        std::string activeName = activeScene;
        for (const std::string& sceneName : openScenes)
        {
            if (sceneName == activeName) continue;
            Asset* asset = AssetManager::Get()->LoadAsset(sceneName);
            Scene* scn = asset ? asset->As<Scene>() : nullptr;
            if (scn != nullptr)
            {
                es->OpenEditScene(scn);
            }
            else
            {
                LogWarning("Project-restart: could not restore scene '%s' (not found after reopen).",
                           sceneName.c_str());
            }
        }
        if (!activeName.empty())
        {
            Asset* asset = AssetManager::Get()->LoadAsset(activeName);
            Scene* scn = asset ? asset->As<Scene>() : nullptr;
            if (scn != nullptr)
            {
                es->OpenEditScene(scn);
            }
            else
            {
                LogWarning("Project-restart: could not restore active scene '%s'.",
                           activeName.c_str());
            }
        }
    }

    LogDebug("Project-restart complete.");
    ProjectRestartReset();
}

void NativeAddonManager::ProjectRestartReset()
{
    mRestart = ProjectRestart{};
}

void NativeAddonManager::TickAsyncBuilds()
{
    // No active build but queue has items: kick off the next one.
    if (mActiveBuild == nullptr)
    {
        if (!mBuildQueue.empty())
        {
            StartNextQueuedBuild();
        }
        return;
    }

    // Active build still running: leave it alone.
    if (!mActiveBuild->complete.load())
        return;

    // Active build finished — finalize on the main thread.
    if (mActiveBuild->thread.joinable())
        mActiveBuild->thread.join();

    const std::string addonId = mActiveBuild->addonId;
    const int  exitCode    = mActiveBuild->exitCode.load();
    const bool exeExists   = SYS_DoesFileExist(mActiveBuild->outputPath.c_str(), false);
    const bool buildOk     = (exitCode == 0) && exeExists;

    auto stateIt = mStates.find(addonId);
    if (stateIt != mStates.end())
    {
        NativeAddonState& state = stateIt->second;
        state.mBuildInProgress = false;
        state.mBuildSucceeded  = buildOk;
        {
            std::lock_guard<std::mutex> lock(mActiveBuild->outputMutex);
            state.mBuildLog = mActiveBuild->output;
        }

        if (buildOk)
        {
            state.mFingerprint = mActiveBuild->fingerprint;
            WriteAddonBuildMeta(mActiveBuild->outputPath, mActiveBuild->fingerprint);

            // DLL is on disk and meta is fresh — LoadNativeAddon's NeedsBuild
            // check will pass and it will proceed straight to MOD_Load and
            // type/UI registration.
            std::string err;
            if (!LoadNativeAddon(addonId, err))
            {
                LogError("Failed to load addon '%s' after async build: %s",
                         addonId.c_str(), err.c_str());
            }
        }
        else
        {
            std::string err = "Build failed (exit code " + std::to_string(exitCode);
            if (!exeExists) err += ", output not produced";
            err += ")";
            state.mBuildError = err;
            LogError("Async build failed for '%s': %s", addonId.c_str(), err.c_str());
            std::lock_guard<std::mutex> lock(mActiveBuild->outputMutex);
            if (!mActiveBuild->output.empty())
            {
                LogError("Build output:\n%s", mActiveBuild->output.c_str());
            }
        }
    }

    mActiveBuild.reset();

    if (mBuildQueue.empty())
    {
        // Session done.
        LogDebug("Async addon build session complete (%d/%d).",
                 mBuildQueueIndex, mBuildQueueTotal);
        mBuildQueueTotal = 0;
        mBuildQueueIndex = 0;

        // Project-restart flow: builds were the async wait — finish the
        // close→build→reopen sequence on the main thread now that the queue
        // is drained. Skip CallOnEditorPreInit here because OpenProject
        // re-runs the full addon-load pipeline (which calls it).
        if (mRestart.mPhase == ProjectRestartPhase::Building)
        {
            ProjectRestartOnBuildsDone();
        }
        else
        {
            CallOnEditorPreInit();
        }
    }
    else
    {
        StartNextQueuedBuild();
    }
}

void NativeAddonManager::ReloadAllNativeAddons()
{
    LogDebug("Reloading all native addons...");

    // Discover addons first
    DiscoverNativeAddons();

    // Get list of addons that should be loaded
    AddonManager* addonMgr = AddonManager::Get();
    const std::vector<InstalledAddon>& installed = addonMgr ? addonMgr->GetInstalledAddons() : std::vector<InstalledAddon>();

    // Build set of enabled native addons IN TOPO ORDER (deps before dependents).
    std::vector<std::string> enabled;

    // Local packages are always loaded
    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    std::string packagesDir = projectDir + "Packages/";

    for (const std::string& addonId : GetLoadOrder())
    {
        auto it = mStates.find(addonId);
        if (it == mStates.end()) continue;
        const NativeAddonState& state = it->second;

        // Check if this is a local package
        bool isLocal = state.mSourcePath.find(packagesDir) == 0;

        if (isLocal)
        {
            enabled.push_back(addonId);
        }
        else
        {
            // Check if installed and native enabled
            for (const InstalledAddon& inst : installed)
            {
                if (inst.mId == addonId && inst.mEnabled && inst.mEnableNative)
                {
                    enabled.push_back(addonId);
                    break;
                }
            }
        }
    }

    // Filter to only addons that actually need to be (re)loaded. An already-loaded
    // addon whose source hasn't changed (NeedsBuild() == false) is skipped — touching
    // it would unmap the DLL, invalidating vtables on every live Node-derived
    // instance from that DLL and crashing the next frame in DrawScenePanel ->
    // GetNodeIcon -> node->As<...>(). The project-restart chokepoint guards
    // against this hazard for explicit user-triggered reloads; here we avoid
    // it entirely for the no-change case so Ctrl+R / "Refresh Scripts" stays
    // safe with open edit scenes when only Lua changed.
    std::vector<std::string> toLoad;
    bool anyCurrentlyLoaded = false;
    for (const std::string& addonId : enabled)
    {
        auto it = mStates.find(addonId);
        if (it == mStates.end()) continue;
        const NativeAddonState& state = it->second;

        bool isLoaded = (state.mModuleHandle != nullptr);
        bool needsBuild = NeedsBuild(addonId);

        if (isLoaded && !needsBuild)
        {
            // Up-to-date and live; leave vtables alone.
            continue;
        }

        toLoad.push_back(addonId);
        if (isLoaded)
        {
            anyCurrentlyLoaded = true;
        }
    }

    if (toLoad.empty())
    {
        LogDebug("No native addons need reloading; skipping.");
        return;
    }

    // At least one currently-loaded addon will be unloaded. Close edit scenes
    // first so no live nodes hold vtable pointers into the DLL pages we're about
    // to unmap. Note: this path is only used by the legacy / startup load
    // flow; explicit user-triggered reloads route through the project-restart
    // chokepoint which handles closing more carefully (with confirm modal +
    // dirty-scene prompt).
    if (anyCurrentlyLoaded)
    {
        EditorState* es = GetEditorState();
        if (es != nullptr)
        {
            es->CloseAllEditScenes();
        }
    }

    // Reload each addon that needs it. For addons whose source has changed
    // (NeedsBuild()==true), unload first then let LoadNativeAddon trigger
    // BuildNativeAddon, which itself runs a thorough pre-build sweep
    // (TryClearAddonIntermediates) that surfaces any locked files via the
    // build-blocked modal — handles the LNK1201 / mspdbsrv-locked-pdb case.
    for (const std::string& addonId : toLoad)
    {
        bool needsBuild = NeedsBuild(addonId);

        if (needsBuild)
        {
            UnloadNativeAddon(addonId);

            // Force LoadNativeAddon's path lookups to recompute against the
            // current source (otherwise it could pick up a cached fingerprint).
            auto stateIt = mStates.find(addonId);
            if (stateIt != mStates.end())
            {
                stateIt->second.mFingerprint.clear();
            }

            std::string error;
            if (!LoadNativeAddon(addonId, error))
            {
                LogWarning("Failed to reload native addon %s: %s", addonId.c_str(), error.c_str());
            }
        }
        else
        {
            // Not currently loaded but should be (no source change) — straight load.
            std::string error;
            if (!ReloadNativeAddon(addonId, error))
            {
                LogWarning("Failed to reload native addon %s: %s", addonId.c_str(), error.c_str());
            }
        }
    }

    // Call OnEditorPreInit on newly loaded plugins
    CallOnEditorPreInit();

    LogDebug("Finished reloading native addons");
}

void NativeAddonManager::TickAllPlugins(float deltaTime)
{
    for (auto& pair : mStates)
    {
        NativeAddonState& state = pair.second;

        // Only tick loaded plugins with a valid Tick callback
        if (state.mModuleHandle != nullptr &&
            state.mDescValid &&
            state.mDesc.Tick != nullptr)
        {
            state.mDesc.Tick(deltaTime);
        }
    }
}

void NativeAddonManager::TickEditorAllPlugins(float deltaTime)
{
    for (auto& pair : mStates)
    {
        NativeAddonState& state = pair.second;

        // Only tick loaded plugins with a valid TickEditor callback
        if (state.mModuleHandle != nullptr &&
            state.mDescValid &&
            state.mDesc.TickEditor != nullptr)
        {
            state.mDesc.TickEditor(deltaTime);
        }
    }
}

void NativeAddonManager::CallOnEditorPreInit()
{
    for (auto& pair : mStates)
    {
        NativeAddonState& state = pair.second;

        if (state.mModuleHandle != nullptr &&
            state.mDescValid &&
            state.mDesc.OnEditorPreInit != nullptr)
        {
            state.mDesc.OnEditorPreInit();
        }
    }
}

void NativeAddonManager::CallOnEditorReady()
{
    for (auto& pair : mStates)
    {
        NativeAddonState& state = pair.second;

        if (state.mModuleHandle != nullptr &&
            state.mDescValid &&
            state.mDesc.OnEditorReady != nullptr)
        {
            state.mDesc.OnEditorReady();
        }
    }
}

const NativeAddonState* NativeAddonManager::GetState(const std::string& addonId) const
{
    auto it = mStates.find(addonId);
    return (it != mStates.end()) ? &it->second : nullptr;
}

bool NativeAddonManager::IsLoaded(const std::string& addonId) const
{
    auto it = mStates.find(addonId);
    return (it != mStates.end()) && (it->second.mModuleHandle != nullptr);
}

std::string NativeAddonManager::GetAddonSourcePath(const std::string& addonId) const
{
    auto it = mStates.find(addonId);
    return (it != mStates.end()) ? it->second.mSourcePath : "";
}

std::vector<NativeAddonState> NativeAddonManager::GetEngineAddons() const
{
    std::vector<NativeAddonState> result;

    for (const auto& pair : mStates)
    {
        if (pair.second.mNativeMetadata.mTarget == NativeAddonTarget::EngineAndEditor)
        {
            result.push_back(pair.second);
        }
    }

    return result;
}

std::vector<std::string> NativeAddonManager::GetLocalPackageIds() const
{
    std::vector<std::string> result;

    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    if (projectDir.empty())
    {
        return result;
    }

    std::string packagesDir = projectDir + "Packages/";

    for (const auto& pair : mStates)
    {
        // Check if this addon is in the local Packages/ folder
        if (pair.second.mSourcePath.find(packagesDir) == 0)
        {
            result.push_back(pair.first);
        }
    }

    return result;
}

// ===== Manifest Generation =====

bool NativeAddonManager::GenerateAddonIncludesManifest()
{
    std::string polyphasePath = SYS_GetPolyphasePath();
    std::string generatedDir = polyphasePath + "Engine/Generated/";
    std::string outputPath = generatedDir + "AddonIncludes.json";

    // Ensure Generated directory exists
    if (!DoesDirExist(generatedDir.c_str()))
    {
        SYS_CreateDirectory(generatedDir.c_str());
    }

    std::stringstream ss;
    ss << "{\n";
    ss << "    \"version\": 1,\n";
    ss << "    \"includePaths\": [\n";
    ss << "        \"Engine/Source\",\n";
    ss << "        \"Engine/Source/Engine\",\n";
    ss << "        \"Engine/Source/Editor\",\n";
    ss << "        \"Engine/Source/Plugins\",\n";
    ss << "        \"External\",\n";
    ss << "        \"External/Assimp\",\n";
    ss << "        \"External/Bullet\",\n";
    ss << "        \"External/Lua\",\n";
    ss << "        \"External/glm\",\n";
    ss << "        \"External/Imgui\",\n";
    ss << "        \"External/ImGuizmo\",\n";
    ss << "        \"External/Vorbis\"\n";
    ss << "    ],\n";
    ss << "    \"defines\": [\n";
    ss << "        \"OCTAVE_PLUGIN_EXPORT\",\n";
    ss << "        \"EDITOR=1\",\n";
    ss << "        \"LUA_ENABLED=1\",\n";
    ss << "        \"GLM_FORCE_RADIANS\",\n";
#if PLATFORM_WINDOWS
    ss << "        \"PLATFORM_WINDOWS=1\",\n";
    ss << "        \"API_VULKAN=1\",\n";
    ss << "        \"NOMINMAX\"\n";
#elif PLATFORM_LINUX
    ss << "        \"PLATFORM_LINUX=1\",\n";
    ss << "        \"API_VULKAN=1\"\n";
#elif PLATFORM_ANDROID
    ss << "        \"PLATFORM_ANDROID=1\",\n";
    ss << "        \"API_VULKAN=1\"\n";
#elif PLATFORM_3DS
    ss << "        \"PLATFORM_3DS=1\",\n";
    ss << "        \"API_C3D=1\"\n";
#endif
    ss << "    ]\n";
    ss << "}\n";

    std::string content = ss.str();
    Stream stream(content.c_str(), (uint32_t)content.size());
    bool success = stream.WriteFile(outputPath.c_str());

    if (success)
    {
        LogDebug("Generated AddonIncludes.json at %s", outputPath.c_str());
    }
    else
    {
        LogError("Failed to generate AddonIncludes.json");
    }

    return success;
}

bool NativeAddonManager::LoadAddonIncludesManifest(std::vector<std::string>& outIncludePaths,
                                                    std::vector<std::string>& outDefines)
{
    std::string polyphasePath = SYS_GetPolyphasePath();
    std::string manifestPath = polyphasePath + "Engine/Generated/AddonIncludes.json";

    // Check if manifest exists
    if (!SYS_DoesFileExist(manifestPath.c_str(), false))
    {
        return false;
    }

    // Read file
    Stream stream;
    if (!stream.ReadFile(manifestPath.c_str(), false))
    {
        return false;
    }

    // Parse JSON
    std::string jsonStr(stream.GetData(), stream.GetSize());
    rapidjson::Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError())
    {
        LogWarning("Failed to parse AddonIncludes.json");
        return false;
    }

    // Read include paths
    if (doc.HasMember("includePaths") && doc["includePaths"].IsArray())
    {
        const rapidjson::Value& paths = doc["includePaths"];
        for (rapidjson::SizeType i = 0; i < paths.Size(); i++)
        {
            if (paths[i].IsString())
            {
                outIncludePaths.push_back(paths[i].GetString());
            }
        }
    }

    // Read defines
    if (doc.HasMember("defines") && doc["defines"].IsArray())
    {
        const rapidjson::Value& defines = doc["defines"];
        for (rapidjson::SizeType i = 0; i < defines.Size(); i++)
        {
            if (defines[i].IsString())
            {
                outDefines.push_back(defines[i].GetString());
            }
        }
    }

    return true;
}

// ===== Creation and Packaging Implementation =====

std::string NativeAddonManager::GenerateIdFromName(const std::string& name)
{
    std::string id;
    id.reserve(name.size());

    bool lastWasHyphen = false;
    for (char c : name)
    {
        if (std::isalnum(c))
        {
            id += static_cast<char>(std::tolower(c));
            lastWasHyphen = false;
        }
        else if (c == '.')
        {
            // Preserve dots so reverse-domain ids (e.g. "com.formats.video") survive
            // through the addon folder name and binary name.
            id += '.';
            lastWasHyphen = false;
        }
        else if (c == ' ' || c == '_' || c == '-')
        {
            if (!lastWasHyphen && !id.empty())
            {
                id += '-';
                lastWasHyphen = true;
            }
        }
    }

    // Remove trailing hyphen
    while (!id.empty() && id.back() == '-')
    {
        id.pop_back();
    }

    return id;
}

bool NativeAddonManager::WriteTemplateSourceFile(const std::string& path,
                                                  const std::string& addonName,
                                                  const std::string& binaryName)
{
    // Generate a clean C++ identifier from addon name
    std::string className;
    bool capitalizeNext = true;
    for (char c : addonName)
    {
        if (std::isalnum(c))
        {
            if (capitalizeNext)
            {
                className += static_cast<char>(std::toupper(c));
                capitalizeNext = false;
            }
            else
            {
                className += c;
            }
        }
        else
        {
            capitalizeNext = true;
        }
    }

    if (className.empty())
    {
        className = "MyAddon";
    }

    std::stringstream ss;
    ss << "/**\n";
    ss << " * @file " << className << ".cpp\n";
    ss << " * @brief Native addon: " << addonName << "\n";
    ss << " */\n";
    ss << "\n";
    ss << "#include \"Plugins/PolyphasePluginAPI.h\"\n";
    ss << "#include \"Plugins/PolyphaseEngineAPI.h\"\n";
    ss << "\n";
    ss << "static PolyphaseEngineAPI* sEngineAPI = nullptr;\n";
    ss << "\n";
    ss << "static int OnLoad(PolyphaseEngineAPI* api)\n";
    ss << "{\n";
    ss << "    sEngineAPI = api;\n";
    ss << "    api->LogDebug(\"" << addonName << " loaded!\");\n";
    ss << "    return 0;\n";
    ss << "}\n";
    ss << "\n";
    ss << "static void OnUnload()\n";
    ss << "{\n";
    ss << "    if (sEngineAPI)\n";
    ss << "    {\n";
    ss << "        sEngineAPI->LogDebug(\"" << addonName << " unloaded.\");\n";
    ss << "    }\n";
    ss << "    sEngineAPI = nullptr;\n";
    ss << "}\n";
    ss << "\n";
    ss << "static void RegisterTypes(void* nodeFactory)\n";
    ss << "{\n";
    ss << "    // Register custom node types here\n";
    ss << "    // Example: REGISTER_NODE(MyCustomNode);\n";
    ss << "}\n";
    ss << "\n";
    ss << "static void RegisterScriptFuncs(lua_State* L)\n";
    ss << "{\n";
    ss << "    // Register Lua functions here\n";
    ss << "    // Use L to interact with Lua state\n";
    ss << "    (void)L; // Suppress unused parameter warning\n";
    ss << "}\n";
    ss << "\n";
    ss << "#if EDITOR\n";
    ss << "static void RegisterEditorUI(EditorUIHooks* hooks, uint64_t hookId)\n";
    ss << "{\n";
    ss << "    // Register editor UI extensions here\n";
    ss << "    // Example:\n";
    ss << "    // hooks->AddMenuItem(hookId, \"Developer\", \"" << addonName << " Tool\",\n";
    ss << "    //     [](void*) { /* do something */ }, nullptr, nullptr);\n";
    ss << "}\n";
    ss << "#endif\n";
    ss << "\n";
    ss << "extern \"C\" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* desc)\n";
    ss << "{\n";
    ss << "    desc->apiVersion = OCTAVE_PLUGIN_API_VERSION;\n";
    ss << "    desc->pluginName = \"" << addonName << "\";\n";
    ss << "    desc->pluginVersion = \"1.0.0\";\n";
    ss << "    desc->OnLoad = OnLoad;\n";
    ss << "    desc->OnUnload = OnUnload;\n";
    ss << "    desc->RegisterTypes = RegisterTypes;\n";
    ss << "    desc->RegisterScriptFuncs = RegisterScriptFuncs;\n";
    ss << "#if EDITOR\n";
    ss << "    desc->RegisterEditorUI = RegisterEditorUI;\n";
    ss << "#else\n";
    ss << "    desc->RegisterEditorUI = nullptr;\n";
    ss << "#endif\n";
    ss << "    desc->OnEditorPreInit = nullptr;\n";
    ss << "    desc->OnEditorReady = nullptr;\n";
    ss << "    return 0;\n";
    ss << "}\n";

    std::string content = ss.str();
    Stream stream(content.c_str(), (uint32_t)content.size());
    return stream.WriteFile(path.c_str());
}

bool NativeAddonManager::WritePackageJson(const std::string& path, const NativeAddonCreateInfo& info)
{
    std::string targetStr = (info.mTarget == NativeAddonTarget::EditorOnly) ? "editor" : "engine";

    std::stringstream ss;
    ss << "{\n";
    ss << "    \"name\": \"" << info.mName << "\",\n";
    ss << "    \"author\": \"" << info.mAuthor << "\",\n";
    ss << "    \"description\": \"" << info.mDescription << "\",\n";
    ss << "    \"version\": \"" << info.mVersion << "\",\n";
    ss << "    \"native\": {\n";
    ss << "        \"target\": \"" << targetStr << "\",\n";
    ss << "        \"sourceDir\": \"Source\",\n";
    ss << "        \"binaryName\": \"" << info.mBinaryName << "\",\n";
    ss << "        \"entrySymbol\": \"PolyphasePlugin_GetDesc\",\n";
    ss << "        \"apiVersion\": 3,\n";
    ss << "        \"resolveMode\": \"source\",\n";
    ss << "        \"binaries\": []\n";
    ss << "    }\n";
    ss << "}\n";

    std::string content = ss.str();
    Stream stream(content.c_str(), (uint32_t)content.size());
    return stream.WriteFile(path.c_str());
}

static bool WriteBuildBat(const std::string& addonPath, const std::string& binaryName)
{
    std::stringstream ss;
    ss << "@echo off\n";
    ss << "REM Native Addon Build Script for Windows\n";
    ss << "REM Run this from the root of your addon folder (where package.json is)\n";
    ss << "REM\n";
    ss << "REM Usage: build.bat [config]\n";
    ss << "REM   config - Optional. \"Debug\", \"Release\", or \"Both\" (default: Both)\n";
    ss << "REM\n";
    ss << "REM Requirements:\n";
    ss << "REM   - Visual Studio with C++ tools installed\n";
    ss << "REM   - Run from a \"Developer Command Prompt\" or ensure cl.exe is in PATH\n";
    ss << "\n";
    ss << "setlocal enabledelayedexpansion\n";
    ss << "\n";
    ss << "set \"ADDON_NAME=" << binaryName << "\"\n";
    ss << "set \"BUILD_CONFIG=%~1\"\n";
    ss << "if \"%BUILD_CONFIG%\"==\"\" set \"BUILD_CONFIG=Both\"\n";
    ss << "\n";
    ss << "echo.\n";
    ss << "echo ========================================\n";
    ss << "echo  Building Native Addon: %ADDON_NAME%\n";
    ss << "echo  Configuration: %BUILD_CONFIG%\n";
    ss << "echo ========================================\n";
    ss << "echo.\n";
    ss << "\n";
    ss << "if not exist \"Source\" (\n";
    ss << "    echo ERROR: Source directory not found!\n";
    ss << "    exit /b 1\n";
    ss << ")\n";
    ss << "\n";
    ss << "where cl.exe >nul 2>&1\n";
    ss << "if errorlevel 1 (\n";
    ss << "    echo ERROR: cl.exe not found!\n";
    ss << "    echo Please run this from a Visual Studio Developer Command Prompt.\n";
    ss << "    exit /b 1\n";
    ss << ")\n";
    ss << "\n";
    ss << "set \"SOURCES=\"\n";
    ss << "for /r \"Source\" %%%%f in (*.cpp) do (\n";
    ss << "    set \"SOURCES=!SOURCES! \"%%%%f\"\"\n";
    ss << ")\n";
    ss << "\n";
    ss << "set \"BUILD_FAILED=0\"\n";
    ss << "\n";
    ss << "if /i \"%BUILD_CONFIG%\"==\"Debug\" goto :BuildDebug\n";
    ss << "if /i \"%BUILD_CONFIG%\"==\"Both\" goto :BuildRelease\n";
    ss << "if /i \"%BUILD_CONFIG%\"==\"Release\" goto :BuildRelease\n";
    ss << "goto :Summary\n";
    ss << "\n";
    ss << ":BuildRelease\n";
    ss << "echo Building Release configuration...\n";
    ss << "if not exist \"build\\Windows\\x64\\Release\" mkdir \"build\\Windows\\x64\\Release\"\n";
    ss << "pushd build\\Windows\\x64\\Release\n";
    ss << "cl /nologo /EHsc /O2 /MD /LD ^\n";
    ss << "    /I\"..\\..\\..\\..\\Source\" ^\n";
    ss << "    /Fe:\"%ADDON_NAME%.dll\" /Fo:\"%ADDON_NAME%_\" ^\n";
    ss << "    /D \"OCTAVE_PLUGIN_EXPORT\" /D \"NDEBUG\" /D \"PLATFORM_WINDOWS=1\" ^\n";
    ss << "    !SOURCES! /link /DLL /MACHINE:X64\n";
    ss << "if errorlevel 1 ( popd & echo Release build FAILED! & set \"BUILD_FAILED=1\" ) else ( popd & echo Release build succeeded )\n";
    ss << "certutil -hashfile \"build\\Windows\\x64\\Release\\%ADDON_NAME%.dll\" SHA256 > \"build\\Windows\\x64\\Release\\%ADDON_NAME%-Windows-x64-Release.sha256\" 2>nul\n";
    ss << "\n";
    ss << "if /i \"%BUILD_CONFIG%\"==\"Release\" goto :Summary\n";
    ss << "\n";
    ss << ":BuildDebug\n";
    ss << "echo Building Debug configuration...\n";
    ss << "if not exist \"build\\Windows\\x64\\Debug\" mkdir \"build\\Windows\\x64\\Debug\"\n";
    ss << "pushd build\\Windows\\x64\\Debug\n";
    ss << "cl /nologo /EHsc /Od /MDd /LD /Zi ^\n";
    ss << "    /I\"..\\..\\..\\..\\Source\" ^\n";
    ss << "    /Fe:\"%ADDON_NAME%.dll\" /Fo:\"%ADDON_NAME%_\" /Fd:\"%ADDON_NAME%.pdb\" ^\n";
    ss << "    /D \"OCTAVE_PLUGIN_EXPORT\" /D \"_DEBUG\" /D \"PLATFORM_WINDOWS=1\" ^\n";
    ss << "    !SOURCES! /link /DLL /MACHINE:X64 /DEBUG\n";
    ss << "if errorlevel 1 ( popd & echo Debug build FAILED! & set \"BUILD_FAILED=1\" ) else ( popd & echo Debug build succeeded )\n";
    ss << "certutil -hashfile \"build\\Windows\\x64\\Debug\\%ADDON_NAME%.dll\" SHA256 > \"build\\Windows\\x64\\Debug\\%ADDON_NAME%-Windows-x64-Debug.sha256\" 2>nul\n";
    ss << "\n";
    ss << ":Summary\n";
    ss << "echo.\n";
    ss << "if \"%BUILD_FAILED%\"==\"1\" ( echo BUILD COMPLETED WITH ERRORS ) else ( echo Build Succeeded! )\n";
    ss << "echo Output: build\\Windows\\x64\\[Debug|Release]\\%ADDON_NAME%.dll\n";
    ss << "\n";
    ss << "if \"%BUILD_FAILED%\"==\"1\" exit /b 1\n";
    ss << "endlocal\n";

    std::string batPath = addonPath + "build.bat";
    std::string content = ss.str();
    Stream stream(content.c_str(), (uint32_t)content.size());
    return stream.WriteFile(batPath.c_str());
}

static bool WriteBuildSh(const std::string& addonPath, const std::string& binaryName)
{
    std::stringstream ss;
    ss << "#!/bin/bash\n";
    ss << "# Native Addon Build Script for Linux/macOS\n";
    ss << "# Run this from the root of your addon folder (where package.json is)\n";
    ss << "#\n";
    ss << "# Usage: ./build.sh [config]\n";
    ss << "#   config - Optional. \"Debug\", \"Release\", or \"Both\" (default: Both)\n";
    ss << "#\n";
    ss << "# Requirements:\n";
    ss << "#   - g++ or clang++ installed\n";
    ss << "\n";
    ss << "ADDON_NAME=\"" << binaryName << "\"\n";
    ss << "BUILD_CONFIG=\"${1:-Both}\"\n";
    ss << "\n";
    ss << "echo \"\"\n";
    ss << "echo \"========================================\"\n";
    ss << "echo \" Building Native Addon: $ADDON_NAME\"\n";
    ss << "echo \" Configuration: $BUILD_CONFIG\"\n";
    ss << "echo \"========================================\"\n";
    ss << "echo \"\"\n";
    ss << "\n";
    ss << "if [ ! -d \"Source\" ]; then\n";
    ss << "    echo \"ERROR: Source directory not found!\"\n";
    ss << "    exit 1\n";
    ss << "fi\n";
    ss << "\n";
    ss << "if command -v g++ &> /dev/null; then\n";
    ss << "    CXX=\"g++\"\n";
    ss << "elif command -v clang++ &> /dev/null; then\n";
    ss << "    CXX=\"clang++\"\n";
    ss << "else\n";
    ss << "    echo \"ERROR: No C++ compiler found!\"\n";
    ss << "    exit 1\n";
    ss << "fi\n";
    ss << "\n";
    ss << "SOURCES=$(find Source -name \"*.cpp\" -type f)\n";
    ss << "BUILD_FAILED=0\n";
    ss << "\n";
    ss << "if [[ \"$BUILD_CONFIG\" == \"Release\" ]] || [[ \"$BUILD_CONFIG\" == \"Both\" ]]; then\n";
    ss << "    echo \"Building Release configuration...\"\n";
    ss << "    mkdir -p \"build/Linux/x64/Release\"\n";
    ss << "    if $CXX -shared -fPIC -O2 -std=c++17 -ISource \\\n";
    ss << "        -DOCTAVE_PLUGIN_EXPORT -DNDEBUG -DPLATFORM_LINUX=1 \\\n";
    ss << "        -o \"build/Linux/x64/Release/lib${ADDON_NAME}.so\" $SOURCES; then\n";
    ss << "        echo \"Release build succeeded\"\n";
    ss << "        command -v sha256sum &> /dev/null && sha256sum \"build/Linux/x64/Release/lib${ADDON_NAME}.so\" > \"build/Linux/x64/Release/${ADDON_NAME}-Linux-x64-Release.sha256\"\n";
    ss << "    else\n";
    ss << "        echo \"Release build FAILED!\"\n";
    ss << "        BUILD_FAILED=1\n";
    ss << "    fi\n";
    ss << "fi\n";
    ss << "\n";
    ss << "if [[ \"$BUILD_CONFIG\" == \"Debug\" ]] || [[ \"$BUILD_CONFIG\" == \"Both\" ]]; then\n";
    ss << "    echo \"Building Debug configuration...\"\n";
    ss << "    mkdir -p \"build/Linux/x64/Debug\"\n";
    ss << "    if $CXX -shared -fPIC -O0 -g -std=c++17 -ISource \\\n";
    ss << "        -DOCTAVE_PLUGIN_EXPORT -D_DEBUG -DPLATFORM_LINUX=1 \\\n";
    ss << "        -o \"build/Linux/x64/Debug/lib${ADDON_NAME}.so\" $SOURCES; then\n";
    ss << "        echo \"Debug build succeeded\"\n";
    ss << "        command -v sha256sum &> /dev/null && sha256sum \"build/Linux/x64/Debug/lib${ADDON_NAME}.so\" > \"build/Linux/x64/Debug/${ADDON_NAME}-Linux-x64-Debug.sha256\"\n";
    ss << "    else\n";
    ss << "        echo \"Debug build FAILED!\"\n";
    ss << "        BUILD_FAILED=1\n";
    ss << "    fi\n";
    ss << "fi\n";
    ss << "\n";
    ss << "echo \"\"\n";
    ss << "if [ $BUILD_FAILED -eq 1 ]; then\n";
    ss << "    echo \"BUILD COMPLETED WITH ERRORS\"\n";
    ss << "else\n";
    ss << "    echo \"Build Succeeded!\"\n";
    ss << "fi\n";
    ss << "echo \"Output: build/Linux/x64/[Debug|Release]/lib${ADDON_NAME}.so\"\n";
    ss << "\n";
    ss << "exit $BUILD_FAILED\n";

    std::string shPath = addonPath + "build.sh";
    std::string content = ss.str();
    Stream stream(content.c_str(), (uint32_t)content.size());
    return stream.WriteFile(shPath.c_str());
}

static bool WriteGitHubWorkflowTemplate(const std::string& addonPath, const std::string& binaryName)
{
    std::string workflowDir = addonPath + ".github/workflows/";

    // Create directories recursively
    std::string githubDir = addonPath + ".github/";
    if (!DoesDirExist(githubDir.c_str()))
    {
        SYS_CreateDirectory(githubDir.c_str());
    }
    if (!DoesDirExist(workflowDir.c_str()))
    {
        SYS_CreateDirectory(workflowDir.c_str());
    }

    std::stringstream ss;
    ss << "# Native Addon Release Workflow\n";
    ss << "# Triggers on tags matching v*\n";
    ss << "# Builds Debug and Release for Windows and Linux, publishes release assets\n";
    ss << "#\n";
    ss << "# Linux: polyphaselabs/polyphase-engine Docker image (engine + toolchain pre-baked).\n";
    ss << "# Windows: Vulkan SDK + MSBuild + cloned engine (mirrors engine's release.yml).\n";
    ss << "#\n";
    ss << "# Usage: Tag a release: git tag v1.0.0 && git push origin v1.0.0\n";
    ss << "#\n";
    ss << "# Local testing:\n";
    ss << "#   Windows: set POLYPHASE_PATH=C:\\path\\to\\engine && .github\\workflows\\build.bat\n";
    ss << "#   Linux:   POLYPHASE_PATH=/path/to/engine ./.github/workflows/build.sh\n";
    ss << "\n";
    ss << "name: Native Addon Release\n";
    ss << "\n";
    ss << "on:\n";
    ss << "  push:\n";
    ss << "    tags:\n";
    ss << "      - 'v*'\n";
    ss << "  workflow_dispatch:\n";
    ss << "\n";
    ss << "permissions:\n";
    ss << "  contents: write\n";
    ss << "\n";
    ss << "env:\n";
    ss << "  POLYPHASE_REPO: \"Polyphase-Labs/Polyphase-Engine\"\n";
    ss << "  POLYPHASE_ENGINE_IMAGE: \"polyphaselabs/polyphase-engine:latest\"\n";
    ss << "\n";
    ss << "jobs:\n";
    ss << "  build-linux:\n";
    ss << "    runs-on: ubuntu-latest\n";
    ss << "    container:\n";
    ss << "      image: polyphaselabs/polyphase-engine:latest\n";
    ss << "    defaults:\n";
    ss << "      run:\n";
    ss << "        shell: bash\n";
    ss << "    steps:\n";
    ss << "      - uses: actions/checkout@v4\n";
    ss << "\n";
    ss << "      - name: Mark workspace safe for git\n";
    ss << "        run: git config --global --add safe.directory '*'\n";
    ss << "\n";
    ss << "      - name: Read addon name from package.json\n";
    ss << "        run: |\n";
    ss << "          ADDON_NAME=$(cat package.json | python3 -c \"import sys,json; d=json.load(sys.stdin); print(d.get('native',{}).get('binaryName') or d.get('name'))\")\n";
    ss << "          echo \"ADDON_NAME=$ADDON_NAME\" >> $GITHUB_ENV\n";
    ss << "\n";
    ss << "      - name: Build Both Configs (Linux)\n";
    ss << "        run: |\n";
    ss << "          chmod +x .github/workflows/build.sh\n";
    ss << "          export POLYPHASE_PATH=\"/polyphase\"\n";
    ss << "          ./.github/workflows/build.sh \"$ADDON_NAME\" Both\n";
    ss << "\n";
    ss << "      - name: Upload Linux Release artifact\n";
    ss << "        uses: actions/upload-artifact@v4\n";
    ss << "        with:\n";
    ss << "          name: ${{ env.ADDON_NAME }}-Linux-x64-Release\n";
    ss << "          path: |\n";
    ss << "            build/Linux/x64/Release/lib${{ env.ADDON_NAME }}.so\n";
    ss << "            build/Linux/x64/Release/*.sha256\n";
    ss << "\n";
    ss << "      - name: Upload Linux Debug artifact\n";
    ss << "        uses: actions/upload-artifact@v4\n";
    ss << "        with:\n";
    ss << "          name: ${{ env.ADDON_NAME }}-Linux-x64-Debug\n";
    ss << "          path: |\n";
    ss << "            build/Linux/x64/Debug/lib${{ env.ADDON_NAME }}.so\n";
    ss << "            build/Linux/x64/Debug/*.sha256\n";
    ss << "\n";
    ss << "  build-windows:\n";
    ss << "    runs-on: windows-latest\n";
    ss << "    steps:\n";
    ss << "      - uses: actions/checkout@v4\n";
    ss << "\n";
    ss << "      - name: Read addon name from package.json\n";
    ss << "        shell: bash\n";
    ss << "        run: |\n";
    ss << "          ADDON_NAME=$(cat package.json | python3 -c \"import sys,json; d=json.load(sys.stdin); print(d.get('native',{}).get('binaryName') or d.get('name'))\")\n";
    ss << "          echo \"ADDON_NAME=$ADDON_NAME\" >> $GITHUB_ENV\n";
    ss << "\n";
    ss << "      - name: Clone Polyphase Engine\n";
    ss << "        shell: bash\n";
    ss << "        run: git clone --depth 1 https://github.com/${{ env.POLYPHASE_REPO }}.git ../polyphase-engine\n";
    ss << "\n";
    ss << "      - name: Initialize engine submodules\n";
    ss << "        shell: bash\n";
    ss << "        working-directory: ../polyphase-engine\n";
    ss << "        run: |\n";
    ss << "          git submodule init -- External/bullet3 External/doxygen-awesome-css Engine/External/PolyVox || true\n";
    ss << "          git submodule update --recursive || true\n";
    ss << "\n";
    ss << "      - name: Install Vulkan SDK\n";
    ss << "        shell: pwsh\n";
    ss << "        run: |\n";
    ss << "          $VER = \"1.3.275.0\"\n";
    ss << "          Invoke-WebRequest -Uri \"https://sdk.lunarg.com/sdk/download/$VER/windows/VulkanSDK-$VER-Installer.exe\" -OutFile \"$env:TEMP\\VulkanSDK.exe\"\n";
    ss << "          Start-Process -FilePath \"$env:TEMP\\VulkanSDK.exe\" -ArgumentList \"--accept-licenses --default-answer --confirm-command install\" -Wait\n";
    ss << "          echo \"VULKAN_SDK=C:\\VulkanSDK\\$VER\" >> $env:GITHUB_ENV\n";
    ss << "\n";
    ss << "      - name: Setup MSVC\n";
    ss << "        uses: ilammy/msvc-dev-cmd@v1\n";
    ss << "        with:\n";
    ss << "          arch: x64\n";
    ss << "\n";
    ss << "      - name: Build Both Configs (Windows)\n";
    ss << "        shell: cmd\n";
    ss << "        run: |\n";
    ss << "          set \"POLYPHASE_PATH=..\\polyphase-engine\"\n";
    ss << "          call .github\\workflows\\build.bat %ADDON_NAME% Both\n";
    ss << "\n";
    ss << "      - name: Upload Windows Release artifact\n";
    ss << "        uses: actions/upload-artifact@v4\n";
    ss << "        with:\n";
    ss << "          name: ${{ env.ADDON_NAME }}-Windows-x64-Release\n";
    ss << "          path: |\n";
    ss << "            build/Windows/x64/Release/${{ env.ADDON_NAME }}.dll\n";
    ss << "            build/Windows/x64/Release/*.sha256\n";
    ss << "\n";
    ss << "      - name: Upload Windows Debug artifact\n";
    ss << "        uses: actions/upload-artifact@v4\n";
    ss << "        with:\n";
    ss << "          name: ${{ env.ADDON_NAME }}-Windows-x64-Debug\n";
    ss << "          path: |\n";
    ss << "            build/Windows/x64/Debug/${{ env.ADDON_NAME }}.dll\n";
    ss << "            build/Windows/x64/Debug/*.sha256\n";
    ss << "            build/Windows/x64/Debug/*.pdb\n";
    ss << "\n";
    ss << "  release:\n";
    ss << "    needs: [build-linux, build-windows]\n";
    ss << "    runs-on: ubuntu-latest\n";
    ss << "    permissions:\n";
    ss << "      contents: write\n";
    ss << "\n";
    ss << "    steps:\n";
    ss << "      - uses: actions/checkout@v4\n";
    ss << "\n";
    ss << "      - name: Read addon name from package.json\n";
    ss << "        run: |\n";
    ss << "          ADDON_NAME=$(cat package.json | python3 -c \"import sys,json; d=json.load(sys.stdin); print(d.get('native',{}).get('binaryName') or d.get('name'))\")\n";
    ss << "          echo \"ADDON_NAME=$ADDON_NAME\" >> $GITHUB_ENV\n";
    ss << "\n";
    ss << "      - name: Download all artifacts\n";
    ss << "        uses: actions/download-artifact@v4\n";
    ss << "        with:\n";
    ss << "          path: artifacts\n";
    ss << "\n";
    ss << "      - name: Update package.json with binary descriptors\n";
    ss << "        run: |\n";
    ss << "          sudo apt-get install -y jq\n";
    ss << "          BINARIES=\"[]\"\n";
    ss << "          [ -f \"artifacts/${{ env.ADDON_NAME }}-Windows-x64-Release/${{ env.ADDON_NAME }}.dll\" ] && \\\n";
    ss << "            BINARIES=$(echo \"$BINARIES\" | jq '. += [{\"platform\": \"Windows\", \"arch\": \"x64\", \"config\": \"Release\", \"type\": \"releaseAsset\", \"value\": \"${{ env.ADDON_NAME }}-Windows-x64-Release.dll\"}]')\n";
    ss << "          [ -f \"artifacts/${{ env.ADDON_NAME }}-Windows-x64-Debug/${{ env.ADDON_NAME }}.dll\" ] && \\\n";
    ss << "            BINARIES=$(echo \"$BINARIES\" | jq '. += [{\"platform\": \"Windows\", \"arch\": \"x64\", \"config\": \"Debug\", \"type\": \"releaseAsset\", \"value\": \"${{ env.ADDON_NAME }}-Windows-x64-Debug.dll\"}]')\n";
    ss << "          [ -f \"artifacts/${{ env.ADDON_NAME }}-Linux-x64-Release/lib${{ env.ADDON_NAME }}.so\" ] && \\\n";
    ss << "            BINARIES=$(echo \"$BINARIES\" | jq '. += [{\"platform\": \"Linux\", \"arch\": \"x64\", \"config\": \"Release\", \"type\": \"releaseAsset\", \"value\": \"lib${{ env.ADDON_NAME }}-Linux-x64-Release.so\"}]')\n";
    ss << "          [ -f \"artifacts/${{ env.ADDON_NAME }}-Linux-x64-Debug/lib${{ env.ADDON_NAME }}.so\" ] && \\\n";
    ss << "            BINARIES=$(echo \"$BINARIES\" | jq '. += [{\"platform\": \"Linux\", \"arch\": \"x64\", \"config\": \"Debug\", \"type\": \"releaseAsset\", \"value\": \"lib${{ env.ADDON_NAME }}-Linux-x64-Debug.so\"}]')\n";
    ss << "          jq --argjson binaries \"$BINARIES\" '.binaries = $binaries | .resolveMode = \"binary\"' package.json > package.json.updated\n";
    ss << "          mv package.json.updated package.json\n";
    ss << "\n";
    ss << "      - name: Prepare release files\n";
    ss << "        run: |\n";
    ss << "          mkdir -p release\n";
    ss << "          for dir in artifacts/*/; do\n";
    ss << "            artifact_name=$(basename \"$dir\")\n";
    ss << "            config=\"${artifact_name##*-}\"\n";
    ss << "            platform_arch=\"${artifact_name%-*}\"\n";
    ss << "            platform_arch=\"${platform_arch#*-}\"\n";
    ss << "            for file in \"$dir\"*; do\n";
    ss << "              if [ -f \"$file\" ]; then\n";
    ss << "                filename=$(basename \"$file\")\n";
    ss << "                if [[ \"$filename\" == *.dll ]]; then\n";
    ss << "                  base=\"${filename%.dll}\"\n";
    ss << "                  cp \"$file\" \"release/${base}-${platform_arch}-${config}.dll\"\n";
    ss << "                elif [[ \"$filename\" == *.so ]]; then\n";
    ss << "                  base=\"${filename%.so}\"\n";
    ss << "                  cp \"$file\" \"release/${base}-${platform_arch}-${config}.so\"\n";
    ss << "                elif [[ \"$filename\" == *.pdb ]]; then\n";
    ss << "                  base=\"${filename%.pdb}\"\n";
    ss << "                  cp \"$file\" \"release/${base}-${platform_arch}-${config}.pdb\"\n";
    ss << "                else\n";
    ss << "                  cp \"$file\" \"release/\"\n";
    ss << "                fi\n";
    ss << "              fi\n";
    ss << "            done\n";
    ss << "          done\n";
    ss << "          cp package.json \"release/package.json\"\n";
    ss << "          ls -la release/\n";
    ss << "\n";
    ss << "      - name: Create Release\n";
    ss << "        uses: softprops/action-gh-release@v1\n";
    ss << "        with:\n";
    ss << "          files: release/*\n";
    ss << "          generate_release_notes: true\n";

    std::string workflowPath = workflowDir + "native-addon-release.yml";
    std::string content = ss.str();
    Stream stream(content.c_str(), (uint32_t)content.size());
    bool success = stream.WriteFile(workflowPath.c_str());

    // Also write the local build scripts
    WriteBuildBat(addonPath, binaryName);
    WriteBuildSh(addonPath, binaryName);

    return success;
}

bool NativeAddonManager::WriteVSCodeConfig(const std::string& addonPath)
{
    std::string vscodeDir = addonPath + ".vscode/";
    if (!DoesDirExist(vscodeDir.c_str()))
    {
        SYS_CreateDirectory(vscodeDir.c_str());
    }

    std::string polyphasePath = SYS_GetPolyphasePath();

    // Helper lambda to normalize paths for JSON (use forward slashes)
    auto normalizePath = [](const std::string& path) -> std::string {
        std::string result;
        for (char c : path)
        {
            if (c == '\\')
                result += '/';
            else
                result += c;
        }
        return result;
    };

    std::string polyphasePathJson = normalizePath(polyphasePath);

    // Try to load from manifest, fall back to hardcoded paths
    std::vector<std::string> includePaths;
    std::vector<std::string> defines;

    if (!LoadAddonIncludesManifest(includePaths, defines))
    {
        // Fallback to hardcoded paths
        includePaths = {
            "Engine/Source",
            "Engine/Source/Engine",
            "Engine/Source/Plugins",
            "External",
            "External/Assimp",
            "External/Bullet",
            "External/Lua",
            "External/glm",
            "External/Imgui",
            "External/ImGuizmo",
            "External/Vorbis"
        };
        defines = {
            "OCTAVE_PLUGIN_EXPORT",
            "EDITOR=1",
            "LUA_ENABLED=1",
            "GLM_FORCE_RADIANS",
#if PLATFORM_WINDOWS
            "PLATFORM_WINDOWS=1",
            "API_VULKAN=1",
            "NOMINMAX"
#elif PLATFORM_LINUX
            "PLATFORM_LINUX=1",
            "API_VULKAN=1"
#elif PLATFORM_ANDROID
            "PLATFORM_ANDROID=1",
            "API_VULKAN=1"
#elif PLATFORM_3DS
            "PLATFORM_3DS=1",
            "API_C3D=1"
#endif
        };
    }

    // Parse package.json for dependencies
    std::string packageJsonPath = addonPath + "package.json";
    NativeModuleMetadata metadata;
    ContentMetadata pkgContent;
    ParsePackageJson(packageJsonPath, metadata, &pkgContent);

    // Get parent Packages directory and addon ID for resolving sibling addon dependencies
    std::string packagesDir;
    std::string addonId;
    {
        std::string path = addonPath;
        while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        {
            path.pop_back();
        }
        size_t lastSlash = path.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            packagesDir = path.substr(0, lastSlash + 1);
            addonId = path.substr(lastSlash + 1);
        }
    }
    std::string packagesDirJson = normalizePath(packagesDir);

    std::stringstream ss;
    ss << "{\n";
    ss << "    \"configurations\": [\n";
    ss << "        {\n";
    ss << "            \"name\": \"Polyphase Addon\",\n";
    ss << "            \"includePath\": [\n";
    ss << "                \"${workspaceFolder}/**\"";

    // Add include paths from manifest
    for (const std::string& path : includePaths)
    {
        ss << ",\n                \"" << polyphasePathJson << path << "\"";
    }
    // Add dependency addon Source directories
    for (const AddonDependencySpec& dep : pkgContent.mDependencies)
    {
        ss << ",\n                \"" << packagesDirJson << dep.mId << "/Source\"";
    }
    // Add Vulkan SDK include path
#if PLATFORM_WINDOWS
    ss << ",\n                \"${env:VULKAN_SDK}/Include\"";
#else
    ss << ",\n                \"${env:VULKAN_SDK}/include\"";
#endif
    ss << "\n            ],\n";

    ss << "            \"defines\": [";
    bool firstDefine = true;
    for (const std::string& define : defines)
    {
        if (!firstDefine) ss << ",";
        ss << "\n                \"" << define << "\"";
        firstDefine = false;
    }
    // Add export macro for this plugin
    std::string exportMacroJson = metadata.mExportDefine.empty()
        ? GenerateExportMacroName(addonId)
        : metadata.mExportDefine;
    ss << ",\n                \"" << exportMacroJson << "\"";
    ss << "\n            ],\n";

    ss << "            \"cStandard\": \"c17\",\n";
    ss << "            \"cppStandard\": \"c++17\",\n";
#if PLATFORM_WINDOWS
    ss << "            \"intelliSenseMode\": \"windows-msvc-x64\"\n";
#else
    ss << "            \"intelliSenseMode\": \"linux-gcc-x64\"\n";
#endif
    ss << "        }\n";
    ss << "    ],\n";
    ss << "    \"version\": 4\n";
    ss << "}\n";

    std::string configPath = vscodeDir + "c_cpp_properties.json";
    std::string content = ss.str();
    Stream stream(content.c_str(), (uint32_t)content.size());
    return stream.WriteFile(configPath.c_str());
}

bool NativeAddonManager::WriteCMakeLists(const std::string& addonPath, const std::string& binaryName)
{
    std::string polyphasePath = SYS_GetPolyphasePath();

    // Helper lambda to normalize paths for CMake (use forward slashes)
    auto normalizePath = [](const std::string& path) -> std::string {
        std::string result;
        for (char c : path)
        {
            if (c == '\\')
                result += '/';
            else
                result += c;
        }
        return result;
    };

    std::string polyphasePathCMake = normalizePath(polyphasePath);

    // Try to load from manifest, fall back to hardcoded paths
    std::vector<std::string> includePaths;
    std::vector<std::string> defines;

    if (!LoadAddonIncludesManifest(includePaths, defines))
    {
        // Fallback to hardcoded paths
        includePaths = {
            "Engine/Source",
            "Engine/Source/Engine",
            "Engine/Source/Plugins",
            "External",
            "External/Assimp",
            "External/Bullet",
            "External/Lua",
            "External/glm",
            "External/Imgui",
            "External/ImGuizmo",
            "External/Vorbis"
        };
        defines = {
            "OCTAVE_PLUGIN_EXPORT",
            "EDITOR=1",
            "LUA_ENABLED=1",
            "GLM_FORCE_RADIANS",
#if PLATFORM_WINDOWS
            "PLATFORM_WINDOWS=1",
            "API_VULKAN=1",
            "NOMINMAX"
#elif PLATFORM_LINUX
            "PLATFORM_LINUX=1",
            "API_VULKAN=1"
#elif PLATFORM_ANDROID
            "PLATFORM_ANDROID=1",
            "API_VULKAN=1"
#elif PLATFORM_3DS
            "PLATFORM_3DS=1",
            "API_C3D=1"
#endif
        };
    }

    std::stringstream ss;
    ss << "cmake_minimum_required(VERSION 3.15)\n";
    ss << "project(" << binaryName << ")\n";
    ss << "\n";
    ss << "set(CMAKE_CXX_STANDARD 17)\n";
    ss << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
    ss << "\n";
    // Resolve POLYPHASE_PATH at configure time. The probes mirror what the
    // .vcxproj does: try the env var, then walk up looking for a
    // Polyphase.sln (covers addon-near-engine layouts), then default
    // install locations, then the captured generation-time path.
    ss << "# Polyphase Engine path resolution.\n";
    ss << "#\n";
    ss << "# Priority order:\n";
    ss << "#   1. -DPOLYPHASE_PATH=... or already set in parent script\n";
    ss << "#   2. POLYPHASE_PATH environment variable\n";
    ss << "#   3. Walk up from this CMakeLists looking for Polyphase.sln\n";
    ss << "#      (catches addon-under-engine and sibling-repo layouts)\n";
    ss << "#   4. Default install locations (C:/Polyphase, /opt/Polyphase)\n";
    ss << "#   5. Engine path captured at generation time (last resort)\n";
    ss << "# Walk up from this CMakeLists looking for either:\n";
    ss << "#   - PolyphaseConfig.cmake : project-local engine path written\n";
    ss << "#                             by the editor on project open. This\n";
    ss << "#                             is the canonical answer for addons\n";
    ss << "#                             living in <project>/Packages.\n";
    ss << "#   - Polyphase.sln         : engine source root, for addons\n";
    ss << "#                             checked out under or beside the\n";
    ss << "#                             engine repo.\n";
    ss << "function(_polyphase_find_via_walk OUT_VAR)\n";
    ss << "    set(_p \"${CMAKE_CURRENT_SOURCE_DIR}\")\n";
    ss << "    foreach(_i RANGE 1 6)\n";
    ss << "        get_filename_component(_p \"${_p}\" DIRECTORY)\n";
    ss << "        if(EXISTS \"${_p}/PolyphaseConfig.cmake\")\n";
    ss << "            include(\"${_p}/PolyphaseConfig.cmake\")\n";
    ss << "            if(DEFINED POLYPHASE_PATH AND NOT POLYPHASE_PATH STREQUAL \"\")\n";
    ss << "                set(${OUT_VAR} \"${POLYPHASE_PATH}\" PARENT_SCOPE)\n";
    ss << "                return()\n";
    ss << "            endif()\n";
    ss << "        endif()\n";
    ss << "        if(EXISTS \"${_p}/Polyphase.sln\")\n";
    ss << "            set(${OUT_VAR} \"${_p}\" PARENT_SCOPE)\n";
    ss << "            return()\n";
    ss << "        endif()\n";
    ss << "    endforeach()\n";
    ss << "    set(${OUT_VAR} \"\" PARENT_SCOPE)\n";
    ss << "endfunction()\n";
    ss << "\n";
    ss << "if(NOT DEFINED POLYPHASE_PATH OR POLYPHASE_PATH STREQUAL \"\")\n";
    ss << "    if(DEFINED ENV{POLYPHASE_PATH} AND NOT \"$ENV{POLYPHASE_PATH}\" STREQUAL \"\")\n";
    ss << "        set(POLYPHASE_PATH \"$ENV{POLYPHASE_PATH}\")\n";
    ss << "    else()\n";
    ss << "        _polyphase_find_via_walk(_walk)\n";
    ss << "        if(NOT _walk STREQUAL \"\")\n";
    ss << "            set(POLYPHASE_PATH \"${_walk}\")\n";
    ss << "        elseif(WIN32 AND EXISTS \"C:/Polyphase/Polyphase.exe\")\n";
    ss << "            set(POLYPHASE_PATH \"C:/Polyphase\")\n";
    ss << "        elseif(WIN32 AND EXISTS \"C:/Program Files/Polyphase/Polyphase.exe\")\n";
    ss << "            set(POLYPHASE_PATH \"C:/Program Files/Polyphase\")\n";
    ss << "        elseif(UNIX AND EXISTS \"/opt/Polyphase/PolyphaseEditor\")\n";
    ss << "            set(POLYPHASE_PATH \"/opt/Polyphase\")\n";
    ss << "        else()\n";
    ss << "            set(POLYPHASE_PATH \"" << polyphasePathCMake << "\")\n";
    ss << "        endif()\n";
    ss << "    endif()\n";
    ss << "endif()\n";
    ss << "message(STATUS \"Polyphase: using POLYPHASE_PATH = ${POLYPHASE_PATH}\")\n";
    ss << "\n";
    ss << "# Gather source files\n";
    ss << "file(GLOB_RECURSE SOURCES \"Source/*.cpp\" \"Source/*.c\")\n";
    ss << "file(GLOB_RECURSE HEADERS \"Source/*.h\" \"Source/*.hpp\")\n";
    ss << "\n";
    ss << "# Create shared library\n";
    ss << "add_library(" << binaryName << " SHARED ${SOURCES} ${HEADERS})\n";
    ss << "\n";
    ss << "# Find Vulkan SDK\n";
    ss << "if(DEFINED ENV{VULKAN_SDK})\n";
    ss << "    set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})\n";
    ss << "else()\n";
    ss << "    find_package(Vulkan QUIET)\n";
    ss << "    if(Vulkan_FOUND)\n";
    ss << "        get_filename_component(VULKAN_SDK_PATH ${Vulkan_INCLUDE_DIRS} DIRECTORY)\n";
    ss << "    endif()\n";
    ss << "endif()\n";
    ss << "\n";
    // Parse package.json for dependencies
    std::string packageJsonPath = addonPath + "package.json";
    NativeModuleMetadata metadata;
    ContentMetadata pkgContent;
    ParsePackageJson(packageJsonPath, metadata, &pkgContent);

    // Get parent Packages directory and addon name for resolving sibling addon dependencies
    std::string packagesDir;
    std::string addonName;
    {
        std::string path = addonPath;
        while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        {
            path.pop_back();
        }
        size_t lastSlash = path.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            packagesDir = path.substr(0, lastSlash + 1);
            addonName = path.substr(lastSlash + 1);
        }
    }

    ss << "# Include directories\n";
    ss << "target_include_directories(" << binaryName << " PRIVATE\n";
    ss << "    ${CMAKE_CURRENT_SOURCE_DIR}/Source\n";

    // Add include paths from manifest
    for (const std::string& path : includePaths)
    {
        ss << "    ${POLYPHASE_PATH}/" << path << "\n";
    }

    // Add dependency addon Source directories
    for (const AddonDependencySpec& dep : pkgContent.mDependencies)
    {
        std::string depSourceDir = normalizePath(packagesDir + dep.mId + "/Source");
        ss << "    " << depSourceDir << "\n";
    }

    // Add Vulkan SDK include path
    ss << ")\n";
    ss << "\n";
    ss << "# Add Vulkan SDK include path if found\n";
    ss << "if(VULKAN_SDK_PATH)\n";
    ss << "    target_include_directories(" << binaryName << " PRIVATE ${VULKAN_SDK_PATH}/Include)\n";
    ss << "endif()\n";
    ss << "\n";
    ss << "# Compile definitions\n";
    ss << "target_compile_definitions(" << binaryName << " PRIVATE\n";
    for (const std::string& define : defines)
    {
        ss << "    " << define << "\n";
    }
    // Add export macro for this plugin
    std::string exportMacroCMake = metadata.mExportDefine.empty()
        ? GenerateExportMacroName(addonName)
        : metadata.mExportDefine;
    ss << "    " << exportMacroCMake << "\n";
    ss << ")\n";
    ss << "\n";
    // Search both the installed-editor layout (libs next to Polyphase.exe
    // or under <install>/lib/) and the engine-source-tree layout. Installed
    // entries are listed first so a user building this addon against an
    // installed editor doesn't accidentally pick up a dev-tree Polyphase.lib.
    ss << "# Link against Polyphase import library and dependencies\n";
    ss << "if(WIN32)\n";
    ss << "    if(CMAKE_BUILD_TYPE STREQUAL \"Debug\")\n";
    ss << "        set(POLYPHASE_DEV_LIB_PATH \"${POLYPHASE_PATH}/Standalone/Build/Windows/x64/DebugEditor\")\n";
    ss << "        set(POLYPHASE_DEV_LUA_PATH \"${POLYPHASE_PATH}/External/Lua/Build/Windows/x64/DebugEditor\")\n";
    ss << "    else()\n";
    ss << "        set(POLYPHASE_DEV_LIB_PATH \"${POLYPHASE_PATH}/Standalone/Build/Windows/x64/ReleaseEditor\")\n";
    ss << "        set(POLYPHASE_DEV_LUA_PATH \"${POLYPHASE_PATH}/External/Lua/Build/Windows/x64/ReleaseEditor\")\n";
    ss << "    endif()\n";
    ss << "    target_link_directories(" << binaryName << " PRIVATE\n";
    ss << "        \"${POLYPHASE_PATH}\"\n";
    ss << "        \"${POLYPHASE_PATH}/lib\"\n";
    ss << "        \"${POLYPHASE_DEV_LIB_PATH}\"\n";
    ss << "        \"${POLYPHASE_DEV_LUA_PATH}\")\n";
    ss << "    target_link_libraries(" << binaryName << " PRIVATE Polyphase Lua)\n";

    // Add dependency link directories and libraries
    std::string packagesDirCMake = normalizePath(packagesDir);
    for (const AddonDependencySpec& dep : pkgContent.mDependencies)
    {
        std::string depLibName = GenerateLibraryName(dep.mId);
        ss << "    target_link_directories(" << binaryName << " PRIVATE \"" << packagesDirCMake << dep.mId << "/Build\")\n";
        ss << "    target_link_libraries(" << binaryName << " PRIVATE " << depLibName << ")\n";
    }
    ss << "endif()\n";

    std::string cmakePath = addonPath + "CMakeLists.txt";
    std::string content = ss.str();
    Stream stream(content.c_str(), (uint32_t)content.size());
    return stream.WriteFile(cmakePath.c_str());
}

bool NativeAddonManager::WriteVSProject(const std::string& addonPath, const std::string& addonName,
                                         const std::string& binaryName)
{
    std::string polyphasePath = SYS_GetPolyphasePath();

    // Helper lambda to normalize paths for XML (use backslashes on Windows)
    auto normalizePathVS = [](const std::string& path) -> std::string {
        std::string result;
        for (char c : path)
        {
            if (c == '/')
                result += '\\';
            else
                result += c;
        }
        return result;
    };

    std::string polyphasePathVS = normalizePathVS(polyphasePath);

    // Try to load from manifest, fall back to hardcoded paths
    std::vector<std::string> includePaths;
    std::vector<std::string> defines;

    if (!LoadAddonIncludesManifest(includePaths, defines))
    {
        // Fallback to hardcoded paths
        includePaths = {
            "Engine/Source",
            "Engine/Source/Engine",
            "Engine/Source/Plugins",
            "External",
            "External/Assimp",
            "External/Bullet",
            "External/Lua",
            "External/glm",
            "External/Imgui",
            "External/ImGuizmo",
            "External/Vorbis"
        };
        defines = {
            "OCTAVE_PLUGIN_EXPORT",
            "EDITOR=1",
            "LUA_ENABLED=1",
            "GLM_FORCE_RADIANS",
#if PLATFORM_WINDOWS
            "PLATFORM_WINDOWS=1",
            "API_VULKAN=1",
            "NOMINMAX"
#elif PLATFORM_LINUX
            "PLATFORM_LINUX=1",
            "API_VULKAN=1"
#elif PLATFORM_ANDROID
            "PLATFORM_ANDROID=1",
            "API_VULKAN=1"
#elif PLATFORM_3DS
            "PLATFORM_3DS=1",
            "API_C3D=1"
#endif
        };
    }

    std::string sourceDir = addonPath + "Source";
    std::string sourceDirVS = normalizePathVS(sourceDir);

    // Gather source files for the project
    std::vector<std::string> sourceFiles = GatherSourceFiles(sourceDir + "/");

    // Generate a simple GUID (not truly unique but sufficient for local use)
    // Format: {8-4-4-4-12}
    std::string guid = "{12345678-1234-1234-1234-123456789ABC}";

    std::stringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    ss << "<Project DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n";
    ss << "  <ItemGroup Label=\"ProjectConfigurations\">\n";
    ss << "    <ProjectConfiguration Include=\"Debug|x64\">\n";
    ss << "      <Configuration>Debug</Configuration>\n";
    ss << "      <Platform>x64</Platform>\n";
    ss << "    </ProjectConfiguration>\n";
    ss << "    <ProjectConfiguration Include=\"Release|x64\">\n";
    ss << "      <Configuration>Release</Configuration>\n";
    ss << "      <Platform>x64</Platform>\n";
    ss << "    </ProjectConfiguration>\n";
    ss << "  </ItemGroup>\n";
    ss << "  <PropertyGroup Label=\"Globals\">\n";
    ss << "    <VCProjectVersion>16.0</VCProjectVersion>\n";
    ss << "    <ProjectGuid>" << guid << "</ProjectGuid>\n";
    ss << "    <RootNamespace>" << binaryName << "</RootNamespace>\n";
    ss << "    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>\n";
    ss << "  </PropertyGroup>\n";
    ss << "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\n";
    ss << "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\" Label=\"Configuration\">\n";
    ss << "    <ConfigurationType>DynamicLibrary</ConfigurationType>\n";
    ss << "    <UseDebugLibraries>true</UseDebugLibraries>\n";
    ss << "    <PlatformToolset>v143</PlatformToolset>\n";
    ss << "    <CharacterSet>Unicode</CharacterSet>\n";
    ss << "  </PropertyGroup>\n";
    ss << "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\" Label=\"Configuration\">\n";
    ss << "    <ConfigurationType>DynamicLibrary</ConfigurationType>\n";
    ss << "    <UseDebugLibraries>false</UseDebugLibraries>\n";
    ss << "    <PlatformToolset>v143</PlatformToolset>\n";
    ss << "    <WholeProgramOptimization>true</WholeProgramOptimization>\n";
    ss << "    <CharacterSet>Unicode</CharacterSet>\n";
    ss << "  </PropertyGroup>\n";
    ss << "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\n";
    ss << "  <ImportGroup Label=\"ExtensionSettings\">\n";
    ss << "  </ImportGroup>\n";
    ss << "  <ImportGroup Label=\"Shared\">\n";
    ss << "  </ImportGroup>\n";
    ss << "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n";
    ss << "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\n";
    ss << "  </ImportGroup>\n";
    ss << "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n";
    ss << "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\n";
    ss << "  </ImportGroup>\n";
    // Compute relative path from the addon's project dir to the engine
    // source root (using \ separators for MSBuild). If both ever stay in
    // the same relative layout, this resolves correctly across machines
    // without any env-var setup. We probe `<addon>/<rel>/Polyphase.sln`
    // before falling back to install locations.
    std::string addonRelToEngine;
    {
        // Manual relative-path computation (engine doesn't use <filesystem>).
        // Both inputs are absolute. Normalise slashes, split on /, drop the
        // common prefix, then ".." for each remaining segment in addonPath
        // and append the engine's tail.
        auto split = [](std::string p) {
            std::vector<std::string> out;
            for (char& c : p) if (c == '\\') c = '/';
            // strip trailing slash
            while (!p.empty() && (p.back() == '/' || p.back() == ' ')) p.pop_back();
            size_t i = 0;
            while (i < p.size())
            {
                size_t j = p.find('/', i);
                if (j == std::string::npos) j = p.size();
                if (j > i) out.push_back(p.substr(i, j - i));
                i = j + 1;
            }
            return out;
        };

        std::vector<std::string> a = split(addonPath);
        std::vector<std::string> e = split(polyphasePath);

        // Drive letters / case-insensitive on Windows
        auto eqi = [](const std::string& x, const std::string& y) {
            if (x.size() != y.size()) return false;
            for (size_t k = 0; k < x.size(); ++k)
            {
                char cx = x[k], cy = y[k];
                if (cx >= 'A' && cx <= 'Z') cx = (char)(cx + 32);
                if (cy >= 'A' && cy <= 'Z') cy = (char)(cy + 32);
                if (cx != cy) return false;
            }
            return true;
        };

        size_t common = 0;
        while (common < a.size() && common < e.size() && eqi(a[common], e[common]))
            ++common;

        // Different drive letters → no relative path possible.
        if (common > 0)
        {
            std::string rel;
            for (size_t k = common; k < a.size(); ++k) rel += "..\\";
            for (size_t k = common; k < e.size(); ++k)
            {
                rel += e[k];
                if (k + 1 < e.size()) rel += "\\";
            }
            // Trim trailing backslash for the Exists() check, but keep one
            // before "Polyphase.sln" when concatenating.
            while (!rel.empty() && rel.back() == '\\') rel.pop_back();
            addonRelToEngine = rel;
        }
    }

    // Resolve $(PolyphasePath) at build time so the project is portable
    // across machines. Priority order:
    //   1. PolyphasePath already set (parent .props, -p:PolyphasePath=...).
    //   2. POLYPHASE_PATH env var (recommended dev/install setup).
    //   3. PolyphaseDevPath env var (alt convention).
    //   4. Generation-time relative layout: addon-near-engine survives the
    //      whole tree being moved as long as relative positions stay the same.
    //   5. Common dev layouts: addon under a Packages/ or Addons/ folder
    //      next to the engine, sibling addon repo, etc. Walks ../ a few
    //      levels looking for Polyphase.sln.
    //   6. Default Windows install locations.
    //   7. Generation-time absolute path (last resort, machine-specific).
    ss << "  <PropertyGroup Label=\"UserMacros\">\n";
    ss << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and '$(POLYPHASE_PATH)' != ''\">$(POLYPHASE_PATH)</PolyphasePath>\n";
    ss << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and '$(PolyphaseDevPath)' != ''\">$(PolyphaseDevPath)</PolyphasePath>\n";

    if (!addonRelToEngine.empty())
    {
        ss << "    <!-- Relative layout from generation: addon ../engine -->\n";
        ss << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and Exists('$(ProjectDir)" << addonRelToEngine << "\\Polyphase.sln')\">$([System.IO.Path]::GetFullPath('$(ProjectDir)" << addonRelToEngine << "'))</PolyphasePath>\n";
    }

    // Generic ../ walk for common addon-near-engine layouts. Each rule
    // probes for Polyphase.sln to confirm the candidate is actually a
    // Polyphase source root, not just any directory.
    ss << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and Exists('$(ProjectDir)..\\Polyphase.sln')\">$([System.IO.Path]::GetFullPath('$(ProjectDir)..'))</PolyphasePath>\n";
    ss << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and Exists('$(ProjectDir)..\\..\\Polyphase.sln')\">$([System.IO.Path]::GetFullPath('$(ProjectDir)..\\..'))</PolyphasePath>\n";
    ss << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and Exists('$(ProjectDir)..\\..\\..\\Polyphase.sln')\">$([System.IO.Path]::GetFullPath('$(ProjectDir)..\\..\\..'))</PolyphasePath>\n";
    ss << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and Exists('$(ProjectDir)..\\..\\..\\..\\Polyphase.sln')\">$([System.IO.Path]::GetFullPath('$(ProjectDir)..\\..\\..\\..'))</PolyphasePath>\n";

    ss << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and Exists('C:\\Polyphase\\Polyphase.exe')\">C:\\Polyphase</PolyphasePath>\n";
    ss << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and Exists('C:\\Program Files\\Polyphase\\Polyphase.exe')\">C:\\Program Files\\Polyphase</PolyphasePath>\n";
    ss << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and Exists('C:\\Program Files (x86)\\Polyphase\\Polyphase.exe')\">C:\\Program Files (x86)\\Polyphase</PolyphasePath>\n";
    ss << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == ''\">" << polyphasePathVS << "</PolyphasePath>\n";
    ss << "  </PropertyGroup>\n";
    ss << "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n";
    ss << "    <OutDir>$(ProjectDir)Build\\Debug\\</OutDir>\n";
    ss << "    <IntDir>$(ProjectDir)Build\\Intermediate\\Debug\\</IntDir>\n";
    ss << "    <TargetName>" << binaryName << "</TargetName>\n";
    ss << "  </PropertyGroup>\n";
    ss << "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n";
    ss << "    <OutDir>$(ProjectDir)Build\\Release\\</OutDir>\n";
    ss << "    <IntDir>$(ProjectDir)Build\\Intermediate\\Release\\</IntDir>\n";
    ss << "    <TargetName>" << binaryName << "</TargetName>\n";
    ss << "  </PropertyGroup>\n";

    // Parse package.json for dependencies
    std::string packageJsonPath = addonPath + "package.json";
    NativeModuleMetadata metadata;
    ContentMetadata pkgContent;
    ParsePackageJson(packageJsonPath, metadata, &pkgContent);

    // Get parent Packages directory for resolving sibling addon dependencies
    std::string packagesDir;
    {
        std::string path = addonPath;
        while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        {
            path.pop_back();
        }
        size_t lastSlash = path.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            packagesDir = path.substr(0, lastSlash + 1);
        }
    }

    // Build include directories string from manifest paths. Use the MSBuild
    // macro $(PolyphasePath) instead of the absolute path captured at
    // generation time so the project is portable across machines (UserMacros
    // chain at the top of the .vcxproj resolves it per-build).
    std::string includesStr;
    for (const std::string& path : includePaths)
    {
        includesStr += std::string("$(PolyphasePath)") + normalizePathVS(path) + ";";
    }
    // Add dependency addon Source directories
    for (const AddonDependencySpec& dep : pkgContent.mDependencies)
    {
        includesStr += normalizePathVS(packagesDir + dep.mId + "/Source") + ";";
    }
    // Add Vulkan SDK include path
    includesStr += "$(VULKAN_SDK)\\Include;";
    includesStr += "$(ProjectDir)Source;%(AdditionalIncludeDirectories)";

    // Build preprocessor definitions string from manifest
    std::string definesStr;
    for (const std::string& define : defines)
    {
        definesStr += define + ";";
    }

    // Add export macro for this plugin (so it exports its symbols when building)
    std::string exportMacro = metadata.mExportDefine.empty()
        ? GenerateExportMacroName(addonName)
        : metadata.mExportDefine;
    definesStr += exportMacro + ";";

    // Library search paths use the $(PolyphasePath) MSBuild macro so they
    // re-resolve correctly per machine (see UserMacros block above). Order:
    //   1. $(PolyphasePath)          - installed editor: libs sit beside .exe
    //   2. $(PolyphasePath)\lib      - alt installed layout
    //   3. dev build output (DebugEditor / ReleaseEditor)
    std::string polyphaseLibPathDebug =
        "$(PolyphasePath)\\;$(PolyphasePath)\\lib\\;$(PolyphasePath)\\Standalone\\Build\\Windows\\x64\\DebugEditor\\";
    std::string polyphaseLibPathRelease =
        "$(PolyphasePath)\\;$(PolyphasePath)\\lib\\;$(PolyphasePath)\\Standalone\\Build\\Windows\\x64\\ReleaseEditor\\";
    std::string luaLibPathDebug   = "$(PolyphasePath)\\External\\Lua\\Build\\Windows\\x64\\DebugEditor\\";
    std::string luaLibPathRelease = "$(PolyphasePath)\\External\\Lua\\Build\\Windows\\x64\\ReleaseEditor\\";

    // Build library paths and dependencies for dependencies
    std::string depLibPaths;
    std::string depLibs;
    for (const AddonDependencySpec& dep : pkgContent.mDependencies)
    {
        // Add dependency's build output directory to library search path
        std::string depBuildPath = normalizePathVS(packagesDir + dep.mId + "/Build");
        depLibPaths += depBuildPath + "\\Debug;";
        depLibPaths += depBuildPath + "\\Release;";

        // Add dependency's .lib file to linker dependencies
        std::string depLibName = GenerateLibraryName(dep.mId) + ".lib;";
        depLibs += depLibName;
    }

    ss << "  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n";
    ss << "    <ClCompile>\n";
    ss << "      <WarningLevel>Level3</WarningLevel>\n";
    ss << "      <SDLCheck>true</SDLCheck>\n";
    ss << "      <PreprocessorDefinitions>" << definesStr << "_DEBUG;_WINDOWS;_USRDLL;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n";
    ss << "      <ConformanceMode>true</ConformanceMode>\n";
    ss << "      <LanguageStandard>stdcpp17</LanguageStandard>\n";
    ss << "      <AdditionalIncludeDirectories>" << includesStr << "</AdditionalIncludeDirectories>\n";
    ss << "    </ClCompile>\n";
    ss << "    <Link>\n";
    ss << "      <SubSystem>Windows</SubSystem>\n";
    ss << "      <GenerateDebugInformation>true</GenerateDebugInformation>\n";
    ss << "      <AdditionalLibraryDirectories>" << polyphaseLibPathDebug << ";" << luaLibPathDebug << ";" << depLibPaths << "%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>\n";
    ss << "      <AdditionalDependencies>Polyphase.lib;Lua.lib;" << depLibs << "%(AdditionalDependencies)</AdditionalDependencies>\n";
    ss << "    </Link>\n";
    ss << "  </ItemDefinitionGroup>\n";
    ss << "  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n";
    ss << "    <ClCompile>\n";
    ss << "      <WarningLevel>Level3</WarningLevel>\n";
    ss << "      <FunctionLevelLinking>true</FunctionLevelLinking>\n";
    ss << "      <IntrinsicFunctions>true</IntrinsicFunctions>\n";
    ss << "      <SDLCheck>true</SDLCheck>\n";
    ss << "      <PreprocessorDefinitions>" << definesStr << "NDEBUG;_WINDOWS;_USRDLL;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n";
    ss << "      <ConformanceMode>true</ConformanceMode>\n";
    ss << "      <LanguageStandard>stdcpp17</LanguageStandard>\n";
    ss << "      <AdditionalIncludeDirectories>" << includesStr << "</AdditionalIncludeDirectories>\n";
    ss << "    </ClCompile>\n";
    ss << "    <Link>\n";
    ss << "      <SubSystem>Windows</SubSystem>\n";
    ss << "      <EnableCOMDATFolding>true</EnableCOMDATFolding>\n";
    ss << "      <OptimizeReferences>true</OptimizeReferences>\n";
    ss << "      <GenerateDebugInformation>true</GenerateDebugInformation>\n";
    ss << "      <AdditionalLibraryDirectories>" << polyphaseLibPathRelease << ";" << luaLibPathRelease << ";" << depLibPaths << "%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>\n";
    ss << "      <AdditionalDependencies>Polyphase.lib;Lua.lib;" << depLibs << "%(AdditionalDependencies)</AdditionalDependencies>\n";
    ss << "    </Link>\n";
    ss << "  </ItemDefinitionGroup>\n";

    // Add source files
    ss << "  <ItemGroup>\n";
    for (const std::string& file : sourceFiles)
    {
        std::string ext = "";
        size_t dotPos = file.find_last_of('.');
        if (dotPos != std::string::npos)
        {
            ext = file.substr(dotPos);
        }

        // Convert to relative path and backslashes
        std::string relPath = file;
        if (relPath.find(addonPath) == 0)
        {
            relPath = relPath.substr(addonPath.length());
        }
        std::string relPathVS;
        for (char c : relPath)
        {
            if (c == '/')
                relPathVS += '\\';
            else
                relPathVS += c;
        }

        if (ext == ".cpp" || ext == ".c")
        {
            ss << "    <ClCompile Include=\"" << relPathVS << "\" />\n";
        }
        else if (ext == ".h" || ext == ".hpp")
        {
            ss << "    <ClInclude Include=\"" << relPathVS << "\" />\n";
        }
    }
    ss << "  </ItemGroup>\n";

    ss << "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\n";
    ss << "  <ImportGroup Label=\"ExtensionTargets\">\n";
    ss << "  </ImportGroup>\n";
    ss << "</Project>\n";

    std::string vcxprojPath = addonPath + binaryName + ".vcxproj";
    std::string content = ss.str();
    Stream stream(content.c_str(), (uint32_t)content.size());
    return stream.WriteFile(vcxprojPath.c_str());
}

bool NativeAddonManager::GenerateIDEConfig(const std::string& addonPath)
{
    bool success = WriteVSCodeConfig(addonPath);

    // Also parse package.json to get binary name for CMakeLists
    std::string packageJsonPath = addonPath + "package.json";
    NativeModuleMetadata metadata;
    if (ParsePackageJson(packageJsonPath, metadata))
    {
        std::string binaryName = metadata.mBinaryName;
        if (binaryName.empty())
        {
            // Try to extract from addon folder name
            std::string path = addonPath;
            while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
            {
                path.pop_back();
            }
            size_t lastSlash = path.find_last_of("/\\");
            if (lastSlash != std::string::npos)
            {
                binaryName = path.substr(lastSlash + 1);
            }
        }

        if (!binaryName.empty())
        {
            WriteCMakeLists(addonPath, binaryName);

            // Get addon name for VS project (use folder name if not in metadata)
            std::string addonName = binaryName;
            std::string path = addonPath;
            while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
            {
                path.pop_back();
            }
            size_t lastSlash = path.find_last_of("/\\");
            if (lastSlash != std::string::npos)
            {
                addonName = path.substr(lastSlash + 1);
            }

            WriteVSProject(addonPath, addonName, binaryName);
        }
    }

    return success;
}

bool NativeAddonManager::CreateNativeAddon(const NativeAddonCreateInfo& info, std::string& outError, std::string* outPath)
{
    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    if (projectDir.empty())
    {
        outError = "No project loaded";
        return false;
    }

    std::string packagesDir = projectDir + "Packages/";
    return CreateNativeAddonAtPath(info, packagesDir, outError, outPath);
}

bool NativeAddonManager::CreateNativeAddonAtPath(const NativeAddonCreateInfo& info, const std::string& targetDir,
                                                   std::string& outError, std::string* outPath)
{
    // Validate name
    if (info.mName.empty())
    {
        outError = "Addon name is required";
        return false;
    }

    // Generate ID from name if not provided
    std::string addonId = info.mId.empty() ? GenerateIdFromName(info.mName) : info.mId;
    if (addonId.empty())
    {
        outError = "Could not generate valid addon ID from name";
        return false;
    }

    // Generate binary name if not provided
    std::string binaryName = info.mBinaryName.empty() ? addonId : info.mBinaryName;
    // Remove hyphens for binary name (use underscores instead)
    std::string binaryNameClean;
    for (char c : binaryName)
    {
        if (c == '-')
            binaryNameClean += '_';
        else
            binaryNameClean += c;
    }

    // Create target directory if it doesn't exist
    if (!DoesDirExist(targetDir.c_str()))
    {
        SYS_CreateDirectory(targetDir.c_str());
    }

    // Create addon directory
    std::string normalizedTarget = targetDir;
    if (!normalizedTarget.empty() && normalizedTarget.back() != '/' && normalizedTarget.back() != '\\')
    {
        normalizedTarget += '/';
    }
    std::string addonPath = normalizedTarget + addonId + "/";
    if (DoesDirExist(addonPath.c_str()))
    {
        outError = "Addon folder already exists: " + addonPath;
        return false;
    }

    SYS_CreateDirectory(addonPath.c_str());

    // Create Source directory
    std::string sourceDir = addonPath + "Source/";
    SYS_CreateDirectory(sourceDir.c_str());

    // Create Assets directory (empty, for user to add assets)
    std::string assetsDir = addonPath + "Assets/";
    SYS_CreateDirectory(assetsDir.c_str());

    // Create Scripts directory (empty, for user to add scripts)
    std::string scriptsDir = addonPath + "Scripts/";
    SYS_CreateDirectory(scriptsDir.c_str());

    // Generate C++ identifier for class name
    std::string className;
    bool capitalizeNext = true;
    for (char c : info.mName)
    {
        if (std::isalnum(c))
        {
            if (capitalizeNext)
            {
                className += static_cast<char>(std::toupper(c));
                capitalizeNext = false;
            }
            else
            {
                className += c;
            }
        }
        else
        {
            capitalizeNext = true;
        }
    }
    if (className.empty())
    {
        className = "MyAddon";
    }

    // Write package.json
    NativeAddonCreateInfo finalInfo = info;
    finalInfo.mId = addonId;
    finalInfo.mBinaryName = binaryNameClean;
    if (!WritePackageJson(addonPath + "package.json", finalInfo))
    {
        outError = "Failed to write package.json";
        return false;
    }

    // Write template source file
    std::string sourceFile = sourceDir + className + ".cpp";
    if (!WriteTemplateSourceFile(sourceFile, info.mName, binaryNameClean))
    {
        outError = "Failed to write template source file";
        return false;
    }

    // Write IDE configurations
    WriteVSCodeConfig(addonPath);
    WriteCMakeLists(addonPath, binaryNameClean);
    WriteVSProject(addonPath, info.mName, binaryNameClean);

    // Write GitHub workflow template for binary releases
    WriteGitHubWorkflowTemplate(addonPath, binaryNameClean);

    // Discover the new addon
    DiscoverNativeAddons();

    LogDebug("Created native addon: %s at %s", addonId.c_str(), addonPath.c_str());

    // Return the created path
    if (outPath != nullptr)
    {
        *outPath = addonPath;
    }

    return true;
}

bool NativeAddonManager::PackageNativeAddon(const NativeAddonPackageOptions& options, std::string& outError)
{
    auto it = mStates.find(options.mAddonId);
    if (it == mStates.end())
    {
        outError = "Addon not found: " + options.mAddonId;
        return false;
    }

    const NativeAddonState& state = it->second;
    std::string addonPath = state.mSourcePath;

    // Verify addon exists
    if (!DoesDirExist(addonPath.c_str()))
    {
        outError = "Addon path not found: " + addonPath;
        return false;
    }

    // Determine output path
    std::string outputPath = options.mOutputPath;
    if (outputPath.empty())
    {
        const std::string& projectDir = GetEngineState()->mProjectDirectory;
        outputPath = projectDir + "Packaged/" + options.mAddonId + ".zip";
    }

    // Create output directory if needed
    size_t lastSlash = outputPath.find_last_of("/\\");
    if (lastSlash != std::string::npos)
    {
        std::string outputDir = outputPath.substr(0, lastSlash + 1);
        if (!DoesDirExist(outputDir.c_str()))
        {
            SYS_CreateDirectory(outputDir.c_str());
        }
    }

    // Build the zip command
    // We'll use a simple approach - create a temp directory with selected contents, then zip
    std::string tempDir = GetEngineState()->mProjectDirectory + "Intermediate/Package_" + options.mAddonId + "/";
    std::string tempAddonDir = tempDir + options.mAddonId + "/";

    // Clean and create temp directory
    if (DoesDirExist(tempDir.c_str()))
    {
        SYS_RemoveDirectory(tempDir.c_str());
    }
    SYS_CreateDirectory(tempDir.c_str());
    SYS_CreateDirectory(tempAddonDir.c_str());

    // Copy package.json (always required)
    std::string srcPackageJson = addonPath + "package.json";
    std::string dstPackageJson = tempAddonDir + "package.json";
    SYS_CopyFile(srcPackageJson.c_str(), dstPackageJson.c_str());

    // Copy selected contents
    if (options.mIncludeSource)
    {
        std::string srcDir = addonPath + "Source/";
        std::string dstDir = tempAddonDir + "Source/";
        if (DoesDirExist(srcDir.c_str()))
        {
            SYS_CopyDirectory(srcDir.c_str(), dstDir.c_str());
        }
    }

    if (options.mIncludeAssets)
    {
        std::string srcDir = addonPath + "Assets/";
        std::string dstDir = tempAddonDir + "Assets/";
        if (DoesDirExist(srcDir.c_str()))
        {
            SYS_CopyDirectory(srcDir.c_str(), dstDir.c_str());
        }
    }

    if (options.mIncludeScripts)
    {
        std::string srcDir = addonPath + "Scripts/";
        std::string dstDir = tempAddonDir + "Scripts/";
        if (DoesDirExist(srcDir.c_str()))
        {
            SYS_CopyDirectory(srcDir.c_str(), dstDir.c_str());
        }
    }

    if (options.mIncludeThumbnail)
    {
        std::string srcFile = addonPath + "thumbnail.png";
        std::string dstFile = tempAddonDir + "thumbnail.png";
        if (SYS_DoesFileExist(srcFile.c_str(), false))
        {
            SYS_CopyFile(srcFile.c_str(), dstFile.c_str());
        }
    }

    // Create zip file
#if PLATFORM_WINDOWS
    // Use PowerShell to create zip on Windows
    std::string cmd = "powershell -Command \"Compress-Archive -Path '" + tempAddonDir + "*' -DestinationPath '" + outputPath + "' -Force\"";
#else
    // Use zip command on Linux
    std::string cmd = "cd \"" + tempDir + "\" && zip -r \"" + outputPath + "\" \"" + options.mAddonId + "\"";
#endif

    std::string cmdOutput;
    int exitCode = 0;
    bool success = SYS_ExecFull(cmd.c_str(), &cmdOutput, nullptr, &exitCode);

    // Clean up temp directory
    SYS_RemoveDirectory(tempDir.c_str());

    if (!success || exitCode != 0)
    {
        outError = "Failed to create zip file: " + cmdOutput;
        return false;
    }

    LogDebug("Packaged native addon %s to %s", options.mAddonId.c_str(), outputPath.c_str());
    return true;
}

#endif // EDITOR
