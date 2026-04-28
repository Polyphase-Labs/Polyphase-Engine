#include "LuaBindings/Serial_Lua.h"
#include "LuaBindings/LuaUtils.h"

#include "SerialManager.h"
#include "Engine.h"

#if LUA_ENABLED

static SerialConfig ReadSerialConfigTable(lua_State* L, int tableIdx)
{
    SerialConfig cfg;
    if (tableIdx < 0)
        tableIdx = lua_gettop(L) + tableIdx + 1;

    if (lua_istable(L, tableIdx))
    {
        lua_getfield(L, tableIdx, "baud");
        if (lua_isnumber(L, -1)) cfg.mBaudRate = (uint32_t)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, tableIdx, "dataBits");
        if (lua_isnumber(L, -1)) cfg.mDataBits = (uint8_t)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, tableIdx, "stopBits");
        if (lua_isnumber(L, -1)) cfg.mStopBits = (uint8_t)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, tableIdx, "parity");
        if (lua_isstring(L, -1))
        {
            const char* p = lua_tostring(L, -1);
            if      (strcmp(p, "odd")   == 0) cfg.mParity = SerialParity::Odd;
            else if (strcmp(p, "even")  == 0) cfg.mParity = SerialParity::Even;
            else if (strcmp(p, "mark")  == 0) cfg.mParity = SerialParity::Mark;
            else if (strcmp(p, "space") == 0) cfg.mParity = SerialParity::Space;
            else                              cfg.mParity = SerialParity::None;
        }
        lua_pop(L, 1);

        lua_getfield(L, tableIdx, "flowControl");
        if (lua_isboolean(L, -1)) cfg.mFlowControl = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
    }
    return cfg;
}

int Serial_Lua::EnumeratePorts(lua_State* L)
{
    std::vector<SerialPortInfo> ports = SerialManager::Get()->EnumeratePorts();

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    for (uint32_t i = 0; i < ports.size(); ++i)
    {
        lua_newtable(L);
        int entryIdx = lua_gettop(L);

        lua_pushstring(L, ports[i].mPortName.c_str());
        lua_setfield(L, entryIdx, "name");

        lua_pushstring(L, ports[i].mDescription.c_str());
        lua_setfield(L, entryIdx, "description");

        lua_rawseti(L, tableIdx, (int)(i + 1));
    }

    return 1;
}

int Serial_Lua::Connect(lua_State* L)
{
    const char* portName = CHECK_STRING(L, 1);

    SerialConfig cfg;
    if (!lua_isnone(L, 2))
    {
        cfg = ReadSerialConfigTable(L, 2);
    }

    SerialHandle h = SerialManager::Get()->Connect(portName, cfg);
    lua_pushinteger(L, (lua_Integer)h);
    return 1;
}

int Serial_Lua::Disconnect(lua_State* L)
{
    SerialHandle h = (SerialHandle)CHECK_INTEGER(L, 1);
    SerialManager::Get()->Disconnect(h);
    return 0;
}

int Serial_Lua::IsConnected(lua_State* L)
{
    SerialHandle h = (SerialHandle)CHECK_INTEGER(L, 1);
    lua_pushboolean(L, SerialManager::Get()->IsConnected(h) ? 1 : 0);
    return 1;
}

int Serial_Lua::Send(lua_State* L)
{
    SerialHandle h = (SerialHandle)CHECK_INTEGER(L, 1);

    size_t len = 0;
    const char* data = luaL_checklstring(L, 2, &len);

    int32_t written = SerialManager::Get()->SendMessage(
        h,
        reinterpret_cast<const uint8_t*>(data),
        (uint32_t)len);

    lua_pushinteger(L, (lua_Integer)written);
    return 1;
}

int Serial_Lua::SendLine(lua_State* L)
{
    SerialHandle h = (SerialHandle)CHECK_INTEGER(L, 1);

    size_t len = 0;
    const char* data = luaL_checklstring(L, 2, &len);

    std::string line(data, len);
    line += '\n';

    int32_t written = SerialManager::Get()->SendMessage(
        h,
        reinterpret_cast<const uint8_t*>(line.data()),
        (uint32_t)line.size());

    lua_pushinteger(L, (lua_Integer)written);
    return 1;
}

int Serial_Lua::StartReceive(lua_State* L)
{
    SerialHandle h = (SerialHandle)CHECK_INTEGER(L, 1);
    SerialManager::Get()->StartReceive(h);
    return 0;
}

int Serial_Lua::StopReceive(lua_State* L)
{
    SerialHandle h = (SerialHandle)CHECK_INTEGER(L, 1);
    SerialManager::Get()->StopReceive(h);
    return 0;
}

int Serial_Lua::IsReceiving(lua_State* L)
{
    SerialHandle h = (SerialHandle)CHECK_INTEGER(L, 1);
    lua_pushboolean(L, SerialManager::Get()->IsReceiving(h) ? 1 : 0);
    return 1;
}

int Serial_Lua::RegisterMessageFunction(lua_State* L)
{
    SerialHandle h = (SerialHandle)CHECK_INTEGER(L, 1);
    const char* pattern = CHECK_STRING(L, 2);
    CHECK_FUNCTION(L, 3);
    ScriptFunc func(L, 3);

    uint32_t id = SerialManager::Get()->RegisterMessageMatcher(
        h, pattern, SerialMessageMatcher::Type::Exact, func);

    lua_pushinteger(L, (lua_Integer)id);
    return 1;
}

int Serial_Lua::RegisterREGEXMessageFunction(lua_State* L)
{
    SerialHandle h = (SerialHandle)CHECK_INTEGER(L, 1);
    const char* pattern = CHECK_STRING(L, 2);
    CHECK_FUNCTION(L, 3);
    ScriptFunc func(L, 3);

    uint32_t id = SerialManager::Get()->RegisterMessageMatcher(
        h, pattern, SerialMessageMatcher::Type::Regex, func);

    lua_pushinteger(L, (lua_Integer)id);
    return 1;
}

int Serial_Lua::UnregisterMessageFunction(lua_State* L)
{
    SerialHandle h = (SerialHandle)CHECK_INTEGER(L, 1);
    uint32_t matcherId = (uint32_t)CHECK_INTEGER(L, 2);
    SerialManager::Get()->UnregisterMessageMatcher(h, matcherId);
    return 0;
}

int Serial_Lua::SetMessageCallback(lua_State* L)
{
    CHECK_FUNCTION(L, 1);
    ScriptFunc func(L, 1);
    SerialManager::Get()->SetScriptMessageCallback(func);
    return 0;
}

int Serial_Lua::SetConnectCallback(lua_State* L)
{
    CHECK_FUNCTION(L, 1);
    ScriptFunc func(L, 1);
    SerialManager::Get()->SetScriptConnectCallback(func);
    return 0;
}

int Serial_Lua::SetDisconnectCallback(lua_State* L)
{
    CHECK_FUNCTION(L, 1);
    ScriptFunc func(L, 1);
    SerialManager::Get()->SetScriptDisconnectCallback(func);
    return 0;
}

void Serial_Lua::Bind()
{
    lua_State* L = GetLua();

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    REGISTER_TABLE_FUNC(L, tableIdx, EnumeratePorts);
    REGISTER_TABLE_FUNC(L, tableIdx, Connect);
    REGISTER_TABLE_FUNC(L, tableIdx, Disconnect);
    REGISTER_TABLE_FUNC(L, tableIdx, IsConnected);
    REGISTER_TABLE_FUNC(L, tableIdx, Send);
    REGISTER_TABLE_FUNC(L, tableIdx, SendLine);
    REGISTER_TABLE_FUNC(L, tableIdx, StartReceive);
    REGISTER_TABLE_FUNC(L, tableIdx, StopReceive);
    REGISTER_TABLE_FUNC(L, tableIdx, IsReceiving);
    REGISTER_TABLE_FUNC(L, tableIdx, RegisterMessageFunction);
    REGISTER_TABLE_FUNC(L, tableIdx, RegisterREGEXMessageFunction);
    REGISTER_TABLE_FUNC(L, tableIdx, UnregisterMessageFunction);
    REGISTER_TABLE_FUNC(L, tableIdx, SetMessageCallback);
    REGISTER_TABLE_FUNC(L, tableIdx, SetConnectCallback);
    REGISTER_TABLE_FUNC(L, tableIdx, SetDisconnectCallback);

    lua_setglobal(L, SERIAL_LUA_NAME);

    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
