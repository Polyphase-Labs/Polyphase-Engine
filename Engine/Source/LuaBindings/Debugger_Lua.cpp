#include "Engine.h"
#include "Log.h"

#include "LuaBindings/LuaUtils.h"
#include "LuaBindings/Debugger_Lua.h"

#if EDITOR
#include "LuaDebugger/LuaDebugger.h"
#endif

#if LUA_ENABLED

int Debugger_Lua::Break(lua_State* L)
{
#if EDITOR
    // Note: deliberately NOT gated on IsInstalled(). Debugger.Break works as
    // a direct pause-via-lua_error; it doesn't need the line hook to be live.
    // This is what lets the script-side API still work even if our line hook
    // got replaced by something else (LuaPanda, addon, etc.).
    LuaDebugger* dbg = LuaDebugger::Get();
    const char* msg = (lua_gettop(L) >= 1 && lua_isstring(L, 1)) ? lua_tostring(L, 1) : "(no msg)";
    LogDebug("Debugger.Break called from script: %s (dbg=%p)", msg, (void*)dbg);
    if (dbg != nullptr)
    {
        return LuaDebugger::LuaBreakBinding(L); // longjmps out, never returns
    }
#endif
    (void)L;
    return 0;
}

int Debugger_Lua::Snapshot(lua_State* L)
{
#if EDITOR
    LuaDebugger* dbg = LuaDebugger::Get();
    const char* msg = (lua_gettop(L) >= 1 && lua_isstring(L, 1)) ? lua_tostring(L, 1) : "(no msg)";
    LogDebug("Debugger.Snapshot called from script: %s (dbg=%p)", msg, (void*)dbg);
    if (dbg != nullptr)
    {
        return LuaDebugger::LuaSnapshotBinding(L); // returns normally, no abort
    }
#endif
    (void)L;
    return 0;
}

int Debugger_Lua::IsAttached(lua_State* L)
{
#if EDITOR
    LuaDebugger* dbg = LuaDebugger::Get();
    bool attached = (dbg != nullptr && dbg->IsInstalled());
    lua_pushboolean(L, attached ? 1 : 0);
#else
    lua_pushboolean(L, 0);
#endif
    return 1;
}

void Debugger_Lua::Bind()
{
    lua_State* L = GetLua();

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    REGISTER_TABLE_FUNC(L, tableIdx, Break);
    REGISTER_TABLE_FUNC(L, tableIdx, Snapshot);
    REGISTER_TABLE_FUNC(L, tableIdx, IsAttached);

    lua_setglobal(L, DEBUGGER_LUA_NAME);

    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
