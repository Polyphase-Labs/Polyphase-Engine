#include "LuaBindings/InputActionPrompt_Lua.h"
#include "LuaBindings/Widget_Lua.h"
#include "LuaBindings/Asset_Lua.h"

#include "Assets/InputPromptMap.h"
#include "Assets/InputPromptStyle.h"

#if LUA_ENABLED

int InputActionPrompt_Lua::SetAction(lua_State* L)
{
    InputActionPrompt* w = CHECK_INPUT_ACTION_PROMPT(L, 1);
    const char* category = CHECK_STRING(L, 2);
    const char* name = CHECK_STRING(L, 3);
    w->SetActionCategory(category);
    w->SetActionName(name);
    return 0;
}

int InputActionPrompt_Lua::GetActionCategory(lua_State* L)
{
    InputActionPrompt* w = CHECK_INPUT_ACTION_PROMPT(L, 1);
    lua_pushstring(L, w->GetActionCategory().c_str());
    return 1;
}

int InputActionPrompt_Lua::GetActionName(lua_State* L)
{
    InputActionPrompt* w = CHECK_INPUT_ACTION_PROMPT(L, 1);
    lua_pushstring(L, w->GetActionName().c_str());
    return 1;
}

int InputActionPrompt_Lua::SetPromptMap(lua_State* L)
{
    InputActionPrompt* w = CHECK_INPUT_ACTION_PROMPT(L, 1);
    Asset* asset = lua_isnil(L, 2) ? nullptr : CHECK_ASSET(L, 2);
    InputPromptMap* map = nullptr;
    if (asset && asset->GetType() == InputPromptMap::GetStaticType())
        map = static_cast<InputPromptMap*>(asset);
    w->SetPromptMap(map);
    return 0;
}

int InputActionPrompt_Lua::SetPromptStyle(lua_State* L)
{
    InputActionPrompt* w = CHECK_INPUT_ACTION_PROMPT(L, 1);
    Asset* asset = lua_isnil(L, 2) ? nullptr : CHECK_ASSET(L, 2);
    InputPromptStyle* style = nullptr;
    if (asset && asset->GetType() == InputPromptStyle::GetStaticType())
        style = static_cast<InputPromptStyle*>(asset);
    w->SetPromptStyle(style);
    return 0;
}

int InputActionPrompt_Lua::GetResolvedLabel(lua_State* L)
{
    InputActionPrompt* w = CHECK_INPUT_ACTION_PROMPT(L, 1);
    lua_pushstring(L, w->GetResolvedLabel().c_str());
    return 1;
}

void InputActionPrompt_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        INPUT_ACTION_PROMPT_LUA_NAME,
        INPUT_ACTION_PROMPT_LUA_FLAG,
        WIDGET_LUA_NAME);

    Node_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, SetAction);
    REGISTER_TABLE_FUNC(L, mtIndex, GetActionCategory);
    REGISTER_TABLE_FUNC(L, mtIndex, GetActionName);
    REGISTER_TABLE_FUNC(L, mtIndex, SetPromptMap);
    REGISTER_TABLE_FUNC(L, mtIndex, SetPromptStyle);
    REGISTER_TABLE_FUNC(L, mtIndex, GetResolvedLabel);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
