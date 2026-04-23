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
    //   Editor:  HARD abort -- aborts the surrounding pcall via lua_error and
    //            freezes the world. The current Lua call stops at this line;
    //            the aborted function does not re-run after Continue. Match
    //            for pdb.set_trace / `debugger;`. Use this for general
    //            "stop here so I can inspect" debugging.
    //   Shipping: silent no-op.
    static int Break(lua_State* L);

    // Debugger.Snapshot([message])
    //   Editor:  SOFT pause -- captures snapshot + freezes the world from the
    //            next frame, but the current Lua call continues to its
    //            natural end. Use this when you want to inspect state inside
    //            a one-shot init function (Awake / Start / SpawnScene flow)
    //            and need the rest of the function to complete -- otherwise
    //            scenes spawned by that function never finish initializing.
    //   Shipping: silent no-op.
    static int Snapshot(lua_State* L);

    // Debugger.IsAttached() -> bool
    //   Editor:  always true.
    //   Shipping: always false.
    static int IsAttached(lua_State* L);

    static void Bind();
};

#endif
