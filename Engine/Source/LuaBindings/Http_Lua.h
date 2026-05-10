#pragma once

#include "PolyphaseAPI.h"
#include "EngineTypes.h"

#if LUA_ENABLED

class POLYPHASE_API Http_Lua
{
public:
    static void Bind();

    // Http.* table
    static int Get(lua_State* L);
    static int Post(lua_State* L);
    static int Put(lua_State* L);
    static int Patch(lua_State* L);
    static int Delete(lua_State* L);
    static int Request(lua_State* L);
    static int IsAvailable(lua_State* L);
    static int GetMissingDependencyMessage(lua_State* L);

    // Userdata GC handlers
    static int Request_Gc(lua_State* L);
    static int Response_Gc(lua_State* L);
    static int Handle_Gc(lua_State* L);

    // Request:* methods
    static int Request_Header(lua_State* L);
    static int Request_Body(lua_State* L);
    static int Request_Timeout(lua_State* L);
    static int Request_VerifySsl(lua_State* L);
    static int Request_Send(lua_State* L);

    // Response:* methods
    static int Response_IsSuccess(lua_State* L);
    static int Response_GetStatus(lua_State* L);
    static int Response_GetError(lua_State* L);
    static int Response_GetBody(lua_State* L);
    static int Response_GetHeader(lua_State* L);
    static int Response_GetHeaders(lua_State* L);
    static int Response_GetFinalUrl(lua_State* L);
    static int Response_GetJson(lua_State* L);
    static int Response_GetTexture(lua_State* L);
    static int Response_GetSoundWave(lua_State* L);

    // Handle:* methods
    static int Handle_Cancel(lua_State* L);
    static int Handle_IsCancelled(lua_State* L);
};

#endif
