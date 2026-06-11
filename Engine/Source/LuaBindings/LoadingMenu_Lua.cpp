#include "LuaBindings/LoadingMenu_Lua.h"

#if LUA_ENABLED

int LoadingMenu_Lua::SetMenuScene(lua_State* L)
{
    const char* sceneName = luaL_checkstring(L, 1);
    GetLoadingMenu()->SetMenuScene(sceneName);
    return 0;
}

int LoadingMenu_Lua::GetMenuScene(lua_State* L)
{
    const std::string& name = GetLoadingMenu()->GetMenuScene();
    lua_pushstring(L, name.c_str());
    return 1;
}

int LoadingMenu_Lua::Open(lua_State* L)
{
    const char* targetSceneName = luaL_checkstring(L, 1);
    int32_t worldIndex = 0;
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
    {
        worldIndex = (int32_t)luaL_checkinteger(L, 2);
    }

    bool opened = GetLoadingMenu()->Open(targetSceneName, worldIndex);
    lua_pushboolean(L, opened);
    return 1;
}

int LoadingMenu_Lua::Close(lua_State* /*L*/)
{
    GetLoadingMenu()->Close();
    return 0;
}

int LoadingMenu_Lua::IsActive(lua_State* L)
{
    lua_pushboolean(L, GetLoadingMenu()->IsActive());
    return 1;
}

int LoadingMenu_Lua::GetState(lua_State* L)
{
    LoadingState s = GetLoadingMenu()->GetState();
    const char* name = "Idle";
    switch (s)
    {
    case LoadingState::Idle:    name = "Idle";    break;
    case LoadingState::Loading: name = "Loading"; break;
    case LoadingState::Closing: name = "Closing"; break;
    }
    lua_pushstring(L, name);
    return 1;
}

int LoadingMenu_Lua::GetTargetScene(lua_State* L)
{
    const std::string& name = GetLoadingMenu()->GetTargetScene();
    lua_pushstring(L, name.c_str());
    return 1;
}

void LoadingMenu_Lua::Bind()
{
    lua_State* L = GetLua();
    OCT_ASSERT(lua_gettop(L) == 0);

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    REGISTER_TABLE_FUNC(L, tableIdx, SetMenuScene);
    REGISTER_TABLE_FUNC(L, tableIdx, GetMenuScene);
    REGISTER_TABLE_FUNC(L, tableIdx, Open);
    REGISTER_TABLE_FUNC(L, tableIdx, Close);
    REGISTER_TABLE_FUNC(L, tableIdx, IsActive);
    REGISTER_TABLE_FUNC(L, tableIdx, GetState);
    REGISTER_TABLE_FUNC(L, tableIdx, GetTargetScene);

    lua_setglobal(L, LOADING_MENU_LUA_NAME);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
