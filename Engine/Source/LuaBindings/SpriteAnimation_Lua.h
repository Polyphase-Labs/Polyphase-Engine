#pragma once

#include "EngineTypes.h"
#include "Log.h"

#include "Assets/SpriteAnimation.h"

#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define SPRITE_ANIMATION_LUA_NAME "SpriteAnimation"
#define SPRITE_ANIMATION_LUA_FLAG "cfSpriteAnimation"
#define CHECK_SPRITE_ANIMATION(L, arg) CheckAssetLuaType<SpriteAnimation>(L, arg, SPRITE_ANIMATION_LUA_NAME, SPRITE_ANIMATION_LUA_FLAG)

struct SpriteAnimation_Lua
{
    static int GetAnimationName(lua_State* L);
    static int SetAnimationName(lua_State* L);
    static int GetMode(lua_State* L);
    static int SetMode(lua_State* L);
    static int GetFps(lua_State* L);
    static int SetFps(lua_State* L);
    static int GetLoop(lua_State* L);
    static int SetLoop(lua_State* L);

    static int GetFrameCount(lua_State* L);
    static int AddFrame(lua_State* L);
    static int ClearFrames(lua_State* L);

    static int GetAtlasTexture(lua_State* L);
    static int SetAtlasTexture(lua_State* L);
    static int SetAtlasGrid(lua_State* L);
    static int SetAtlasMargin(lua_State* L);
    static int SetAtlasSpacing(lua_State* L);

    static void Bind();
};

#endif
