#pragma once

#include "EngineTypes.h"
#include "Log.h"
#include "Engine.h"

#include "Nodes/3D/SpotLight3d.h"

#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define SPOT_LIGHT_3D_LUA_NAME "SpotLight3D"
#define SPOT_LIGHT_3D_LUA_FLAG "cfSpotLight3D"
#define CHECK_SPOT_LIGHT_3D(L, arg) static_cast<SpotLight3D*>(CheckNodeLuaType(L, arg, SPOT_LIGHT_3D_LUA_NAME, SPOT_LIGHT_3D_LUA_FLAG));

struct SpotLight3D_Lua
{
    static int SetInnerAngle(lua_State* L);
    static int GetInnerAngle(lua_State* L);
    static int SetOuterAngle(lua_State* L);
    static int GetOuterAngle(lua_State* L);
    static int GetDirection(lua_State* L);
    static int SetDirection(lua_State* L);

    static void Bind();
};

#endif
