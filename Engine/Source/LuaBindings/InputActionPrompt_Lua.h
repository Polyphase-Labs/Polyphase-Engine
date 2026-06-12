#pragma once

#include "EngineTypes.h"
#include "Log.h"

#include "Nodes/Widgets/InputActionPrompt.h"

#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

#define INPUT_ACTION_PROMPT_LUA_NAME "InputActionPrompt"
#define INPUT_ACTION_PROMPT_LUA_FLAG "cfInputActionPrompt"
#define CHECK_INPUT_ACTION_PROMPT(L, arg) \
    (InputActionPrompt*)CheckNodeLuaType(L, arg, INPUT_ACTION_PROMPT_LUA_NAME, INPUT_ACTION_PROMPT_LUA_FLAG);

struct InputActionPrompt_Lua
{
    static int SetAction(lua_State* L);
    static int GetActionCategory(lua_State* L);
    static int GetActionName(lua_State* L);
    static int SetPromptMap(lua_State* L);
    static int SetPromptStyle(lua_State* L);
    static int GetResolvedLabel(lua_State* L);

    static void Bind();
};

#endif
