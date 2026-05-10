#include "LuaBindings/Http_Lua.h"

#if LUA_ENABLED

#include "LuaBindings/LuaUtils.h"
#include "LuaBindings/Texture_Lua.h"
#include "LuaBindings/SoundWave_Lua.h"
#include "LuaBindings/Stream_Lua.h"
#include "Network/Http/HttpClient.h"
#include "Network/Http/JsonHelpers.h"
#include "Assets/Texture.h"
#include "Assets/SoundWave.h"
#include "Stream.h"
#include "Log.h"
#include "Engine.h"

#include <memory>

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}

// See JsonHelpers.cpp for why this is "document.h" not <rapidjson/document.h>.
#include "document.h"

// Userdata metatable names.
static const char* kRequestMt  = "Polyphase.HttpRequest";
static const char* kResponseMt = "Polyphase.HttpResponse";
static const char* kHandleMt   = "Polyphase.HttpHandle";

namespace
{
    HttpRequest* CheckRequest(lua_State* L, int idx)
    {
        return *static_cast<HttpRequest**>(luaL_checkudata(L, idx, kRequestMt));
    }
    std::shared_ptr<HttpResponse>* CheckResponse(lua_State* L, int idx)
    {
        return static_cast<std::shared_ptr<HttpResponse>*>(luaL_checkudata(L, idx, kResponseMt));
    }
    HttpHandle* CheckHandle(lua_State* L, int idx)
    {
        return static_cast<HttpHandle*>(luaL_checkudata(L, idx, kHandleMt));
    }

    // Push a fresh HttpRequest userdata on the stack.
    HttpRequest* PushRequest(lua_State* L, HttpVerb verb, const char* url)
    {
        HttpRequest** ud = static_cast<HttpRequest**>(lua_newuserdata(L, sizeof(HttpRequest*)));
        *ud = new HttpRequest(verb, url != nullptr ? std::string(url) : std::string());
        luaL_setmetatable(L, kRequestMt);
        return *ud;
    }

    void PushResponse(lua_State* L, std::shared_ptr<HttpResponse> resp)
    {
        void* mem = lua_newuserdata(L, sizeof(std::shared_ptr<HttpResponse>));
        new (mem) std::shared_ptr<HttpResponse>(std::move(resp));
        luaL_setmetatable(L, kResponseMt);
    }

    void PushHandle(lua_State* L, HttpHandle h)
    {
        void* mem = lua_newuserdata(L, sizeof(HttpHandle));
        new (mem) HttpHandle(std::move(h));
        luaL_setmetatable(L, kHandleMt);
    }

    // Wraps a Lua function ref so it can be called later from the main thread.
    // Holds a ref to LUA_REGISTRYINDEX; releases it on destruction (which must
    // run on the main thread).
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

    HttpResponseCallback BuildLuaCallback(lua_State* L, int callbackIdx)
    {
        if (callbackIdx == 0 || lua_isnoneornil(L, callbackIdx)) return {};
        if (!lua_isfunction(L, callbackIdx))
        {
            luaL_error(L, "Http callback must be a function");
            return {};
        }
        auto cbRef = std::make_shared<LuaCallbackRef>(L, callbackIdx);
        return [cbRef](const HttpResponse& r)
        {
            // Always invoked on the main thread by Http::Tick — safe to touch Lua.
            lua_State* L = cbRef->L;
            if (L == nullptr || cbRef->ref == LUA_REFNIL) return;

            lua_rawgeti(L, LUA_REGISTRYINDEX, cbRef->ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }

            PushResponse(L, std::make_shared<HttpResponse>(r));
            const int rc = lua_pcall(L, 1, 0, 0);
            if (rc != 0)
            {
                const char* err = lua_tostring(L, -1);
                LogError("Http callback error: %s", err != nullptr ? err : "(unknown)");
                lua_pop(L, 1);
            }
        };
    }

    HttpVerb VerbFromLua(lua_State* L, int idx)
    {
        const char* s = luaL_checkstring(L, idx);
        return HttpVerbFromString(s);
    }
}

// -- Http.* ----------------------------------------------------------------

int Http_Lua::Get(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);
    HttpResponseCallback cb = BuildLuaCallback(L, lua_isnoneornil(L, 2) ? 0 : 2);
    HttpHandle h = Http::Get(url, std::move(cb));
    PushHandle(L, std::move(h));
    return 1;
}

int Http_Lua::Post(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);
    size_t len = 0;
    const char* body = lua_isnoneornil(L, 2) ? "" : luaL_checklstring(L, 2, &len);
    std::vector<uint8_t> bodyBytes(body, body + len);
    HttpResponseCallback cb = BuildLuaCallback(L, lua_isnoneornil(L, 3) ? 0 : 3);
    HttpHandle h = Http::Post(url, std::move(bodyBytes), std::move(cb));
    PushHandle(L, std::move(h));
    return 1;
}

int Http_Lua::Put(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);
    size_t len = 0;
    const char* body = lua_isnoneornil(L, 2) ? "" : luaL_checklstring(L, 2, &len);
    std::vector<uint8_t> bodyBytes(body, body + len);
    HttpResponseCallback cb = BuildLuaCallback(L, lua_isnoneornil(L, 3) ? 0 : 3);
    HttpHandle h = Http::Put(url, std::move(bodyBytes), std::move(cb));
    PushHandle(L, std::move(h));
    return 1;
}

int Http_Lua::Patch(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);
    size_t len = 0;
    const char* body = lua_isnoneornil(L, 2) ? "" : luaL_checklstring(L, 2, &len);
    std::vector<uint8_t> bodyBytes(body, body + len);
    HttpResponseCallback cb = BuildLuaCallback(L, lua_isnoneornil(L, 3) ? 0 : 3);
    HttpHandle h = Http::Patch(url, std::move(bodyBytes), std::move(cb));
    PushHandle(L, std::move(h));
    return 1;
}

int Http_Lua::Delete(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);
    HttpResponseCallback cb = BuildLuaCallback(L, lua_isnoneornil(L, 2) ? 0 : 2);
    HttpHandle h = Http::Delete(url, std::move(cb));
    PushHandle(L, std::move(h));
    return 1;
}

int Http_Lua::Request(lua_State* L)
{
    const HttpVerb verb = VerbFromLua(L, 1);
    const char* url = luaL_checkstring(L, 2);
    PushRequest(L, verb, url);
    return 1;
}

int Http_Lua::IsAvailable(lua_State* L)
{
    lua_pushboolean(L, Http::IsAvailable() ? 1 : 0);
    return 1;
}

int Http_Lua::GetMissingDependencyMessage(lua_State* L)
{
    lua_pushstring(L, Http::GetMissingDependencyMessage());
    return 1;
}

// -- Request:* -------------------------------------------------------------

int Http_Lua::Request_Header(lua_State* L)
{
    HttpRequest* r = CheckRequest(L, 1);
    const char* k = luaL_checkstring(L, 2);
    const char* v = luaL_checkstring(L, 3);
    r->Header(k, v);
    lua_settop(L, 1);   // return self for chaining
    return 1;
}

int Http_Lua::Request_Body(lua_State* L)
{
    HttpRequest* r = CheckRequest(L, 1);
    size_t len = 0;
    const char* body = luaL_checklstring(L, 2, &len);
    r->Body(std::vector<uint8_t>(body, body + len));
    lua_settop(L, 1);
    return 1;
}

int Http_Lua::Request_Timeout(lua_State* L)
{
    HttpRequest* r = CheckRequest(L, 1);
    r->TimeoutMs((int32_t)luaL_checkinteger(L, 2));
    lua_settop(L, 1);
    return 1;
}

int Http_Lua::Request_VerifySsl(lua_State* L)
{
    HttpRequest* r = CheckRequest(L, 1);
    r->VerifySsl(lua_toboolean(L, 2) != 0);
    lua_settop(L, 1);
    return 1;
}

int Http_Lua::Request_Send(lua_State* L)
{
    HttpRequest* r = CheckRequest(L, 1);
    HttpResponseCallback cb = BuildLuaCallback(L, lua_isnoneornil(L, 2) ? 0 : 2);
    HttpHandle h = Http::Send(*r, std::move(cb));
    PushHandle(L, std::move(h));
    return 1;
}

int Http_Lua::Request_Gc(lua_State* L)
{
    HttpRequest** ud = static_cast<HttpRequest**>(luaL_checkudata(L, 1, kRequestMt));
    if (*ud != nullptr) { delete *ud; *ud = nullptr; }
    return 0;
}

// -- Response:* ------------------------------------------------------------

int Http_Lua::Response_IsSuccess(lua_State* L)
{
    auto* r = CheckResponse(L, 1);
    lua_pushboolean(L, (*r)->IsSuccess() ? 1 : 0);
    return 1;
}

int Http_Lua::Response_GetStatus(lua_State* L)
{
    auto* r = CheckResponse(L, 1);
    lua_pushinteger(L, (*r)->GetStatus());
    return 1;
}

int Http_Lua::Response_GetError(lua_State* L)
{
    auto* r = CheckResponse(L, 1);
    if ((*r)->GetError() == HttpError::None)
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, (*r)->GetErrorMessage().empty()
            ? HttpErrorToString((*r)->GetError())
            : (*r)->GetErrorMessage().c_str());
    }
    return 1;
}

int Http_Lua::Response_GetBody(lua_State* L)
{
    auto* r = CheckResponse(L, 1);
    const auto& body = (*r)->GetBody();
    lua_pushlstring(L, reinterpret_cast<const char*>(body.data()), body.size());
    return 1;
}

int Http_Lua::Response_GetHeader(lua_State* L)
{
    auto* r = CheckResponse(L, 1);
    const char* name = luaL_checkstring(L, 2);
    const std::string& value = (*r)->GetHeader(name);
    if (value.empty() && !(*r)->HasHeader(name)) lua_pushnil(L);
    else                                          lua_pushstring(L, value.c_str());
    return 1;
}

int Http_Lua::Response_GetHeaders(lua_State* L)
{
    auto* r = CheckResponse(L, 1);
    const auto& hdrs = (*r)->GetHeaders();
    lua_createtable(L, 0, (int)hdrs.size());
    for (const auto& kv : hdrs)
    {
        lua_pushlstring(L, kv.first.c_str(), kv.first.size());
        lua_pushlstring(L, kv.second.c_str(), kv.second.size());
        lua_rawset(L, -3);
    }
    return 1;
}

int Http_Lua::Response_GetFinalUrl(lua_State* L)
{
    auto* r = CheckResponse(L, 1);
    lua_pushstring(L, (*r)->GetFinalUrl().c_str());
    return 1;
}

int Http_Lua::Response_GetJson(lua_State* L)
{
    auto* r = CheckResponse(L, 1);
    const auto& body = (*r)->GetBody();
    rapidjson::Document doc;
    std::string err;
    if (!ParseJsonBytes(body.data(), body.size(), doc, err))
    {
        lua_pushnil(L);
        lua_pushstring(L, err.c_str());
        return 2;
    }
    RapidJsonValueToLua(L, doc);
    return 1;
}

int Http_Lua::Response_GetTexture(lua_State* L)
{
    auto* r = CheckResponse(L, 1);
    Texture* t = (*r)->GetTexture();
    if (t == nullptr) { lua_pushnil(L); return 1; }
    Asset_Lua::Create(L, t);
    return 1;
}

int Http_Lua::Response_GetSoundWave(lua_State* L)
{
    auto* r = CheckResponse(L, 1);
    SoundWave* s = (*r)->GetSoundWave();
    if (s == nullptr) { lua_pushnil(L); return 1; }
    Asset_Lua::Create(L, s);
    return 1;
}

int Http_Lua::Response_Gc(lua_State* L)
{
    auto* r = static_cast<std::shared_ptr<HttpResponse>*>(luaL_checkudata(L, 1, kResponseMt));
    using Sp = std::shared_ptr<HttpResponse>;
    r->~Sp();
    return 0;
}

// -- Handle:* --------------------------------------------------------------

int Http_Lua::Handle_Cancel(lua_State* L)
{
    HttpHandle* h = CheckHandle(L, 1);
    h->Cancel();
    return 0;
}

int Http_Lua::Handle_IsCancelled(lua_State* L)
{
    HttpHandle* h = CheckHandle(L, 1);
    lua_pushboolean(L, h->IsCancelled() ? 1 : 0);
    return 1;
}

int Http_Lua::Handle_Gc(lua_State* L)
{
    HttpHandle* h = static_cast<HttpHandle*>(luaL_checkudata(L, 1, kHandleMt));
    h->~HttpHandle();
    return 0;
}

// -- Bind ------------------------------------------------------------------

void Http_Lua::Bind()
{
    lua_State* L = GetLua();

    // ---- Request metatable ----
    luaL_newmetatable(L, kRequestMt);
    int mtRequest = lua_gettop(L);

    lua_pushvalue(L, mtRequest);
    lua_setfield(L, mtRequest, "__index");

    REGISTER_TABLE_FUNC_EX(L, mtRequest, Http_Lua::Request_Header,    "Header");
    REGISTER_TABLE_FUNC_EX(L, mtRequest, Http_Lua::Request_Body,      "Body");
    REGISTER_TABLE_FUNC_EX(L, mtRequest, Http_Lua::Request_Timeout,   "Timeout");
    REGISTER_TABLE_FUNC_EX(L, mtRequest, Http_Lua::Request_VerifySsl, "VerifySsl");
    REGISTER_TABLE_FUNC_EX(L, mtRequest, Http_Lua::Request_Send,      "Send");
    REGISTER_TABLE_FUNC_EX(L, mtRequest, Http_Lua::Request_Gc,        "__gc");

    lua_pop(L, 1);

    // ---- Response metatable ----
    luaL_newmetatable(L, kResponseMt);
    int mtResponse = lua_gettop(L);

    lua_pushvalue(L, mtResponse);
    lua_setfield(L, mtResponse, "__index");

    REGISTER_TABLE_FUNC_EX(L, mtResponse, Http_Lua::Response_IsSuccess,    "IsSuccess");
    REGISTER_TABLE_FUNC_EX(L, mtResponse, Http_Lua::Response_GetStatus,    "GetStatus");
    REGISTER_TABLE_FUNC_EX(L, mtResponse, Http_Lua::Response_GetError,     "GetError");
    REGISTER_TABLE_FUNC_EX(L, mtResponse, Http_Lua::Response_GetBody,      "GetBody");
    REGISTER_TABLE_FUNC_EX(L, mtResponse, Http_Lua::Response_GetHeader,    "GetHeader");
    REGISTER_TABLE_FUNC_EX(L, mtResponse, Http_Lua::Response_GetHeaders,   "GetHeaders");
    REGISTER_TABLE_FUNC_EX(L, mtResponse, Http_Lua::Response_GetFinalUrl,  "GetFinalUrl");
    REGISTER_TABLE_FUNC_EX(L, mtResponse, Http_Lua::Response_GetJson,      "GetJson");
    REGISTER_TABLE_FUNC_EX(L, mtResponse, Http_Lua::Response_GetTexture,   "GetTexture");
    REGISTER_TABLE_FUNC_EX(L, mtResponse, Http_Lua::Response_GetSoundWave, "GetSoundWave");
    REGISTER_TABLE_FUNC_EX(L, mtResponse, Http_Lua::Response_Gc,           "__gc");

    lua_pop(L, 1);

    // ---- Handle metatable ----
    luaL_newmetatable(L, kHandleMt);
    int mtHandle = lua_gettop(L);

    lua_pushvalue(L, mtHandle);
    lua_setfield(L, mtHandle, "__index");

    REGISTER_TABLE_FUNC_EX(L, mtHandle, Http_Lua::Handle_Cancel,      "Cancel");
    REGISTER_TABLE_FUNC_EX(L, mtHandle, Http_Lua::Handle_IsCancelled, "IsCancelled");
    REGISTER_TABLE_FUNC_EX(L, mtHandle, Http_Lua::Handle_Gc,          "__gc");

    lua_pop(L, 1);

    // ---- Http table ----
    lua_newtable(L);
    int tHttp = lua_gettop(L);

    REGISTER_TABLE_FUNC_EX(L, tHttp, Http_Lua::Get,                          "Get");
    REGISTER_TABLE_FUNC_EX(L, tHttp, Http_Lua::Post,                         "Post");
    REGISTER_TABLE_FUNC_EX(L, tHttp, Http_Lua::Put,                          "Put");
    REGISTER_TABLE_FUNC_EX(L, tHttp, Http_Lua::Patch,                        "Patch");
    REGISTER_TABLE_FUNC_EX(L, tHttp, Http_Lua::Delete,                       "Delete");
    REGISTER_TABLE_FUNC_EX(L, tHttp, Http_Lua::Request,                      "Request");
    REGISTER_TABLE_FUNC_EX(L, tHttp, Http_Lua::IsAvailable,                  "IsAvailable");
    REGISTER_TABLE_FUNC_EX(L, tHttp, Http_Lua::GetMissingDependencyMessage,  "GetMissingDependencyMessage");

    lua_setglobal(L, "Http");

    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
