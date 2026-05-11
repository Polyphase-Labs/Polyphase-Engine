#include "LuaBindings/SignalBus_Lua.h"

#include "SignalBus.h"

#include "LuaBindings/LuaUtils.h"
#include "LuaBindings/Node_Lua.h"

#include "Utilities.h"

#if LUA_ENABLED

int SignalBus_Lua::Subscribe(lua_State* L)
{
    const char* signalName = CHECK_STRING(L, 1);
    Node* listenerNode = CHECK_NODE(L, 2);
    CHECK_FUNCTION(L, 3);

    ScriptFunc listenerFunc(L, 3);
    GetSignalBus()->Subscribe(signalName, listenerNode, listenerFunc);

    return 0;
}

int SignalBus_Lua::Unsubscribe(lua_State* L)
{
    const char* signalName = CHECK_STRING(L, 1);
    Node* listenerNode = CHECK_NODE(L, 2);

    GetSignalBus()->Unsubscribe(signalName, listenerNode);

    return 0;
}

int SignalBus_Lua::Emit(lua_State* L)
{
    const char* signalName = CHECK_STRING(L, 1);

    std::vector<Datum> args;
    int numArgs = lua_gettop(L) - 1;

    if (numArgs > 0)
    {
        args.reserve(numArgs);
    }

    for (int32_t i = 2; i <= 1 + numArgs; ++i)
    {
        args.push_back(LuaObjectToDatum(L, i));
    }

    std::vector<Datum> retVals = GetSignalBus()->Emit(signalName, args);
    for (uint32_t i = 0; i < retVals.size(); ++i)
    {
        LuaPushDatum(L, retVals[i]);
    }

    return (int)retVals.size();
}

int SignalBus_Lua::Clear(lua_State* L)
{
    GetSignalBus()->Clear();
    return 0;
}

void SignalBus_Lua::Bind()
{
    lua_State* L = GetLua();

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    REGISTER_TABLE_FUNC(L, tableIdx, Subscribe);
    REGISTER_TABLE_FUNC(L, tableIdx, Unsubscribe);
    REGISTER_TABLE_FUNC(L, tableIdx, Emit);
    REGISTER_TABLE_FUNC(L, tableIdx, Clear);

    lua_setglobal(L, SIGNAL_BUS_LUA_NAME);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
