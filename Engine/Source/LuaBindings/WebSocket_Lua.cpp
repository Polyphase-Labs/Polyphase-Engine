#include "LuaBindings/WebSocket_Lua.h"

#if LUA_ENABLED

#include "LuaBindings/LuaUtils.h"
#include "Network/WebSocketClient.h"
#include "Log.h"
#include "Engine.h"

#include <memory>

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}

static const char* kConnectionMt = "Polyphase.WsConnection";

namespace
{
    std::shared_ptr<WsConnection>* CheckConnection(lua_State* L, int idx)
    {
        return static_cast<std::shared_ptr<WsConnection>*>(luaL_checkudata(L, idx, kConnectionMt));
    }

    void PushConnection(lua_State* L, std::shared_ptr<WsConnection> conn)
    {
        void* mem = lua_newuserdata(L, sizeof(std::shared_ptr<WsConnection>));
        new (mem) std::shared_ptr<WsConnection>(std::move(conn));
        luaL_setmetatable(L, kConnectionMt);
    }

    // Wraps a Lua function ref so it can be called later from WebSocket::Tick.
    // Holds a ref in LUA_REGISTRYINDEX; releases it on destruction, which
    // always runs on the main thread (the connection is owned by the userdata,
    // so the release happens from __gc).
    struct LuaCallbackRef
    {
        lua_State* L = nullptr;
        int        ref = LUA_REFNIL;

        explicit LuaCallbackRef(lua_State* state, int stackIdx) : L(state)
        {
            lua_pushvalue(L, stackIdx);
            ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        ~LuaCallbackRef()
        {
            if (ref != LUA_REFNIL && L != nullptr)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, ref);
            }
        }
        LuaCallbackRef(const LuaCallbackRef&) = delete;
        LuaCallbackRef& operator=(const LuaCallbackRef&) = delete;
    };

    // Pushes the callback and returns true when it is ready to be called with
    // lua_pcall. Leaves nothing on the stack when it returns false.
    bool PushCallback(const std::shared_ptr<LuaCallbackRef>& cbRef)
    {
        if (cbRef == nullptr || cbRef->L == nullptr || cbRef->ref == LUA_REFNIL)
        {
            return false;
        }

        lua_rawgeti(cbRef->L, LUA_REGISTRYINDEX, cbRef->ref);
        if (!lua_isfunction(cbRef->L, -1))
        {
            lua_pop(cbRef->L, 1);
            return false;
        }

        return true;
    }

    void CallCallback(lua_State* L, int argCount)
    {
        const int rc = lua_pcall(L, argCount, 0, 0);
        if (rc != 0)
        {
            const char* err = lua_tostring(L, -1);
            LogError("WebSocket callback error: %s", err != nullptr ? err : "(unknown)");
            lua_pop(L, 1);
        }
    }

    std::shared_ptr<LuaCallbackRef> MakeCallbackRef(lua_State* L, int idx)
    {
        if (lua_isnoneornil(L, idx))
        {
            return nullptr;
        }

        if (!lua_isfunction(L, idx))
        {
            luaL_error(L, "WebSocket callback must be a function");
            return nullptr;
        }

        return std::make_shared<LuaCallbackRef>(L, idx);
    }

    void ReadOptions(lua_State* L, int idx, WsConnectOptions& outOptions)
    {
        if (lua_isnoneornil(L, idx))
        {
            return;
        }

        luaL_checktype(L, idx, LUA_TTABLE);

        lua_getfield(L, idx, "protocols");
        if (lua_istable(L, -1))
        {
            const int count = (int)lua_rawlen(L, -1);
            for (int i = 1; i <= count; ++i)
            {
                lua_rawgeti(L, -1, i);
                if (lua_isstring(L, -1))
                {
                    outOptions.mProtocols.push_back(lua_tostring(L, -1));
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);

        lua_getfield(L, idx, "headers");
        if (lua_istable(L, -1))
        {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                // Only string keys - a header name has to be a name.
                if (lua_type(L, -2) == LUA_TSTRING && lua_isstring(L, -1))
                {
                    outOptions.mHeaders.push_back(
                        std::make_pair(std::string(lua_tostring(L, -2)), std::string(lua_tostring(L, -1))));
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);

        lua_getfield(L, idx, "maxQueuedBytes");
        if (lua_isnumber(L, -1))
        {
            const lua_Integer bytes = lua_tointeger(L, -1);
            if (bytes > 0)
            {
                outOptions.mMaxQueuedBytes = (uint32_t)bytes;
            }
        }
        lua_pop(L, 1);
    }

}

// Kept at file scope, unindented, matching Misc_Lua.cpp's enum-table shape -
// Tools/generate_lua_stubs.py only sees top-level function bodies.
void BindWebSocketState()
{
    lua_State* L = GetLua();
    OCT_ASSERT(lua_gettop(L) == 0);

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    lua_pushinteger(L, (int)WsState::Connecting);
    lua_setfield(L, tableIdx, "Connecting");

    lua_pushinteger(L, (int)WsState::Open);
    lua_setfield(L, tableIdx, "Open");

    lua_pushinteger(L, (int)WsState::Closing);
    lua_setfield(L, tableIdx, "Closing");

    lua_pushinteger(L, (int)WsState::Closed);
    lua_setfield(L, tableIdx, "Closed");

    lua_setglobal(L, "WebSocketState");

    OCT_ASSERT(lua_gettop(L) == 0);
}

// -- WebSocket.* -----------------------------------------------------------

int WebSocket_Lua::Connect(lua_State* L)
{
    WsConnectOptions options;
    options.mUrl = luaL_checkstring(L, 1);
    ReadOptions(L, 2, options);

    std::string error;
    std::shared_ptr<WsConnection> conn = WebSocket::Connect(options, error);

    if (conn == nullptr)
    {
        lua_pushnil(L);
        lua_pushstring(L, error.empty() ? "WebSocket connect failed" : error.c_str());
        return 2;
    }

    PushConnection(L, std::move(conn));
    return 1;
}

int WebSocket_Lua::IsAvailable(lua_State* L)
{
    lua_pushboolean(L, WebSocket::IsAvailable() ? 1 : 0);
    return 1;
}

int WebSocket_Lua::GetMissingDependencyMessage(lua_State* L)
{
    lua_pushstring(L, WebSocket::GetMissingDependencyMessage());
    return 1;
}

// -- Connection:* callbacks ------------------------------------------------

int WebSocket_Lua::Connection_SetOpenCallback(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    std::shared_ptr<LuaCallbackRef> cbRef = MakeCallbackRef(L, 2);

    if (cbRef == nullptr)
    {
        (*conn)->SetOpenCallback(WsOpenCallback());
        return 0;
    }

    (*conn)->SetOpenCallback([cbRef]()
    {
        if (!PushCallback(cbRef)) return;
        CallCallback(cbRef->L, 0);
    });
    return 0;
}

int WebSocket_Lua::Connection_SetMessageCallback(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    std::shared_ptr<LuaCallbackRef> cbRef = MakeCallbackRef(L, 2);

    if (cbRef == nullptr)
    {
        (*conn)->SetMessageCallback(WsMessageCallback());
        return 0;
    }

    (*conn)->SetMessageCallback([cbRef](const uint8_t* data, uint32_t size, bool binary)
    {
        if (!PushCallback(cbRef)) return;
        lua_State* L = cbRef->L;
        lua_pushlstring(L, (const char*)data, size);
        lua_pushboolean(L, binary ? 1 : 0);
        CallCallback(L, 2);
    });
    return 0;
}

int WebSocket_Lua::Connection_SetErrorCallback(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    std::shared_ptr<LuaCallbackRef> cbRef = MakeCallbackRef(L, 2);

    if (cbRef == nullptr)
    {
        (*conn)->SetErrorCallback(WsErrorCallback());
        return 0;
    }

    (*conn)->SetErrorCallback([cbRef](const char* message)
    {
        if (!PushCallback(cbRef)) return;
        lua_State* L = cbRef->L;
        lua_pushstring(L, message != nullptr ? message : "");
        CallCallback(L, 1);
    });
    return 0;
}

int WebSocket_Lua::Connection_SetClosedCallback(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    std::shared_ptr<LuaCallbackRef> cbRef = MakeCallbackRef(L, 2);

    if (cbRef == nullptr)
    {
        (*conn)->SetClosedCallback(WsClosedCallback());
        return 0;
    }

    (*conn)->SetClosedCallback([cbRef](uint16_t code, const char* reason, bool wasClean)
    {
        if (!PushCallback(cbRef)) return;
        lua_State* L = cbRef->L;
        lua_pushinteger(L, code);
        lua_pushstring(L, reason != nullptr ? reason : "");
        lua_pushboolean(L, wasClean ? 1 : 0);
        CallCallback(L, 3);
    });
    return 0;
}

// -- Connection:* actions --------------------------------------------------

int WebSocket_Lua::Connection_SendText(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    size_t len = 0;
    const char* data = luaL_checklstring(L, 2, &len);
    lua_pushboolean(L, (*conn)->SendText(data, (uint32_t)len) ? 1 : 0);
    return 1;
}

int WebSocket_Lua::Connection_SendBinary(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    size_t len = 0;
    const char* data = luaL_checklstring(L, 2, &len);
    lua_pushboolean(L, (*conn)->SendBinary((const uint8_t*)data, (uint32_t)len) ? 1 : 0);
    return 1;
}

int WebSocket_Lua::Connection_Close(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    const uint16_t code = lua_isnoneornil(L, 2) ? uint16_t(WS_CLOSE_NORMAL) : (uint16_t)luaL_checkinteger(L, 2);
    const char* reason = lua_isnoneornil(L, 3) ? "" : luaL_checkstring(L, 3);
    (*conn)->Close(code, reason);
    return 0;
}

// -- Connection:* polling --------------------------------------------------

int WebSocket_Lua::Connection_GetState(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    lua_pushinteger(L, (int)(*conn)->GetState());
    return 1;
}

int WebSocket_Lua::Connection_GetAvailablePacketCount(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    lua_pushinteger(L, (lua_Integer)(*conn)->GetAvailablePacketCount());
    return 1;
}

int WebSocket_Lua::Connection_GetPacket(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);

    WsMessage message;
    if (!(*conn)->TakePacket(message))
    {
        lua_pushnil(L);
        return 1;
    }

    lua_pushlstring(L, (const char*)message.mData.data(), message.mData.size());
    lua_pushboolean(L, message.mBinary ? 1 : 0);
    return 2;
}

int WebSocket_Lua::Connection_GetSelectedProtocol(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    lua_pushstring(L, (*conn)->GetSelectedProtocol().c_str());
    return 1;
}

int WebSocket_Lua::Connection_GetCloseCode(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    lua_pushinteger(L, (*conn)->GetCloseCode());
    return 1;
}

int WebSocket_Lua::Connection_GetCloseReason(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    lua_pushstring(L, (*conn)->GetCloseReason().c_str());
    return 1;
}

int WebSocket_Lua::Connection_WasDowngraded(lua_State* L)
{
    auto* conn = CheckConnection(L, 1);
    lua_pushboolean(L, (*conn)->WasDowngraded() ? 1 : 0);
    return 1;
}

int WebSocket_Lua::Connection_Gc(lua_State* L)
{
    auto* conn = static_cast<std::shared_ptr<WsConnection>*>(luaL_checkudata(L, 1, kConnectionMt));

    if (*conn != nullptr)
    {
        // Dropping the handle closes the connection, matching Godot's
        // WebSocketPeer. Clear the callbacks first: they hold registry refs
        // and Close() must not try to call back into a Lua state that is
        // already collecting this userdata.
        (*conn)->SetOpenCallback(WsOpenCallback());
        (*conn)->SetMessageCallback(WsMessageCallback());
        (*conn)->SetErrorCallback(WsErrorCallback());
        (*conn)->SetClosedCallback(WsClosedCallback());
        (*conn)->Close(WS_CLOSE_GOING_AWAY, "");
    }

    using Sp = std::shared_ptr<WsConnection>;
    conn->~Sp();
    return 0;
}

// -- Bind ------------------------------------------------------------------

void WebSocket_Lua::Bind()
{
    lua_State* L = GetLua();

    // ---- Connection metatable ----
    luaL_newmetatable(L, kConnectionMt);
    int mtConnection = lua_gettop(L);

    lua_pushvalue(L, mtConnection);
    lua_setfield(L, mtConnection, "__index");

    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_SetOpenCallback,          "SetOpenCallback");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_SetMessageCallback,       "SetMessageCallback");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_SetErrorCallback,         "SetErrorCallback");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_SetClosedCallback,        "SetClosedCallback");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_SendText,                 "SendText");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_SendBinary,               "SendBinary");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_Close,                    "Close");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_GetState,                 "GetState");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_GetAvailablePacketCount,  "GetAvailablePacketCount");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_GetPacket,                "GetPacket");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_GetSelectedProtocol,      "GetSelectedProtocol");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_GetCloseCode,             "GetCloseCode");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_GetCloseReason,           "GetCloseReason");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_WasDowngraded,            "WasDowngraded");
    REGISTER_TABLE_FUNC_EX(L, mtConnection, WebSocket_Lua::Connection_Gc,                       "__gc");

    lua_pop(L, 1);

    // ---- WebSocket table ----
    lua_newtable(L);
    int tWebSocket = lua_gettop(L);

    REGISTER_TABLE_FUNC_EX(L, tWebSocket, WebSocket_Lua::Connect,                        "Connect");
    REGISTER_TABLE_FUNC_EX(L, tWebSocket, WebSocket_Lua::IsAvailable,                    "IsAvailable");
    REGISTER_TABLE_FUNC_EX(L, tWebSocket, WebSocket_Lua::GetMissingDependencyMessage,    "GetMissingDependencyMessage");

    lua_setglobal(L, "WebSocket");

    OCT_ASSERT(lua_gettop(L) == 0);

    BindWebSocketState();
}

#endif
