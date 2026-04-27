#pragma once

#include "EngineTypes.h"
#include "Log.h"
#include "Engine.h"

#include "Nodes/SpriteAnimator.h"

#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define SPRITE_ANIMATOR_LUA_NAME "SpriteAnimator"
#define SPRITE_ANIMATOR_LUA_FLAG "cfSpriteAnimator"
#define CHECK_SPRITE_ANIMATOR(L, arg) static_cast<SpriteAnimator*>(CheckNodeLuaType(L, arg, SPRITE_ANIMATOR_LUA_NAME, SPRITE_ANIMATOR_LUA_FLAG));

struct SpriteAnimator_Lua
{
    // Playback
    static int Play(lua_State* L);
    static int Pause(lua_State* L);
    static int Stop(lua_State* L);
    static int PlayAnimation(lua_State* L);
    static int IsPlaying(lua_State* L);

    // Speed
    static int SetSpeed(lua_State* L);
    static int GetSpeed(lua_State* L);

    // Configuration
    static int SetAutoPlay(lua_State* L);
    static int GetAutoPlay(lua_State* L);
    static int SetLoopOverride(lua_State* L);
    static int GetLoopOverride(lua_State* L);
    static int SetDefaultAnimation(lua_State* L);
    static int GetDefaultAnimation(lua_State* L);

    // Asset registration
    static int AddAnimation(lua_State* L);            // (asset) or (path)

    // Runtime registration
    static int CreateAnimation(lua_State* L);         // (name) or (name, {textures})
    static int AddImage(lua_State* L);                // (name, texture) or (name, path)
    static int AddImages(lua_State* L);               // (name, {paths})
    static int RemoveAnimation(lua_State* L);
    static int HasAnimation(lua_State* L);

    // Output
    static int GetCurrentTexture(lua_State* L);
    static int GetCurrentUVScale(lua_State* L);       // returns (sx, sy)
    static int GetCurrentUVOffset(lua_State* L);      // returns (ox, oy)
    static int GetCurrentUVRect(lua_State* L);        // returns (u0, v0, u1, v1)
    static int GetCurrentAnimationName(lua_State* L);
    static int GetCurrentFrameIndex(lua_State* L);

    static void Bind();
};

#endif
