#include "LuaBindings/PlayerInput_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/LuaTypeCheck.h"
#include "Input/PlayerInputSystem.h"
#include "Input/InputActionsAsset.h"
#include "AssetManager.h"

#include "LuaBindings/LuaUtils.h"
#include "Assertion.h"

int PlayerInput_Lua::IsActive(lua_State* L)
{
    const char* category = luaL_checkstring(L, 1);
    const char* name = luaL_checkstring(L, 2);
    int32_t playerIndex = lua_isinteger(L, 3) ? (int32_t)lua_tointeger(L, 3) - 1 : -1;

    PlayerInputSystem* sys = PlayerInputSystem::Get();
    bool active = sys != nullptr && sys->IsActionActive(category, name, playerIndex);

    lua_pushboolean(L, active);
    return 1;
}

int PlayerInput_Lua::WasJustActivated(lua_State* L)
{
    const char* category = luaL_checkstring(L, 1);
    const char* name = luaL_checkstring(L, 2);
    int32_t playerIndex = lua_isinteger(L, 3) ? (int32_t)lua_tointeger(L, 3) - 1 : -1;

    PlayerInputSystem* sys = PlayerInputSystem::Get();
    bool activated = sys != nullptr && sys->WasActionJustActivated(category, name, playerIndex);

    lua_pushboolean(L, activated);
    return 1;
}

int PlayerInput_Lua::WasJustDeactivated(lua_State* L)
{
    const char* category = luaL_checkstring(L, 1);
    const char* name = luaL_checkstring(L, 2);
    int32_t playerIndex = lua_isinteger(L, 3) ? (int32_t)lua_tointeger(L, 3) - 1 : -1;

    PlayerInputSystem* sys = PlayerInputSystem::Get();
    bool deactivated = sys != nullptr && sys->WasActionJustDeactivated(category, name, playerIndex);

    lua_pushboolean(L, deactivated);
    return 1;
}

int PlayerInput_Lua::GetValue(lua_State* L)
{
    const char* category = luaL_checkstring(L, 1);
    const char* name = luaL_checkstring(L, 2);
    int32_t playerIndex = lua_isinteger(L, 3) ? (int32_t)lua_tointeger(L, 3) - 1 : -1;

    PlayerInputSystem* sys = PlayerInputSystem::Get();
    float value = sys != nullptr ? sys->GetActionValue(category, name, playerIndex) : 0.0f;

    lua_pushnumber(L, value);
    return 1;
}

int PlayerInput_Lua::GetPlayersConnected(lua_State* L)
{
    PlayerInputSystem* sys = PlayerInputSystem::Get();
    int32_t count = sys != nullptr ? sys->GetPlayersConnected() : 0;
    lua_pushinteger(L, count);
    return 1;
}

int PlayerInput_Lua::RegisterAction(lua_State* L)
{
    const char* category = luaL_checkstring(L, 1);
    const char* name = luaL_checkstring(L, 2);

    PlayerInputSystem* sys = PlayerInputSystem::Get();
    if (sys == nullptr)
    {
        luaL_error(L, "PlayerInputSystem not initialized");
        return 0;
    }

    // Parse optional table argument (3rd arg)
    TriggerMode mode = TriggerMode::SinglePress;
    float holdDuration = 1.0f;
    int32_t multiPressCount = 2;
    float multiPressWindow = 0.3f;

    if (lua_istable(L, 3))
    {
        // trigger mode
        lua_getfield(L, 3, "trigger");
        if (lua_isstring(L, -1))
        {
            const char* modeStr = lua_tostring(L, -1);
            if (strcmp(modeStr, "Hold") == 0) mode = TriggerMode::Hold;
            else if (strcmp(modeStr, "KeepHeld") == 0) mode = TriggerMode::KeepHeld;
            else if (strcmp(modeStr, "MultiPress") == 0) mode = TriggerMode::MultiPress;
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "holdDuration");
        if (lua_isnumber(L, -1)) holdDuration = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 3, "multiPressCount");
        if (lua_isinteger(L, -1)) multiPressCount = (int32_t)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 3, "multiPressWindow");
        if (lua_isnumber(L, -1)) multiPressWindow = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    sys->RegisterAction(category, name, mode);

    InputActionTrigger trigger;
    trigger.mode = mode;
    trigger.holdDuration = holdDuration;
    trigger.multiPressCount = multiPressCount;
    trigger.multiPressWindow = multiPressWindow;
    sys->SetTrigger(category, name, trigger);

    // Parse bindings array
    if (lua_istable(L, 3))
    {
        lua_getfield(L, 3, "bindings");
        if (lua_istable(L, -1))
        {
            int bindingsTable = lua_gettop(L);
            int bindingCount = (int)lua_rawlen(L, bindingsTable);

            for (int i = 1; i <= bindingCount; ++i)
            {
                lua_rawgeti(L, bindingsTable, i);
                if (lua_istable(L, -1))
                {
                    int bTable = lua_gettop(L);
                    InputActionBinding binding;

                    // type
                    lua_getfield(L, bTable, "type");
                    if (lua_isstring(L, -1))
                    {
                        const char* typeStr = lua_tostring(L, -1);
                        if (strcmp(typeStr, "Key") == 0) binding.sourceType = InputSourceType::Keyboard;
                        else if (strcmp(typeStr, "Mouse") == 0) binding.sourceType = InputSourceType::MouseButton;
                        else if (strcmp(typeStr, "Gamepad") == 0) binding.sourceType = InputSourceType::GamepadButton;
                        else if (strcmp(typeStr, "GamepadAxis") == 0) binding.sourceType = InputSourceType::GamepadAxis;
                        else if (strcmp(typeStr, "Pointer") == 0) binding.sourceType = InputSourceType::Pointer;
                    }
                    lua_pop(L, 1);

                    // code (key, button, or axis)
                    lua_getfield(L, bTable, "key");
                    if (lua_isinteger(L, -1)) binding.code = (int32_t)lua_tointeger(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, bTable, "button");
                    if (lua_isinteger(L, -1)) binding.code = (int32_t)lua_tointeger(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, bTable, "axis");
                    if (lua_isinteger(L, -1)) binding.code = (int32_t)lua_tointeger(L, -1);
                    lua_pop(L, 1);

                    // axis direction
                    lua_getfield(L, bTable, "axisDirection");
                    if (lua_isstring(L, -1))
                    {
                        const char* dir = lua_tostring(L, -1);
                        if (strcmp(dir, "Negative") == 0) binding.axisDirection = AxisDirection::Negative;
                        else if (strcmp(dir, "Full") == 0) binding.axisDirection = AxisDirection::Full;
                    }
                    lua_pop(L, 1);

                    lua_getfield(L, bTable, "axisThreshold");
                    if (lua_isnumber(L, -1)) binding.axisThreshold = (float)lua_tonumber(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, bTable, "gamepadIndex");
                    if (lua_isinteger(L, -1)) binding.gamepadIndex = (int32_t)lua_tointeger(L, -1);
                    lua_pop(L, 1);

                    // Modifiers
                    lua_getfield(L, bTable, "ctrl");
                    if (lua_isboolean(L, -1)) binding.requireCtrl = lua_toboolean(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, bTable, "shift");
                    if (lua_isboolean(L, -1)) binding.requireShift = lua_toboolean(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, bTable, "alt");
                    if (lua_isboolean(L, -1)) binding.requireAlt = lua_toboolean(L, -1);
                    lua_pop(L, 1);

                    sys->AddBinding(category, name, binding);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }

    return 0;
}

int PlayerInput_Lua::UnregisterAction(lua_State* L)
{
    const char* category = luaL_checkstring(L, 1);
    const char* name = luaL_checkstring(L, 2);

    PlayerInputSystem* sys = PlayerInputSystem::Get();
    if (sys != nullptr)
    {
        sys->UnregisterAction(category, name);
    }

    return 0;
}

int PlayerInput_Lua::SetEnabled(lua_State* L)
{
    bool enabled = lua_toboolean(L, 1);
    PlayerInputSystem* sys = PlayerInputSystem::Get();
    if (sys != nullptr) sys->SetEnabled(enabled);
    return 0;
}

int PlayerInput_Lua::IsEnabled(lua_State* L)
{
    PlayerInputSystem* sys = PlayerInputSystem::Get();
    lua_pushboolean(L, sys != nullptr && sys->IsEnabled());
    return 1;
}

int PlayerInput_Lua::LoadActions(lua_State* L)
{
    PlayerInputSystem* sys = PlayerInputSystem::Get();
    if (sys == nullptr)
    {
        luaL_error(L, "PlayerInputSystem not initialized");
        return 0;
    }

    // Accept either a string name or an asset reference
    Asset* asset = CHECK_ASSET(L, 1);
    if (asset == nullptr)
    {
        LogWarning("PlayerInput.LoadActions: Asset not found");
        lua_pushboolean(L, false);
        return 1;
    }

    InputActionsAsset* actionsAsset = (asset->GetType() == InputActionsAsset::GetStaticType())
        ? static_cast<InputActionsAsset*>(asset) : nullptr;
    if (actionsAsset == nullptr)
    {
        LogWarning("PlayerInput.LoadActions: Asset '%s' is not an InputActionsAsset", asset->GetName().c_str());
        lua_pushboolean(L, false);
        return 1;
    }

    // Clear existing actions first to prevent duplicate bindings on repeated loads
    const std::vector<InputAction>& existing = sys->GetActions();
    while (!existing.empty())
    {
        sys->UnregisterAction(existing[0].category, existing[0].name);
    }

    std::vector<InputAction>& actions = actionsAsset->mActions;
    for (const InputAction& action : actions)
    {
        sys->RegisterAction(action.category, action.name, action.trigger.mode);
        sys->SetTrigger(action.category, action.name, action.trigger);
        for (const InputActionBinding& binding : action.bindings)
        {
            sys->AddBinding(action.category, action.name, binding);
        }
    }

    LogDebug("PlayerInput.LoadActions: Loaded %d actions from '%s'", (int)actions.size(), asset->GetName().c_str());
    lua_pushboolean(L, true);
    return 1;
}

void PlayerInput_Lua::Bind()
{
    lua_State* L = GetLua();
    OCT_ASSERT(lua_gettop(L) == 0);

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    REGISTER_TABLE_FUNC(L, tableIdx, IsActive);
    REGISTER_TABLE_FUNC(L, tableIdx, WasJustActivated);
    REGISTER_TABLE_FUNC(L, tableIdx, WasJustDeactivated);
    REGISTER_TABLE_FUNC(L, tableIdx, GetValue);
    REGISTER_TABLE_FUNC(L, tableIdx, RegisterAction);
    REGISTER_TABLE_FUNC(L, tableIdx, UnregisterAction);
    REGISTER_TABLE_FUNC(L, tableIdx, GetPlayersConnected);
    REGISTER_TABLE_FUNC(L, tableIdx, SetEnabled);
    REGISTER_TABLE_FUNC(L, tableIdx, IsEnabled);
    REGISTER_TABLE_FUNC(L, tableIdx, LoadActions);

    lua_setglobal(L, PLAYER_INPUT_LUA_NAME);
    OCT_ASSERT(lua_gettop(L) == 0);
}
