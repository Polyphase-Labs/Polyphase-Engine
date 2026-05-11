#pragma once

#include "EngineTypes.h"

#if LUA_ENABLED

#define SIGNAL_BUS_LUA_NAME "SignalBus"

struct SignalBus_Lua
{
    static int Subscribe(lua_State* L);
    static int Unsubscribe(lua_State* L);
    static int Emit(lua_State* L);
    static int Clear(lua_State* L);

    static void Bind();
};

#endif
