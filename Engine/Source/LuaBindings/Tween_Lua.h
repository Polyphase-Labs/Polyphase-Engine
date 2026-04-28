#pragma once

#include "EngineTypes.h"

#if LUA_ENABLED

#define TWEEN_LUA_NAME "Tween"

struct Tween_Lua
{
    // Core
    static int Value(lua_State* L);
    static int Vector(lua_State* L);
    static int Quaternion(lua_State* L);

    // Node helpers
    static int Position(lua_State* L);
    static int Rotation(lua_State* L);
    static int Scale(lua_State* L);
    static int Color(lua_State* L);

    // Control
    static int Cancel(lua_State* L);
    static int CancelAll(lua_State* L);
    static int Pause(lua_State* L);
    static int Resume(lua_State* L);

    static void Bind();
};

#endif
