#pragma once

#include "EngineTypes.h"
#include "Log.h"
#include "Engine.h"

#include "Nodes/3D/Terrain3d.h"

#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define TERRAIN_3D_LUA_NAME "Terrain3D"
#define TERRAIN_3D_LUA_FLAG "cfTerrain3D"
#define CHECK_TERRAIN_3D(L, arg) static_cast<Terrain3D*>(CheckNodeLuaType(L, arg, TERRAIN_3D_LUA_NAME, TERRAIN_3D_LUA_FLAG));

struct Terrain3D_Lua
{
    // Heightmap
    static int SetHeight(lua_State* L);
    static int GetHeight(lua_State* L);
    static int GetHeightAtWorldPos(lua_State* L);
    static int SetHeightFromTexture(lua_State* L);
    static int FlattenAll(lua_State* L);
    static int Resize(lua_State* L);

    // Dimensions
    static int GetResolution(lua_State* L);
    static int GetWorldSize(lua_State* L);
    static int GetHeightScale(lua_State* L);
    static int SetHeightScale(lua_State* L);

    // Mesh control
    static int MarkDirty(lua_State* L);
    static int RebuildMesh(lua_State* L);
    static int IsDirty(lua_State* L);

    // Material slots
    static int SetMaterialSlot(lua_State* L);
    static int GetMaterialSlot(lua_State* L);

    // Splatmap
    static int SetMaterialWeight(lua_State* L);
    static int GetMaterialWeight(lua_State* L);

    // Snapping
    static int SetSnapGridSize(lua_State* L);
    static int GetSnapGridSize(lua_State* L);

    static void Bind();
};

#endif
