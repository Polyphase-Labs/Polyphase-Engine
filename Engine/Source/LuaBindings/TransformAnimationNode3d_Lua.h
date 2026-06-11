#pragma once

#include "EngineTypes.h"
#include "Log.h"
#include "Engine.h"

#include "Nodes/3D/TransformAnimationNode3d.h"

#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define TRANSFORM_ANIM_NODE3D_LUA_NAME "TransformAnimationNode3D"
#define TRANSFORM_ANIM_NODE3D_LUA_FLAG "cfTransformAnimNode3D"
#define CHECK_TRANSFORM_ANIM_NODE3D(L, arg) static_cast<TransformAnimationNode3D*>(CheckNodeLuaType(L, arg, TRANSFORM_ANIM_NODE3D_LUA_NAME, TRANSFORM_ANIM_NODE3D_LUA_FLAG));

struct TransformAnimationNode3D_Lua
{
    static int Play(lua_State* L);
    static int Pause(lua_State* L);
    static int Stop(lua_State* L);
    static int SetAnimation(lua_State* L);
    static int GetAnimation(lua_State* L);
    static int SetKeyframes(lua_State* L);
    static int SetTargetNode(lua_State* L);
    static int GetTargetNode(lua_State* L);
    static int ApplyKeyframe(lua_State* L);
    static int SampleNow(lua_State* L);
    static int SetTime(lua_State* L);
    static int GetTime(lua_State* L);
    static int GetDuration(lua_State* L);
    static int IsPlaying(lua_State* L);
    static int IsPaused(lua_State* L);
    static int GetProgress(lua_State* L);
    static int SetLoop(lua_State* L);
    static int IsLooping(lua_State* L);
    static int SetPlayRate(lua_State* L);
    static int GetPlayRate(lua_State* L);
    static int SetPlayOnStart(lua_State* L);
    static int GetPlayOnStart(lua_State* L);

    static void Bind();
};

#endif
