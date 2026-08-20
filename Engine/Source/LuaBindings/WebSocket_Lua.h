#pragma once

#include "PolyphaseAPI.h"
#include "EngineTypes.h"

#if LUA_ENABLED

class POLYPHASE_API WebSocket_Lua
{
public:
    static void Bind();

    // WebSocket.* table
    static int Connect(lua_State* L);
    static int IsAvailable(lua_State* L);
    static int GetMissingDependencyMessage(lua_State* L);

    // Connection:* callbacks
    static int Connection_SetOpenCallback(lua_State* L);
    static int Connection_SetMessageCallback(lua_State* L);
    static int Connection_SetErrorCallback(lua_State* L);
    static int Connection_SetClosedCallback(lua_State* L);

    // Connection:* actions
    static int Connection_SendText(lua_State* L);
    static int Connection_SendBinary(lua_State* L);
    static int Connection_Close(lua_State* L);

    // Connection:* polling
    static int Connection_GetState(lua_State* L);
    static int Connection_GetAvailablePacketCount(lua_State* L);
    static int Connection_GetPacket(lua_State* L);
    static int Connection_GetSelectedProtocol(lua_State* L);
    static int Connection_GetCloseCode(lua_State* L);
    static int Connection_GetCloseReason(lua_State* L);
    static int Connection_WasDowngraded(lua_State* L);

    static int Connection_Gc(lua_State* L);
};

#endif
