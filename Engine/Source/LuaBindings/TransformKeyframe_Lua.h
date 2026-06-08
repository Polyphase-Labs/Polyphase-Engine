#pragma once

#include "Engine.h"
#include "Timeline/TimelineTypes.h"

#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define TRANSFORM_KEYFRAME_LUA_NAME "TransformKeyframe"
#define CHECK_TRANSFORM_KEYFRAME(L, Arg) CheckLuaType<TransformKeyframe_Lua>(L, Arg, TRANSFORM_KEYFRAME_LUA_NAME)->mKeyframe;

struct TransformKeyframe_Lua
{
    TransformKeyframe mKeyframe;

    TransformKeyframe_Lua() {}
    ~TransformKeyframe_Lua() {}

    static int Create(lua_State* L);
    static int Create(lua_State* L, const TransformKeyframe& value);

    static int Index(lua_State* L);
    static int NewIndex(lua_State* L);
    static int ToString(lua_State* L);

    static int Lerp(lua_State* L);
    static int Clone(lua_State* L);

    static void Bind();
};

#endif
