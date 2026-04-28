#pragma once

#include "Engine.h"
#include "LuaBindings/LuaUtils.h"

#define PLAYER_INPUT_LUA_NAME "PlayerInput"

struct PlayerInput_Lua
{
    static int IsActive(lua_State* L);
    static int WasJustActivated(lua_State* L);
    static int WasJustDeactivated(lua_State* L);
    static int GetValue(lua_State* L);

    static int RegisterAction(lua_State* L);
    static int UnregisterAction(lua_State* L);

    static int GetPlayersConnected(lua_State* L);

    static int SetEnabled(lua_State* L);
    static int IsEnabled(lua_State* L);
    static int LoadActions(lua_State* L);

    static void Bind();
};
