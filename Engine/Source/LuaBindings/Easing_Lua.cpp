#include "LuaBindings/Easing_Lua.h"
#include "LuaBindings/LuaUtils.h"
#include "Easing.h"

#if LUA_ENABLED

void Easing_Lua::Bind()
{
    lua_State* L = GetLua();

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    // Linear (special case, handled in TweenManager)
    lua_pushstring(L, "Linear");
    lua_pushinteger(L, EaseLinear);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InSine");
    lua_pushinteger(L, EaseInSine);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "OutSine");
    lua_pushinteger(L, EaseOutSine);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InOutSine");
    lua_pushinteger(L, EaseInOutSine);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InQuad");
    lua_pushinteger(L, EaseInQuad);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "OutQuad");
    lua_pushinteger(L, EaseOutQuad);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InOutQuad");
    lua_pushinteger(L, EaseInOutQuad);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InCubic");
    lua_pushinteger(L, EaseInCubic);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "OutCubic");
    lua_pushinteger(L, EaseOutCubic);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InOutCubic");
    lua_pushinteger(L, EaseInOutCubic);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InQuart");
    lua_pushinteger(L, EaseInQuart);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "OutQuart");
    lua_pushinteger(L, EaseOutQuart);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InOutQuart");
    lua_pushinteger(L, EaseInOutQuart);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InQuint");
    lua_pushinteger(L, EaseInQuint);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "OutQuint");
    lua_pushinteger(L, EaseOutQuint);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InOutQuint");
    lua_pushinteger(L, EaseInOutQuint);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InExpo");
    lua_pushinteger(L, EaseInExpo);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "OutExpo");
    lua_pushinteger(L, EaseOutExpo);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InOutExpo");
    lua_pushinteger(L, EaseInOutExpo);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InCirc");
    lua_pushinteger(L, EaseInCirc);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "OutCirc");
    lua_pushinteger(L, EaseOutCirc);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InOutCirc");
    lua_pushinteger(L, EaseInOutCirc);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InBack");
    lua_pushinteger(L, EaseInBack);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "OutBack");
    lua_pushinteger(L, EaseOutBack);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InOutBack");
    lua_pushinteger(L, EaseInOutBack);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InElastic");
    lua_pushinteger(L, EaseInElastic);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "OutElastic");
    lua_pushinteger(L, EaseOutElastic);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InOutElastic");
    lua_pushinteger(L, EaseInOutElastic);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InBounce");
    lua_pushinteger(L, EaseInBounce);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "OutBounce");
    lua_pushinteger(L, EaseOutBounce);
    lua_rawset(L, tableIdx);

    lua_pushstring(L, "InOutBounce");
    lua_pushinteger(L, EaseInOutBounce);
    lua_rawset(L, tableIdx);

    lua_setglobal(L, EASING_LUA_NAME);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
