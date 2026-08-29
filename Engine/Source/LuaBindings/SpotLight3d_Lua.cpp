#include "LuaBindings/SpotLight3d_Lua.h"
#include "LuaBindings/PointLight3d_Lua.h"
#include "LuaBindings/Vector_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

int SpotLight3D_Lua::SetInnerAngle(lua_State* L)
{
    SpotLight3D* comp = CHECK_SPOT_LIGHT_3D(L, 1);
    float value = CHECK_NUMBER(L, 2);

    comp->SetInnerAngle(value);

    return 0;
}

int SpotLight3D_Lua::GetInnerAngle(lua_State* L)
{
    SpotLight3D* comp = CHECK_SPOT_LIGHT_3D(L, 1);

    float ret = comp->GetInnerAngle();

    lua_pushnumber(L, ret);
    return 1;
}

int SpotLight3D_Lua::SetOuterAngle(lua_State* L)
{
    SpotLight3D* comp = CHECK_SPOT_LIGHT_3D(L, 1);
    float value = CHECK_NUMBER(L, 2);

    comp->SetOuterAngle(value);

    return 0;
}

int SpotLight3D_Lua::GetOuterAngle(lua_State* L)
{
    SpotLight3D* comp = CHECK_SPOT_LIGHT_3D(L, 1);

    float ret = comp->GetOuterAngle();

    lua_pushnumber(L, ret);
    return 1;
}

int SpotLight3D_Lua::GetDirection(lua_State* L)
{
    SpotLight3D* comp = CHECK_SPOT_LIGHT_3D(L, 1);

    glm::vec3 ret = comp->GetDirection();

    Vector_Lua::Create(L, ret);
    return 1;
}

int SpotLight3D_Lua::SetDirection(lua_State* L)
{
    SpotLight3D* comp = CHECK_SPOT_LIGHT_3D(L, 1);
    glm::vec3 value = CHECK_VECTOR(L, 2);

    comp->SetDirection(value);

    return 0;
}

void SpotLight3D_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        SPOT_LIGHT_3D_LUA_NAME,
        SPOT_LIGHT_3D_LUA_FLAG,
        POINT_LIGHT_3D_LUA_NAME);

    Node_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, SetInnerAngle);

    REGISTER_TABLE_FUNC(L, mtIndex, GetInnerAngle);

    REGISTER_TABLE_FUNC(L, mtIndex, SetOuterAngle);

    REGISTER_TABLE_FUNC(L, mtIndex, GetOuterAngle);

    REGISTER_TABLE_FUNC(L, mtIndex, GetDirection);

    REGISTER_TABLE_FUNC(L, mtIndex, SetDirection);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
