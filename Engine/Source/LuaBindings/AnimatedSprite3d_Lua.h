#pragma once

#include "EngineTypes.h"
#include "Log.h"
#include "Engine.h"

#include "Nodes/3D/AnimatedSprite3d.h"

#include "LuaBindings/Node3d_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define ANIMATED_SPRITE_3D_LUA_NAME "AnimatedSprite3D"
#define ANIMATED_SPRITE_3D_LUA_FLAG "cfAnimatedSprite3D"
#define CHECK_ANIMATED_SPRITE_3D(L, arg) static_cast<AnimatedSprite3D*>(CheckNodeLuaType(L, arg, ANIMATED_SPRITE_3D_LUA_NAME, ANIMATED_SPRITE_3D_LUA_FLAG));

struct AnimatedSprite3D_Lua
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

    static int SetMaterial(lua_State* L);
    static int GetMaterial(lua_State* L);
    static int SetAffectDiffuse(lua_State* L);
    static int SetAffectAlpha(lua_State* L);
    static int SetAffectEmission(lua_State* L);
    static int GetAffectDiffuse(lua_State* L);
    static int GetAffectAlpha(lua_State* L);
    static int GetAffectEmission(lua_State* L);

    static void Bind();
};

#endif
