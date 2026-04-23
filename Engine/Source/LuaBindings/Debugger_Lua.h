#pragma once

#include "EngineTypes.h"

#if LUA_ENABLED

#define DEBUGGER_LUA_NAME "Debugger"

// Script-side bindings for the in-engine Lua debugger.
//
// These are registered in BOTH editor and shipping builds so script source
// stays identical between dev and packaged games. In shipping (non-EDITOR)
// builds the implementations are silent no-ops and IsAttached returns false.
struct Debugger_Lua
{
    // Debugger.Break([message])
    //   Editor:  pauses execution at the call site, opens the debugger panel.
    //   Shipping: silent no-op.
    static int Break(lua_State* L);

    // Debugger.IsAttached() -> bool
    //   Editor:  always true.
    //   Shipping: always false.
    static int IsAttached(lua_State* L);

    static void Bind();
};

#endif
