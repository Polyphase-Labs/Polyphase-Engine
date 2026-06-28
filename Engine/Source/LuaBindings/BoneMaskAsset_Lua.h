#pragma once

#include "EngineTypes.h"
#include "Log.h"

#include "Assets/BoneMaskAsset.h"

#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define BONE_MASK_ASSET_LUA_NAME "BoneMaskAsset"
#define BONE_MASK_ASSET_LUA_FLAG "cfBoneMask"
#define CHECK_BONE_MASK_ASSET(L, arg) CheckAssetLuaType<BoneMaskAsset>(L, arg, BONE_MASK_ASSET_LUA_NAME, BONE_MASK_ASSET_LUA_FLAG)

struct BoneMaskAsset_Lua
{
    static int GetTargetMesh(lua_State* L);
    static int SetTargetMesh(lua_State* L);
    static int GetSelfOnly(lua_State* L);
    static int SetSelfOnly(lua_State* L);
    static int GetNumIncludeRoots(lua_State* L);
    static int GetIncludeRoot(lua_State* L);
    static int AddIncludeRoot(lua_State* L);
    static int RemoveIncludeRoot(lua_State* L);
    static int GetNumExcludeRoots(lua_State* L);
    static int GetExcludeRoot(lua_State* L);
    static int AddExcludeRoot(lua_State* L);
    static int RemoveExcludeRoot(lua_State* L);

    static void Bind();
};

#endif
