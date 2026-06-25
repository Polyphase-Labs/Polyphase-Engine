#pragma once

#include "EngineTypes.h"
#include "Log.h"

#include "Assets/SkeletalAnimationAsset.h"

#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define SKELETAL_ANIMATION_ASSET_LUA_NAME "SkeletalAnimationAsset"
#define SKELETAL_ANIMATION_ASSET_LUA_FLAG "cfSkeletalAnimation"
#define CHECK_SKELETAL_ANIMATION_ASSET(L, arg) CheckAssetLuaType<SkeletalAnimationAsset>(L, arg, SKELETAL_ANIMATION_ASSET_LUA_NAME, SKELETAL_ANIMATION_ASSET_LUA_FLAG)

struct SkeletalAnimationAsset_Lua
{
    static int GetClipName(lua_State* L);
    static int GetDuration(lua_State* L);
    static int GetDurationSeconds(lua_State* L);
    static int GetTicksPerSecond(lua_State* L);
    static int GetSourceRigName(lua_State* L);
    static int GetNumChannels(lua_State* L);
    static int GetChannelBoneName(lua_State* L);

    static void Bind();
};

#endif
