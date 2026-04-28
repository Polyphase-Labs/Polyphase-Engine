#pragma once

#include "EngineTypes.h"
#include "Log.h"
#include "Engine.h"

#include "Nodes/Widgets/AnimatedWidget.h"

#include "LuaBindings/Quad_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define ANIMATED_WIDGET_LUA_NAME "AnimatedWidget"
#define ANIMATED_WIDGET_LUA_FLAG "cfAnimatedWidget"
#define CHECK_ANIMATED_WIDGET(L, arg) static_cast<AnimatedWidget*>(CheckNodeLuaType(L, arg, ANIMATED_WIDGET_LUA_NAME, ANIMATED_WIDGET_LUA_FLAG));

struct AnimatedWidget_Lua
{
    static int Play(lua_State* L);
    static int Pause(lua_State* L);
    static int Stop(lua_State* L);
    static int PlayAnimation(lua_State* L);
    static int IsPlaying(lua_State* L);
    static int SetFrame(lua_State* L);
    static int GetProgress(lua_State* L);
    static int AnimateTo(lua_State* L);
    static int AnimateToProgress(lua_State* L);
    static int CancelAnimateTo(lua_State* L);
    static int SetSpeed(lua_State* L);
    static int GetSpeed(lua_State* L);

    static int SetAutoPlay(lua_State* L);
    static int GetAutoPlay(lua_State* L);
    static int SetLoopOverride(lua_State* L);
    static int GetLoopOverride(lua_State* L);
    static int SetDefaultAnimation(lua_State* L);
    static int GetDefaultAnimation(lua_State* L);

    static int AddAnimation(lua_State* L);
    static int CreateAnimation(lua_State* L);
    static int AddImage(lua_State* L);
    static int AddImages(lua_State* L);
    static int RemoveAnimation(lua_State* L);
    static int HasAnimation(lua_State* L);

    static int GetCurrentTexture(lua_State* L);
    static int GetCurrentAnimationName(lua_State* L);
    static int GetCurrentFrameIndex(lua_State* L);

    static void Bind();
};

#endif
