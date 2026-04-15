#pragma once

#include "SerialManager.h"
#include "EngineTypes.h"

#if LUA_ENABLED

#define SERIAL_LUA_NAME "Serial"

struct Serial_Lua
{
    static int EnumeratePorts(lua_State* L);
    static int Connect(lua_State* L);
    static int Disconnect(lua_State* L);
    static int IsConnected(lua_State* L);
    static int Send(lua_State* L);
    static int StartReceive(lua_State* L);
    static int StopReceive(lua_State* L);
    static int IsReceiving(lua_State* L);

    static int SetMessageCallback(lua_State* L);
    static int SetConnectCallback(lua_State* L);
    static int SetDisconnectCallback(lua_State* L);

    static void Bind();
};

#endif
