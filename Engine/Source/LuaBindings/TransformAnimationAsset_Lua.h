#pragma once

#include "EngineTypes.h"
#include "Log.h"

#include "Assets/TransformAnimationAsset.h"

#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define TRANSFORM_ANIMATION_ASSET_LUA_NAME "TransformAnimationAsset"
#define TRANSFORM_ANIMATION_ASSET_LUA_FLAG "cfTransformAnim"
#define CHECK_TRANSFORM_ANIMATION_ASSET(L, arg) CheckAssetLuaType<TransformAnimationAsset>(L, arg, TRANSFORM_ANIMATION_ASSET_LUA_NAME, TRANSFORM_ANIMATION_ASSET_LUA_FLAG)

struct TransformAnimationAsset_Lua
{
    static int Sample(lua_State* L);
    static int GetDuration(lua_State* L);
    static int SetDuration(lua_State* L);
    static int GetKeyframeCount(lua_State* L);
    static int GetKeyframe(lua_State* L);
    static int IsLooping(lua_State* L);
    static int SetLooping(lua_State* L);
    static int GetPlayRate(lua_State* L);
    static int SetPlayRate(lua_State* L);
    static int AddKeyframe(lua_State* L);
    static int RemoveKeyframe(lua_State* L);
    static int ClearKeyframes(lua_State* L);

    static void Bind();
};

#endif
