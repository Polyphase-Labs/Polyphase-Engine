#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: lua <script> [args...]\n");
        return 2;
    }
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_newtable(L);
    for (int i = 1; i < argc; ++i)
    {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i - 1);
    }
    lua_setglobal(L, "arg");
    if (luaL_dofile(L, argv[1]) != LUA_OK)
    {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        return 1;
    }
    lua_close(L);
    return 0;
}
