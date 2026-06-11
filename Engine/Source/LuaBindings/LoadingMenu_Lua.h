#pragma once

#include "Engine.h"
#include "LoadingMenu.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define LOADING_MENU_LUA_NAME "LoadingMenu"

struct LoadingMenu_Lua
{
    static int SetMenuScene(lua_State* L);
    static int GetMenuScene(lua_State* L);
    static int Open(lua_State* L);
    static int Close(lua_State* L);
    static int IsActive(lua_State* L);
    static int GetState(lua_State* L);
    static int GetTargetScene(lua_State* L);

    static void Bind();
};

#endif
