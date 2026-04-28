#pragma once

#include "EngineTypes.h"

#if LUA_ENABLED

#define EASING_LUA_NAME "Easing"

struct Easing_Lua
{
    static void Bind();
};

#endif
