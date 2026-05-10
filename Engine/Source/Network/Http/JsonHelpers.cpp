#include "Network/Http/JsonHelpers.h"

// rapidjson is bundled via External/Assimp/contrib/rapidjson/. The engine's
// existing include search path points at THAT directory (not its parent), so
// the headers are referenced bare — matches the convention used by
// Editor/AutoUpdater/AutoUpdater.cpp. If/when rapidjson is vendored at a
// top-level External/rapidjson/ with a nested rapidjson/ subdirectory,
// switch these to the standard <rapidjson/foo.h> form and update the
// include path accordingly.
#include "document.h"
#include "error/en.h"
#include "stringbuffer.h"
#include "writer.h"

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}

bool ParseJsonBytes(const uint8_t* data, size_t size,
                    rapidjson::Document& outDoc,
                    std::string& outError)
{
    if (data == nullptr || size == 0)
    {
        outError = "Empty JSON buffer";
        return false;
    }

    outDoc.Parse(reinterpret_cast<const char*>(data), size);
    if (outDoc.HasParseError())
    {
        outError = rapidjson::GetParseError_En(outDoc.GetParseError());
        return false;
    }
    return true;
}

void RapidJsonValueToLua(lua_State* L, const rapidjson::Value& v)
{
    if (v.IsNull())
    {
        lua_pushnil(L);
    }
    else if (v.IsBool())
    {
        lua_pushboolean(L, v.GetBool() ? 1 : 0);
    }
    else if (v.IsInt())
    {
        lua_pushinteger(L, (lua_Integer)v.GetInt());
    }
    else if (v.IsInt64())
    {
        lua_pushinteger(L, (lua_Integer)v.GetInt64());
    }
    else if (v.IsUint())
    {
        lua_pushinteger(L, (lua_Integer)v.GetUint());
    }
    else if (v.IsUint64())
    {
        lua_pushinteger(L, (lua_Integer)v.GetUint64());
    }
    else if (v.IsDouble())
    {
        // Polyphase builds Lua with LUA_32BITS for console parity, which sets
        // lua_Number to float. Cast explicitly to avoid /WX double→float
        // narrowing warnings under MSVC.
        lua_pushnumber(L, (lua_Number)v.GetDouble());
    }
    else if (v.IsString())
    {
        lua_pushlstring(L, v.GetString(), v.GetStringLength());
    }
    else if (v.IsArray())
    {
        lua_createtable(L, (int)v.Size(), 0);
        int idx = 1;
        for (auto it = v.Begin(); it != v.End(); ++it, ++idx)
        {
            RapidJsonValueToLua(L, *it);
            lua_rawseti(L, -2, idx);
        }
    }
    else if (v.IsObject())
    {
        lua_createtable(L, 0, (int)v.MemberCount());
        for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it)
        {
            lua_pushlstring(L, it->name.GetString(), it->name.GetStringLength());
            RapidJsonValueToLua(L, it->value);
            lua_rawset(L, -3);
        }
    }
    else
    {
        lua_pushnil(L);
    }
}

namespace
{
    // Treat a Lua table as an array iff its keys are 1..n contiguous integers.
    bool IsLuaTableArray(lua_State* L, int idx, int& outLen)
    {
        outLen = 0;
        const int top = lua_gettop(L);
        const int abs = idx > 0 ? idx : top + idx + 1;

        // # operator + manual contiguity check. lua_rawlen replaced lua_objlen
        // in Lua 5.2; Polyphase ships 5.4.
        const int n = (int)lua_rawlen(L, abs);
        if (n == 0)
        {
            // Empty table → treat as array (encodes as [])
            outLen = 0;
            return true;
        }

        // Verify keys 1..n exist and there are no extras.
        for (int i = 1; i <= n; ++i)
        {
            lua_rawgeti(L, abs, i);
            const bool present = !lua_isnil(L, -1);
            lua_pop(L, 1);
            if (!present) return false;
        }

        // Are there any non-integer or out-of-range keys?
        lua_pushnil(L);
        while (lua_next(L, abs) != 0)
        {
            const int keyType = lua_type(L, -2);
            if (keyType != LUA_TNUMBER) { lua_pop(L, 2); return false; }
            const lua_Number k = lua_tonumber(L, -2);
            if (k < (lua_Number)1 || k > (lua_Number)n || (lua_Number)(int)k != k)
            {
                lua_pop(L, 2);
                return false;
            }
            lua_pop(L, 1);
        }

        outLen = n;
        return true;
    }
}

bool LuaToRapidJsonValue(lua_State* L, int idx,
                         rapidjson::Value& outVal,
                         rapidjson::Document& doc)
{
    auto& alloc = doc.GetAllocator();
    const int absIdx = idx > 0 ? idx : lua_gettop(L) + idx + 1;
    const int t = lua_type(L, absIdx);

    switch (t)
    {
    case LUA_TNIL:
        outVal.SetNull();
        return true;
    case LUA_TBOOLEAN:
        outVal.SetBool(lua_toboolean(L, absIdx) != 0);
        return true;
    case LUA_TNUMBER:
    {
        // lua_tonumber returns lua_Number — `float` on Polyphase's LUA_32BITS
        // build. Widen to double for the integer-vs-double round-trip test.
        const double n = (double)lua_tonumber(L, absIdx);
        const long long ll = (long long)n;
        if ((double)ll == n) outVal.SetInt64(ll);
        else                 outVal.SetDouble(n);
        return true;
    }
    case LUA_TSTRING:
    {
        size_t len = 0;
        const char* s = lua_tolstring(L, absIdx, &len);
        outVal.SetString(s, (rapidjson::SizeType)len, alloc);
        return true;
    }
    case LUA_TTABLE:
    {
        int n = 0;
        if (IsLuaTableArray(L, absIdx, n))
        {
            outVal.SetArray();
            outVal.Reserve((rapidjson::SizeType)n, alloc);
            for (int i = 1; i <= n; ++i)
            {
                lua_rawgeti(L, absIdx, i);
                rapidjson::Value child;
                if (!LuaToRapidJsonValue(L, -1, child, doc)) { lua_pop(L, 1); return false; }
                outVal.PushBack(child, alloc);
                lua_pop(L, 1);
            }
        }
        else
        {
            outVal.SetObject();
            lua_pushnil(L);
            while (lua_next(L, absIdx) != 0)
            {
                if (lua_type(L, -2) != LUA_TSTRING)
                {
                    // Skip non-string keys silently (consistent with how most
                    // Lua → JSON serialisers behave).
                    lua_pop(L, 1);
                    continue;
                }
                size_t klen = 0;
                const char* k = lua_tolstring(L, -2, &klen);
                rapidjson::Value child;
                if (!LuaToRapidJsonValue(L, -1, child, doc)) { lua_pop(L, 2); return false; }
                rapidjson::Value key;
                key.SetString(k, (rapidjson::SizeType)klen, alloc);
                outVal.AddMember(key, child, alloc);
                lua_pop(L, 1);
            }
        }
        return true;
    }
    default:
        outVal.SetNull();
        return false;
    }
}
